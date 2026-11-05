/*
  SecureDrop - edge node firmware (ESP32-CAM, AI-Thinker)
  Board:    Tools > Board > "AI Thinker ESP32-CAM"
  Library:  PubSubClient (Nick O'Leary), via Library Manager
  Wiring:   PIR VCC -> 5V, PIR GND -> GND, PIR OUT -> GPIO 13

  Deep sleep resets the chip on every wake, so one whole delivery cycle
  lives in setup():  wake -> capture -> Wi-Fi -> MQTT publish -> sleep.
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ---------------- settings: edit these ----------------
const char* WIFI_SSID   = "YOUR_WIFI";
const char* WIFI_PASS   = "YOUR_PASSWORD";
const char* MQTT_HOST   = "192.168.1.10";   // IP of the PC running Node-RED (ipconfig / ifconfig)
const uint16_t MQTT_PORT = 1883;
const char* DEVICE_ID   = "securedrop-01";
// ------------------------------------------------------

#define PIR_PIN    GPIO_NUM_13   // RTC-capable, safe at boot
#define FLASH_PIN  GPIO_NUM_4    // on-board white LED

// AI-Thinker ESP32-CAM camera pins
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

RTC_DATA_ATTR uint32_t bootCount = 0;   // survives deep sleep

WiFiClient net;
PubSubClient mqtt(net);

bool initCamera() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM; c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM; c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size   = FRAMESIZE_VGA;     // 640x480, a few tens of kB
  c.jpeg_quality = 12;                // 0-63, lower = better
  c.fb_count     = 1;
  c.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  c.grab_mode    = CAMERA_GRAB_LATEST;
  return esp_camera_init(&c) == ESP_OK;
}

void goToSleep() {
  mqtt.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // HC-SR501 holds its output HIGH for a few seconds. Sleeping while it is
  // still HIGH would wake the node again immediately, so wait for LOW first.
  pinMode(PIR_PIN, INPUT);
  uint32_t t0 = millis();
  while (digitalRead(PIR_PIN) == HIGH && millis() - t0 < 15000) delay(50);

  digitalWrite(FLASH_PIN, LOW);
  rtc_gpio_hold_en(FLASH_PIN);                    // keep the LED off during sleep
  rtc_gpio_pullup_dis(PIR_PIN);
  rtc_gpio_pulldown_en(PIR_PIN);
  esp_sleep_enable_ext0_wakeup(PIR_PIN, 1);       // wake when PIR OUT goes HIGH
  Serial.println("Sleeping.");
  Serial.flush();
  esp_deep_sleep_start();
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);      // brown-out workaround for USB power
  Serial.begin(115200);
  rtc_gpio_hold_dis(FLASH_PIN);
  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);
  bootCount++;

  // Power-on or reset (not a PIR wake): just arm the sensor and sleep.
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Power-on. Arming PIR.");
    goToSleep();
  }

  // 1. Capture. Throw away the first frame so auto-exposure can settle.
  if (!initCamera()) { Serial.println("Camera init failed"); goToSleep(); }
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
  delay(300);
  fb = esp_camera_fb_get();
  if (!fb) { Serial.println("Capture failed"); goToSleep(); }
  Serial.printf("Captured %u bytes\n", (unsigned)fb->len);

  // 2. Wi-Fi (give up after 15 s rather than drain the battery)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(200);
  if (WiFi.status() != WL_CONNECTED) { Serial.println("Wi-Fi failed"); esp_camera_fb_return(fb); goToSleep(); }

  // 3. MQTT
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(2048);
  mqtt.setSocketTimeout(10);
  if (!mqtt.connect(DEVICE_ID)) { Serial.println("MQTT failed"); esp_camera_fb_return(fb); goToSleep(); }

  // 4. Event first, then the image bytes (streamed in 1 kB chunks)
  char evt[160];
  snprintf(evt, sizeof(evt),
           "{\"device\":\"%s\",\"event\":\"delivery\",\"wake\":\"pir\",\"rssi\":%d,\"bytes\":%u,\"boot\":%u}",
           DEVICE_ID, (int)WiFi.RSSI(), (unsigned)fb->len, (unsigned)bootCount);
  mqtt.publish("securedrop/event", evt);

  bool ok = mqtt.beginPublish("securedrop/image", fb->len, false);
  for (size_t i = 0; ok && i < fb->len; i += 1024) {
    size_t n = min((size_t)1024, fb->len - i);
    ok = mqtt.write(fb->buf + i, n) == n;
  }
  ok = mqtt.endPublish() && ok;
  Serial.println(ok ? "Published." : "Publish failed");
  esp_camera_fb_return(fb);

  mqtt.loop();
  delay(200);                                     // let the last TCP segment leave
  goToSleep();
}

void loop() {}   // never reached
