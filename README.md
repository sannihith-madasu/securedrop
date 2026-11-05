# SecureDrop gateway

Node-RED gateway for SecureDrop, the self-verifying IoT parcel box.
It runs the MQTT broker, archives every delivery photo, sends it on Telegram and updates Blynk.

## Run it

Needs Node.js 22.9 or newer (https://nodejs.org, LTS installer). Check with `node -v`.

    npm install
    npm start

| What | Where |
|---|---|
| Flow editor | http://localhost:1880 |
| Delivery log | http://localhost:1880/securedrop |
| Edge-node simulator (webcam) | http://localhost:1880/securedrop/sim |
| Archived photos | `archive/` folder |

With no tokens set, the gateway still archives photos and shows them on the delivery log.

## Demo without hardware

1. `npm start`, open the delivery log in one window and the simulator in another.
2. Press **Simulate PIR trigger**. The page takes a webcam frame and publishes the same two
   MQTT messages the ESP32-CAM publishes. The photo appears on the log within a second.
3. In the flow editor, open the debug sidebar to show the event JSON and notification results.

`npm run simulate` (or `node simulate.js photo.jpg`) does the same from the command line.

## Telegram

1. In Telegram, message **@BotFather**, send `/newbot`, copy the token.
2. Send any message to your new bot.
3. Open `https://api.telegram.org/bot<TOKEN>/getUpdates` and copy `chat.id`.
4. In the flow editor, open the **CONFIG** node, paste both values, press **Deploy**.

## Blynk

1. On blynk.cloud create a template with two datastreams: `V0` Integer (delivery count, raise the max)
   and `V1` String (last delivery time). Add a device from it.
2. Copy the device **Auth Token** into the **CONFIG** node, press **Deploy**.
3. Put a Value Display / Label widget on each datastream in the dashboard.

Do not upload `flows.json` to GitHub with real tokens in it.

## MQTT contract

| Topic | Payload |
|---|---|
| `securedrop/event` | JSON: `{"device":"securedrop-01","event":"delivery","wake":"pir","rssi":-55,"bytes":23110,"boot":7}` |
| `securedrop/image` | raw JPEG bytes, published right after the event |

## Real hardware

`firmware/SecureDrop/SecureDrop.ino` is the ESP32-CAM sketch (Arduino IDE, board "AI Thinker ESP32-CAM",
library PubSubClient). Set Wi-Fi name, password and the gateway PC's IP at the top.
Wiring: PIR VCC to 5V, GND to GND, OUT to GPIO 13.
The PC firewall must allow inbound TCP 1883, and both devices must be on the same Wi-Fi.
