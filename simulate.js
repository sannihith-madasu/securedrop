#!/usr/bin/env node
// Command-line stand-in for the ESP32-CAM. Publishes the same two MQTT
// messages as the firmware.   Usage: node simulate.js [photo.jpg] [broker-host]
const fs = require("fs");
const path = require("path");
const mqtt = require("mqtt");

const file = process.argv[2] || path.join(__dirname, "sample", "parcel.jpg");
const host = process.argv[3] || "localhost";
const img = fs.readFileSync(file);
const client = mqtt.connect("mqtt://" + host + ":1883", { clientId: "securedrop-sim-" + process.pid, connectTimeout: 5000 });

client.on("connect", () => {
  const event = { device: "securedrop-sim", event: "delivery", wake: "cli", rssi: -52, bytes: img.length };
  client.publish("securedrop/event", JSON.stringify(event), { qos: 1 }, () => {
    client.publish("securedrop/image", img, { qos: 1 }, () => {
      console.log("published event + " + img.length + " byte image from " + path.basename(file));
      client.end();
    });
  });
});
client.on("error", (e) => { console.error("MQTT error: " + e.message + "  (is the gateway running?)"); process.exit(1); });
