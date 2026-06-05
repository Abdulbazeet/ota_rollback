
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_ota_ops.h"
#include <ESPmDNS.h>
#include <Preferences.h>
#include "mbedtls/sha256.h"

Preferences prefs;

const char *ssid = "Tony Stank";
const char *password = "Ba1za22ta@.com#";

#define CURRENT_VERSION 2

// const char *serverBaseUrl = "http://10.16.190.150:5000";
// const char *manifestUrl = "http://10.16.190.150:5000/manifest.json";
const char *serverBaseUrl = "http://olatunji.local:5000";
const char *manifestUrl = "http://olatunji.local:5000/manifest.json";

// ---------------- STATE FLAGS ----------------
bool otaChecked = false;
bool wifiReady = false;

String bytesToHex(uint8_t *hash, size_t len)
{
  String result;

  for (size_t i = 0; i < len; i++)
  {
    char buf[3];

    sprintf(buf, "%02x", hash[i]);

    result += buf;
  }

  return result;
}

void runOTA(
    String firmwareUrl,
    int serverVersion,
    String expectedHash)
{
  Serial.println("===== OTA START =====");

  HTTPClient http;

  http.begin(firmwareUrl);

  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK)
  {
    Serial.print("Firmware download failed: ");
    Serial.println(httpCode);
    http.end();
    return;
  }

  int contentLength = http.getSize();

  Serial.print("Firmware Size: ");
  Serial.println(contentLength);

  const esp_partition_t *updatePartition =
      esp_ota_get_next_update_partition(NULL);

  if (!updatePartition)
  {
    Serial.println("No OTA partition found");
    http.end();
    return;
  }

  Serial.print("Writing to partition: ");
  Serial.println(updatePartition->label);

  esp_ota_handle_t otaHandle;

  if (esp_ota_begin(
          updatePartition,
          OTA_SIZE_UNKNOWN,
          &otaHandle) != ESP_OK)
  {
    Serial.println("esp_ota_begin failed");
    http.end();
    return;
  }

  WiFiClient *stream = http.getStreamPtr();

  uint8_t buffer[1024];

  mbedtls_sha256_context shaCtx;

  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, 0);

  int totalWritten = 0;

  while (http.connected() &&
         (contentLength > 0 || contentLength == -1))
  {
    size_t available = stream->available();

    if (available)
    {
      int readBytes =
          stream->readBytes(
              buffer,
              min((size_t)1024, available));

      esp_err_t writeResult =
          esp_ota_write(
              otaHandle,
              buffer,
              readBytes);

      if (writeResult != ESP_OK)
      {
        Serial.println("esp_ota_write failed");

        esp_ota_abort(otaHandle);

        http.end();

        return;
      }

      mbedtls_sha256_update(
          &shaCtx,
          buffer,
          readBytes);

      totalWritten += readBytes;

      if (contentLength > 0)
      {
        contentLength -= readBytes;
      }
    }

    delay(1);
  }

  uint8_t hash[32];

  mbedtls_sha256_finish(
      &shaCtx,
      hash);

  mbedtls_sha256_free(
      &shaCtx);

  Serial.print("Total Bytes Written: ");
  Serial.println(totalWritten);

  String calculatedHash =
      bytesToHex(hash, 32);

  Serial.print("Calculated SHA256: ");
  Serial.println(calculatedHash);

  Serial.print("Expected SHA256: ");
  Serial.println(expectedHash);

  if (calculatedHash != expectedHash)
  {
    Serial.println("HASH VERIFICATION FAILED");

    esp_ota_abort(otaHandle);

    http.end();

    return;
  }

  Serial.println("HASH VERIFIED");

  if (esp_ota_end(otaHandle) != ESP_OK)
  {
    Serial.println("esp_ota_end failed");

    http.end();

    return;
  }

  if (esp_ota_set_boot_partition(
          updatePartition) != ESP_OK)
  {
    Serial.println(
        "Failed to set boot partition");

    http.end();

    return;
  }
  Serial.println("OTA Success!");
  Serial.println("Rebooting...");

  http.end();

  delay(2000);

  ESP.restart();
}

// ---------------- SETUP ----------------
void setup()
{
  Serial.begin(115200);
  delay(3000);

  Serial.println("\nESP32 OTA Booting VERSION 2");
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  // }
  if (WiFi.status() == WL_CONNECTED)
  {
    wifiReady = true;

    Serial.println("WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    prefs.begin("ota", false);
    prefs.putInt("version", CURRENT_VERSION);
    prefs.end();

    // const esp_partition_t *running =
    //     esp_ota_get_running_partition();

    // Serial.print("Running partition: ");
    // Serial.println(running->label);

    const esp_partition_t *running =
        esp_ota_get_running_partition();

    const esp_partition_t *boot =
        esp_ota_get_boot_partition();

    Serial.print("Running partition: ");
    Serial.println(running->label);

    Serial.print("Boot partition: ");
    Serial.println(boot->label);

    Serial.print("Partition size: ");
    Serial.println(running->size);

    Serial.print("Sketch size: ");
    Serial.println(ESP.getSketchSize());

    Serial.print("Free sketch space: ");
    Serial.println(ESP.getFreeSketchSpace());

    prefs.begin("ota", true);

    int storedVersion =
        prefs.getInt("version", 1);

    prefs.end();

    Serial.print("Stored Version: ");
    Serial.println(storedVersion);
  }
  else
  {
    Serial.println("WiFi Failed!");
  }
}

// ---------------- LOOP (SAFE OTA EXECUTION) ----------------
void loop()
{

  Serial.println("Device Running...");
  delay(3000);

  // ---------------- SAFE OTA CHECK ----------------
  if (wifiReady && !otaChecked)
  {
    otaChecked = true;

    Serial.println("Checking manifest...");

    HTTPClient http;
    http.begin(manifestUrl);

    int httpCode = http.GET();

    if (httpCode == 200)
    {
      String payload = http.getString();

      Serial.println("Manifest received:");
      Serial.println(payload);

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (error)
      {
        Serial.println("Failed to parse manifest");
        return;
      }

      int serverVersion = doc["version"];
      String firmwareFile = doc["firmware"];
      String expectedHash = doc["sha256"];

      Serial.print("Expected SHA256: ");
      Serial.println(expectedHash);

      prefs.begin("ota", true);

      int storedVersion =
          prefs.getInt("version", CURRENT_VERSION);

      prefs.end();

      Serial.print("Stored Version: ");
      Serial.println(storedVersion);

      Serial.print("Server Version: ");
      Serial.println(serverVersion);

      if (serverVersion > storedVersion)
      {
        Serial.println("New firmware available!");

        String firmwareUrl =
            String(serverBaseUrl) + "/" + firmwareFile;

        runOTA(
            firmwareUrl,
            serverVersion, expectedHash);
      }
      else if (serverVersion == storedVersion)
      {
        Serial.println("Firmware already up to date.");
      }
      else
      {
        Serial.println("Rollback attempt detected!");
        Serial.println("Update rejected.");
      }
    }
    else
    {
      Serial.print("Manifest request failed: ");
      Serial.println(httpCode);
    }

    http.end();
  }
}