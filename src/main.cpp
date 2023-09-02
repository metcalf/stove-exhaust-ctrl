#include <Arduino.h>
#include <WiFi.h>

//#include <PMS.h>
#include <PMserial.h>
#include <SparkFun_SGP40_Arduino_Library.h>
#include <UMS3.h>
//#include <sensiron_voc_algorithm.h>

#include <Wire.h>

#include "RemoteLogger.h"
#include "wifi_credentials.h"

// TODO
#define PMS_RX 2
#define PMS_TX 1

#define VOC_POLL_INTERVAL_MS 1 * 1000
#define PMS_POLL_INTERVAL_MS 15 * 1000
#define PMS_WARMUP_MS 30 * 1000

// Until we add a humidity sensor, just make assumptions
#define VOC_RH 50 // Assume about 50% relative humidity
#define VOC_T 21  // Assume ~70F indoor temp

RemoteLogger *remoteLogger;

SerialPM pms(PMSx003, PMS_RX, PMS_TX);
UMS3 ums3;

SGP40 sgp;
VocAlgorithmParams vocAlgorithmParameters;

unsigned long lastVOCMillis, lastPMSMillis, pmsStartMillis;

bool pmsSleeping = true;

void pms_wake() {
  if (!pmsSleeping) {
    return;
  }

  pms.wake();
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
  // Serial.setDebugOutput(true);
  Serial.println("Booting");

  ums3.begin();
  ums3.setPixelBrightness(100);
  ums3.setPixelColor(UMS3::color(0, 0, 255)); // Blue

  // Set these in gitignored file include/wifi_credentials.h
  WiFi.disconnect();
  WiFi.begin(wifi_ssid, wifi_pass);
  WiFi.setTxPower(WIFI_POWER_13dBm);

  remoteLogger = new RemoteLogger("stove-exhaust-ctrl", "home-logger-local.itsshedtime.com");

  pms.init();
  pms_sleep();

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
    if (wifi_status == WL_IDLE_STATUS || wifi_status == WL_CONNECT_FAILED) {
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
    Serial.println("warming up");
    // Sensor isn't ready yet
    return;
  }
  if (now - lastPMSMillis < PMS_POLL_INTERVAL_MS) {
    Serial.println("waiting to poll PMS");
    // Not time to take a reading yet
    return;
  }

  SerialPM::STATUS pms_status = pms.read();
  if (pms_status != SerialPM::STATUS::OK) {
    Serial.printf("PMS error: %d\n", pms_status);
    return;
  }
  if (!pms.has_particulate_matter()) {
    Serial.println("Could not decode PM measurement");
    return;
  }
  lastPMSMillis = now;

  char event_data[128];
  snprintf(event_data, sizeof(event_data), "voc_ticks=%u voc_index=%u pm25=%u", voc_ticks,
           voc_index, pms.pm25);

  Serial.println(event_data);
  remoteLogger->log(event_data, LEVEL_INFO);
}
