#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include "esp_ota_ops.h"
#include <ESPmDNS.h>

const char *ssid = "Tony Stank";
const char *password = "Ba1za22ta@.com#";

#define CURRENT_VERSION 1

// const char *serverBaseUrl = "http://10.16.190.150:5000";
// const char *manifestUrl = "http://10.16.190.150:5000/manifest.json";
const char *serverBaseUrl = "http://olatunji.local:5000";
const char *manifestUrl = "http://olatunji.local:5000/manifest.json";

// ---------------- STATE FLAGS ----------------
bool otaChecked = false;
bool wifiReady = false;

// ---------------- OTA FUNCTION ----------------
// void runOTA(String firmwareUrl)
// {
//   Serial.println("Starting OTA update...");

//   HTTPClient updateHttp;
//   updateHttp.begin(firmwareUrl);

//   t_httpUpdate_return ret = httpUpdate.update(updateHttp);

//   switch (ret)
//   {
//   case HTTP_UPDATE_FAILED:
//     Serial.printf(
//         "OTA Failed (%d): %s\n",
//         httpUpdate.getLastError(),
//         httpUpdate.getLastErrorString().c_str());
//     break;

//   case HTTP_UPDATE_NO_UPDATES:
//     Serial.println("No updates.");
//     break;

//   case HTTP_UPDATE_OK:
//     Serial.println("OTA Success! Rebooting...");
//     break;
//   }

//   updateHttp.end();
// }

void runOTA(String firmwareUrl)
{
  Serial.println("===== OTA START =====");

  Serial.print("Firmware URL: ");
  Serial.println(firmwareUrl);

  Serial.print("Free Heap Before OTA: ");
  Serial.println(ESP.getFreeHeap());

  HTTPClient updateHttp;

  bool ok = updateHttp.begin(firmwareUrl);

  Serial.print("HTTP Begin Result: ");
  Serial.println(ok);

  if (!ok)
  {
    Serial.println("Failed to initialize HTTP client");
    return;
  }

  Serial.println("Calling httpUpdate.update()...");

  t_httpUpdate_return ret = httpUpdate.update(updateHttp);

  Serial.println("Returned from httpUpdate.update()");

  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
    Serial.printf(
        "OTA Failed (%d): %s\n",
        httpUpdate.getLastError(),
        httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("No updates.");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("OTA Success!");
    break;
  }

  updateHttp.end();
}

// ---------------- SETUP ----------------
void setup()
{
  Serial.begin(115200);
  delay(3000);

  Serial.println("\nESP32 OTA Booting V3 TEST");

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

  if (WiFi.status() == WL_CONNECTED)
  {
    wifiReady = true;

    Serial.println("WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    /////

    const esp_partition_t *running =
        esp_ota_get_running_partition();

    Serial.print("Running partition: ");
    Serial.println(running->label);

    Serial.print("Partition size: ");
    Serial.println(running->size);

    Serial.print("Sketch size: ");
    Serial.println(ESP.getSketchSize());

    Serial.print("Free sketch space: ");
    Serial.println(ESP.getFreeSketchSpace());
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

      Serial.print("Current Version: ");
      Serial.println(CURRENT_VERSION);

      Serial.print("Server Version: ");
      Serial.println(serverVersion);

      if (serverVersion > CURRENT_VERSION)
      {
        Serial.println("New firmware available!");

        String firmwareUrl =
            String(serverBaseUrl) + "/" + firmwareFile;

        runOTA(firmwareUrl);
      }
      else
      {
        Serial.println("Firmware already up to date.");
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