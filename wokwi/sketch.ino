/*
  SecureDrop - edge node, Wokwi simulation build
  ------------------------------------------------------------------
  Same cycle as the ESP32-CAM firmware:
      deep sleep -> PIR wake (GPIO 13) -> capture -> Wi-Fi -> MQTT -> deep sleep

  Differences from the real firmware, both because of the simulator:
    1. Wokwi has no camera, so a stored test JPEG (parcel.h) is sent
       in place of the OV2640 capture.
    2. Wokwi cannot reach a broker on the PC, so a public broker is
       used and the topics carry a private prefix.

  To trigger: click the PIR sensor, then "Simulate motion".
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "parcel.h"

// ---------------- settings ----------------
const char* WIFI_SSID    = "Wokwi-GUEST";        // Wokwi's virtual access point
const char* WIFI_PASS    = "";
const char* MQTT_HOST    = "broker.hivemq.com";  // public broker (simulation only)
const uint16_t MQTT_PORT = 1883;
const char* DEVICE_ID    = "securedrop-01";
const char* TOPIC_EVENT  = "sd0475/securedrop/event";   // must match the Node-RED flow
const char* TOPIC_IMAGE  = "sd0475/securedrop/image";

// 1 = real deep sleep with ext0 wake (as on the hardware).
// 0 = stay awake and poll the PIR. Use this only if the simulator
//     does not wake the chip when the PIR fires.
#define USE_DEEP_SLEEP 0
// ------------------------------------------

#define PIR_PIN GPIO_NUM_13      // RTC-capable pin, usable as ext0 wake source

RTC_DATA_ATTR uint32_t bootCount = 0;      // kept in RTC memory across deep sleep
uint32_t cycle = 0;

WiFiClient net;
PubSubClient mqtt(net);

// On the ESP32-CAM this is esp_camera_fb_get(). Here it returns the test frame.
const uint8_t* captureImage(size_t* len) {
  *len = PARCEL_JPG_LEN;
  return PARCEL_JPG;
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);     // channel 6: skips the scan in Wokwi
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(100);
  return WiFi.status() == WL_CONNECTED;
}

bool publishDelivery(const uint8_t* img, size_t len) {
  char clientId[40];
  snprintf(clientId, sizeof(clientId), "%s-%04X", DEVICE_ID, (unsigned)random(0x10000));
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setSocketTimeout(15);
  if (!mqtt.connect(clientId)) return false;
  Serial.printf("[mqtt ] connected to %s:%u\n", MQTT_HOST, MQTT_PORT);

  // 1. small JSON event
  char evt[180];
  snprintf(evt, sizeof(evt),
           "{\"device\":\"%s\",\"event\":\"delivery\",\"wake\":\"pir\",\"rssi\":%d,\"bytes\":%u,\"boot\":%u}",
           DEVICE_ID, (int)WiFi.RSSI(), (unsigned)len, (unsigned)bootCount);
  if (!mqtt.publish(TOPIC_EVENT, evt)) return false;
  Serial.printf("[mqtt ] event  -> %s\n        %s\n", TOPIC_EVENT, evt);

  // 2. JPEG bytes, streamed in 512-byte chunks
  if (!mqtt.beginPublish(TOPIC_IMAGE, len, false)) return false;
  uint8_t chunk[512];
  for (size_t i = 0; i < len; i += sizeof(chunk)) {
    size_t n = min(sizeof(chunk), len - i);
    memcpy_P(chunk, img + i, n);
    if (mqtt.write(chunk, n) != n) return false;
  }
  if (!mqtt.endPublish()) return false;
  Serial.printf("[mqtt ] image  -> %s  (%u bytes)\n", TOPIC_IMAGE, (unsigned)len);

  for (int i = 0; i < 10; i++) { mqtt.loop(); delay(50); }   // flush before closing
  mqtt.disconnect();
  return true;
}

void runDelivery() {
  uint32_t t0 = millis();
  size_t len = 0;
  const uint8_t* img = captureImage(&len);
  Serial.printf("[cam  ] frame ready: %u bytes JPEG (test frame, no camera in simulator)\n", (unsigned)len);

  if (!connectWiFi()) { Serial.println("[wifi ] FAILED to connect"); return; }
  Serial.printf("[wifi ] connected to %s, IP %s, RSSI %d dBm\n",
                WIFI_SSID, WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());

  bool ok = publishDelivery(img, len);
  Serial.println(ok ? "[done ] delivery published" : "[mqtt ] FAILED to publish");
  Serial.printf("[time ] awake for %lu ms\n", (unsigned long)(millis() - t0));

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void waitForPirLow() {
  // The PIR output stays HIGH for a few seconds. Sleeping while it is still
  // HIGH would wake the node again at once, so wait for it to drop first.
  while (digitalRead(PIR_PIN) == HIGH) delay(50);
}

void goToSleep() {
  waitForPirLow();
  rtc_gpio_pullup_dis(PIR_PIN);
  rtc_gpio_pulldown_en(PIR_PIN);
  esp_sleep_enable_ext0_wakeup(PIR_PIN, 1);          // wake when PIR OUT goes HIGH
  Serial.println("[sleep] entering deep sleep, waiting for PIR on GPIO 13\n");
  Serial.flush();
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  bootCount++;

#if USE_DEEP_SLEEP
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.printf("\n[wake ] boot #%u, cause: PIR motion (ext0, GPIO 13)\n", (unsigned)bootCount);
    runDelivery();
  } else {
    Serial.println("\nSecureDrop edge node (Wokwi simulation)");
    Serial.printf("[boot ] power-on, boot #%u. Arming PIR.\n", (unsigned)bootCount);
  }
  goToSleep();
#else
  Serial.println("\nSecureDrop edge node (Wokwi simulation, polling mode)");
  Serial.println("[idle ] waiting for PIR on GPIO 13");
#endif
}

void loop() {
#if !USE_DEEP_SLEEP
  if (digitalRead(PIR_PIN) == HIGH) {
    bootCount = ++cycle;
    Serial.printf("\n[wake ] cycle #%u, cause: PIR motion (GPIO 13)\n", (unsigned)cycle);
    runDelivery();
    waitForPirLow();
    Serial.println("[idle ] waiting for PIR on GPIO 13\n");
  }
  delay(50);
#endif
}
