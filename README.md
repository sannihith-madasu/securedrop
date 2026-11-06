<div align="center">

  <!-- Animated Typing Header -->
  <a href="https://github.com/sannihith-madasu/securedrop">
    <img src="https://readme-typing-svg.demolab.com?font=JetBrains+Mono&weight=700&size=28&duration=3000&pause=1000&color=10B981&center=true&vCenter=true&width=600&lines=SecureDrop;Edge-Triggered+Delivery+Sentinel;ESP32-CAM+%2B+Node-RED+Telemetry" alt="SecureDrop Header" />
  </a>

  <p><strong>Low-power, event-driven package verification system built on ESP32-CAM, Node-RED, and the Telegram Bot API.</strong></p>

  <p>
    <a href="#architecture">Architecture</a> •
    <a href="#repository-structure">Directory Layout</a> •
    <a href="#hardware-setup">Hardware</a> •
    <a href="#getting-started">Quick Start</a> •
    <a href="#simulation">Simulation</a>
  </p>

  <p>
    <img src="https://img.shields.io/badge/Hardware-ESP32--CAM-000000?style=flat-square&logo=espressif&logoColor=white" alt="ESP32-CAM" />
    <img src="https://img.shields.io/badge/Backend-Node--RED-8F0000?style=flat-square&logo=nodered&logoColor=white" alt="Node-RED" />
    <img src="https://img.shields.io/badge/Runtime-Node.js-339933?style=flat-square&logo=nodedotjs&logoColor=white" alt="Node.js" />
    <img src="https://img.shields.io/badge/Alerts-Telegram_API-26A5E4?style=flat-square&logo=telegram&logoColor=white" alt="Telegram" />
  </p>

</div>

---

## Overview

Unattended deliveries fail at the last mile due to zero physical verification and high power requirements for continuous video monitoring. **SecureDrop** replaces always-on CCTV streams with an event-driven edge sentinel.

The node stays in deep sleep drawing microamps until a PIR motion interrupt fires. Upon wake, it sweeps the drop zone via a PWM servo, captures multi-angle JPEG frames into PSRAM, dispatches the payload to a local Node-RED orchestration server / Telegram webhook, and returns to deep sleep.

<!-- Drop a screen recording or hardware capture gif at sample/demo.gif -->
<div align="center">
  <img src="sample/demo.gif" alt="SecureDrop System Demo" width="750px" />
</div>

## Architecture

```text
+-------------------------------------------------------------------------+
| EDGE NODE (firmware/)                                                   |
|                                                                         |
|  [PIR Interrupt] --> [Wake EXT0] --> [Servo Pan] --> [OV2640 Frame Grab] |
+---------------------------------------------------------------+---------+
                                                                |
                                                   HTTP POST / MQTT Payload
                                                                |
                                                                v
+-------------------------------------------------------------------------+
| ORCHESTRATION & TELEMETRY (flows.json / simulate.js)                    |
|                                                                         |
|  [Node-RED Ingest] --> [State Logging] --> [Telegram Bot API Dispatch]  |
+-------------------------------------------------------------------------+
```
## Repository Structure

```text
securedrop/
├── firmware/           # ESP32-CAM C/C++ source, pin definitions, and sleep logic
├── sample/             # Reference payloads, test captures, and media assets
├── flows.json          # Node-RED flow definitions for ingestion and alerting
├── settings.js         # Node-RED runtime configuration
├── simulate.js         # Mock event generator for testing flows offline
├── package.json        # Node.js dependencies and start scripts
└── README.md
```

## Hardware Setup

| Component | Specification | Pin / Interface |
| :--- | :--- | :--- |
| **MCU** | AI-Thinker ESP32-CAM (4MB PSRAM) | Core Controller |
| **Camera** | OV2640 Sensor (UXGA / SVGA) | Onboard DVP |
| **Motion Sensor** | HC-SR501 / AM312 PIR | `GPIO 13` (RTC IO) |
| **Actuator** | SG90 PWM Micro Servo | `GPIO 14` |
| **Power Supply** | 5V / 2A Buck Converter | `5V` / `GND` |

<details>
<summary><strong>FTDI Flashing Pinout (Click to expand)</strong></summary>

<br>

The ESP32-CAM lacks an onboard USB-to-UART bridge. Connect an external 3.3V/5V FTDI programmer as follows:

| ESP32-CAM | FTDI Programmer | Notes |
| :--- | :--- | :--- |
| `5V` | `VCC (5V)` | Use 5V jumper to prevent brownouts |
| `GND` | `GND` | Common ground |
| `U0R` | `TX` | Serial Receive |
| `U0T` | `RX` | Serial Transmit |
| `GPIO 0` | `GND` | **Bridge only while flashing**; remove to boot |

</details>

## Getting Started

### 1. Backend & Node-RED Setup

Clone the repository and install the Node-RED runtime dependencies[cite: 1]:

```bash
git clone [https://github.com/sannihith-madasu/securedrop.git](https://github.com/sannihith-madasu/securedrop.git)
cd securedrop
npm install
```

Start the Node-RED instance using the local project configuration[cite: 1]:

```bash
npx node-red -s settings.js flows.json
```

Open `http://localhost:1880` in your browser to inspect the flow or configure your Telegram Bot token credentials.

### 2. Testing via Simulation

To verify the pipeline without physical ESP32-CAM hardware connected, run the simulator script in a separate terminal[cite: 1]:

```bash
node simulate.js
```

This dispatches synthetic motion and image events into the Node-RED ingestion endpoint to verify downstream Telegram notifications.

### 3. Flashing Firmware

1. Open the `firmware/` directory in PlatformIO or Arduino IDE[cite: 1].
2. Copy the template header and populate your Wi-Fi credentials, Node-RED host IP, and Telegram parameters.
3. Bridge `GPIO 0` to `GND`, compile, and upload.
4. Remove the `GPIO 0` bridge and press the onboard `RST` button.

## Acknowledgments & Disclaimer

This project was built as part of the Summer Internship Program at **Sreenidhi Institute of Science and Technology (SNIST)** and is intended strictly as a proof-of-concept and educational prototype and is not a hardened, production-ready security system. 

