#include <Arduino.h>
#include <WiFi.h>

#include <PMS.h>
#include <SparkFun_SGP40_Arduino_Library.h>
#include <UMS3.h>
//#include <sensiron_voc_algorithm.h>

#include <Wire.h>

#include "RemoteLogger.h"
#include "wifi_credentials.h"

#include "nvs.h"
#include "nvs_flash.h"

// TODO
#define PMS_RX 1
#define PMS_TX 2

#define VOC_POLL_INTERVAL_MS 1 * 1000
#define PMS_POLL_INTERVAL_MS 15 * 1000
#define PMS_WARMUP_MS 30 * 1000

// Until we add a humidity sensor, just make assumptions
#define VOC_RH 50 // Assume about 50% relative humidity
#define VOC_T 21  // Assume ~70F indoor temp

RemoteLogger *remoteLogger;

HardwareSerial pmsSerial(2);
PMS pms(pmsSerial);
UMS3 ums3;

SGP40 sgp;
VocAlgorithmParams vocAlgorithmParameters;

unsigned long lastVOCMillis, lastPMSMillis, pmsStartMillis;

bool pmsSleeping = true;

void pms_wake() {
  if (!pmsSleeping) {
    return;
  }

  pms.wakeUp();
  pmsSleeping = false;
  lastPMSMillis = pmsStartMillis = millis();
}

void pms_sleep() {
  if (pmsSleeping) {
    return;
  }

  pms.sleep();
  pmsSleeping = true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  ums3.begin();
  ums3.setPixelBrightness(100);
  ums3.setPixelColor(UMS3::color(0, 0, 255)); // Blue

  // Set these in gitignored file include/wifi_credentials.h
  WiFi.begin(wifi_ssid, wifi_pass);

  remoteLogger = new RemoteLogger("stove-exhaust-ctrl", "home-logger-local.itsshedtime.com");

  pmsSerial.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  pms_wake();

  Wire.begin();
  if (!sgp.begin()) {
    Serial.println("SGP40 not detected, freezing");
    esp_light_sleep_start();
  }

  VocAlgorithm_init(&vocAlgorithmParameters);

  Serial.println("Setup complete");
  ums3.setPixelColor(UMS3::color(255, 0, 255)); // Purple
}

void loop() {
  uint16_t voc_ticks;
  int32_t voc_index;
  PMS::DATA data;

  // Poll the VOC sensor at a consistent rate for the VOC index algorithm
  unsigned long time_since_voc_millis = millis() - lastVOCMillis;
  if (time_since_voc_millis < VOC_POLL_INTERVAL_MS) {
    delay(VOC_POLL_INTERVAL_MS - time_since_voc_millis);
  }

  SGP40ERR err = sgp.measureRaw(&voc_ticks, VOC_RH, VOC_T);
  if (err != SGP40_SUCCESS) {
    Serial.printf("SGP40ERR: %d\n", err);
    return;
  }

  unsigned long now = millis();
  lastVOCMillis = now;

  VocAlgorithm_process(&vocAlgorithmParameters, voc_ticks, &voc_index);

  wl_status_t wifi_status = WiFi.status();
  if (wifi_status != WL_CONNECTED) {
    if (wifi_status == WL_IDLE_STATUS) {
      WiFi.begin(wifi_ssid, wifi_pass);
    }
    ums3.setPixelColor(UMS3::color(255, 0, 0)); // Red
    Serial.printf("Wifi Status: %d\n", wifi_status);
    // If wifi is disconnected, put the PM sensor to sleep to extend its lifespan.
    // We keep polling the VOC sensor to keep the index algo accurate
    pms_sleep();
    return;
  }
  ums3.setPixelColor(UMS3::color(0, 255, 0)); // Green
  pms_wake();

  if (now - pmsStartMillis < PMS_WARMUP_MS) {
    // Sensor isn't ready yet
    return;
  }
  if (now - lastPMSMillis < PMS_POLL_INTERVAL_MS) {
    // Not time to take a reading yet
    return;
  }

  if (!pms.read(data)) {
    Serial.printf("PMS error\n");
    return;
  }

  char event_data[128];
  snprintf(event_data, sizeof(event_data), "voc_ticks=%u voc_index=%u pm25=%u", voc_ticks,
           voc_index, data.PM_SP_UG_2_5);

  Serial.println(event_data);
  remoteLogger->log(event_data, LEVEL_INFO);
}
