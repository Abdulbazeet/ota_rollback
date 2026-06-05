from flask import Flask, send_file
import os

app = Flask(__name__)

@app.route("/firmware.bin")
def firmware():
    return send_file(
        "firmware.bin",
        mimetype="application/octet-stream",
        as_attachment=True
    )

@app.route("/firmware.sig")
def signature():
    return send_file(
        "firmware.sig",
        mimetype="application/octet-stream"
    )

@app.route("/manifest.json")
def manifest():
    return send_file(
        "manifest.json",
        mimetype="application/json"
    )

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)