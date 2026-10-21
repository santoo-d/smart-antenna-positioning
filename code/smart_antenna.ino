/*
  Smart Antenna Position System
  Board   : ESP32 (assumed)
  Sensors : LDR (light), DHT11 (temperature & humidity)
  Outputs : 2 servos (azimuth = pan, elevation = tilt)
  App     : Blynk IoT dashboard (sliders, gauges, auto-mode switch)

  Modes:
  - Manual: move the antenna with the Azimuth / Elevation sliders.
  - Auto  : scan the servos, measure light at each position and
            move the antenna to the position with the strongest reading.

  Blynk virtual pins:
    V0 Azimuth slider (0-180)      V4 Humidity gauge
    V1 Elevation slider (0-180)    V5 Azimuth value
    V2 Light intensity gauge (0-100) V6 Elevation value
    V3 Temperature gauge           V7 Auto mode switch
*/

#include "secrets.h"            // copy secrets_example.h -> secrets.h

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <DHT.h>

// ---------- Pins (change to match your wiring) ----------
const int AZIMUTH_PIN   = 18;
const int ELEVATION_PIN = 19;
const int LDR_PIN       = 34;   // analog input
const int DHT_PIN       = 4;
#define DHT_TYPE DHT11

// ---------- Objects ----------
Servo azimuthServo;
Servo elevationServo;
DHT dht(DHT_PIN, DHT_TYPE);
BlynkTimer timer;

// ---------- State ----------
int azimuthAngle   = 90;
int elevationAngle = 90;
bool autoMode      = false;
unsigned long lastScan = 0;
const unsigned long RESCAN_INTERVAL = 30000;  // ms between auto scans

// ---------- Helpers ----------
int readLight() {
  // 12-bit ADC (0-4095) -> 0-100 %. Swap the range if your LDR is wired inverted.
  return map(analogRead(LDR_PIN), 0, 4095, 0, 100);
}

void moveAntenna(int az, int el) {
  azimuthAngle   = constrain(az, 0, 180);
  elevationAngle = constrain(el, 0, 180);
  azimuthServo.write(azimuthAngle);
  elevationServo.write(elevationAngle);
  Blynk.virtualWrite(V5, azimuthAngle);
  Blynk.virtualWrite(V6, elevationAngle);
}

// ---------- Blynk inputs ----------
BLYNK_WRITE(V0) { if (!autoMode) moveAntenna(param.asInt(), elevationAngle); }
BLYNK_WRITE(V1) { if (!autoMode) moveAntenna(azimuthAngle, param.asInt()); }
BLYNK_WRITE(V7) {
  autoMode = param.asInt();
  if (autoMode) lastScan = 0;   // scan immediately
}

// ---------- Sensor upload ----------
void sendSensorData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  Blynk.virtualWrite(V2, readLight());
  if (!isnan(t)) Blynk.virtualWrite(V3, t);
  if (!isnan(h)) Blynk.virtualWrite(V4, h);
}

// ---------- Auto scan: find the position with the strongest light ----------
void autoScan() {
  int bestLight = -1, bestAz = azimuthAngle, bestEl = elevationAngle;

  for (int el = 30; el <= 150; el += 30) {
    for (int az = 0; az <= 180; az += 15) {
      moveAntenna(az, el);
      for (int i = 0; i < 25; i++) { Blynk.run(); delay(10); }  // settle
      int light = readLight();
      if (light > bestLight) {
        bestLight = light; bestAz = az; bestEl = el;
      }
      if (!autoMode) return;    // user switched auto off
    }
  }
  moveAntenna(bestAz, bestEl);
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  azimuthServo.attach(AZIMUTH_PIN);
  elevationServo.attach(ELEVATION_PIN);
  moveAntenna(90, 90);

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
  timer.setInterval(2000L, sendSensorData);
}

void loop() {
  Blynk.run();
  timer.run();

  if (autoMode && (lastScan == 0 || millis() - lastScan > RESCAN_INTERVAL)) {
    autoScan();
    lastScan = millis();
  }
}
