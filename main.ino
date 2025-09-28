/*
  ESP32 + DHT11 + MPU6050 (robust Wi-Fi)
  - samples MPU6050 for SAMPLE_WINDOW_MS milliseconds
  - computes vibration RMS and peak dynamic deviation (g)
  - sends aggregated values to ThingSpeak (fields 1..5)
  - connects to Wi-Fi once, keeps connection alive, only reconnects on drop
*/

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include "MPU6050_light.h"

// -------- SENSOR / I2C --------
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

MPU6050 mpu(Wire);
const int I2C_SDA = 21;
const int I2C_SCL = 22;

// -------- WIFI / THINGSPEAK --------
const char* ssid = "ragnarok";
const char* password = "aadikadam";
const char* thingspeakApiKey = "J918QDBIWSXCJK6H";

// -------- SAMPLING CONFIG --------
const unsigned long SAMPLE_WINDOW_MS = 5000;   // sample duration (5 seconds)
const unsigned long TOTAL_LOOP_MS   = 20000;   // total loop interval to respect ThingSpeak (20s)
const unsigned long SAMPLE_DELAY_US = 10000;   // inter-sample delay: 10000us -> ~100 Hz (adjust if needed)

// -------- SETUP --------
void setup() {
  Serial.begin(115200);
  delay(200);

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(50);

  // DHT
  dht.begin();

  // MPU6050
  byte status = mpu.begin();
  Serial.print("MPU6050 begin status: ");
  Serial.println(status);
  if (status != 0) {
    Serial.println("MPU init failed - check wiring");
  } else {
    Serial.println("MPU OK. Calibrating (keep still)...");
    delay(500);
    mpu.calcOffsets(); // optional: calibrate if the sensor is still at startup
    Serial.println("MPU calibrated.");
  }

  // WiFi: configure and connect once
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // let the driver try to reconnect automatically
  WiFi.persistent(true);       // keep credentials in flash across resets
  connectWiFi();
}

// -------- MAIN LOOP --------
void loop() {
  unsigned long loopStart = millis();

  // --- light connection check: only reconnect if dropped ---
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi dropped or disconnected — attempting reconnect...");
    connectWiFi();
  }

  // --- Read DHT (single instant reading) ---
  float humidity = dht.readHumidity();
  float tempC = dht.readTemperature();
  float heatIndexC = NAN;
  if (!isnan(tempC) && !isnan(humidity)) {
    heatIndexC = dht.computeHeatIndex(tempC, humidity, false);
  }

  // --- Sample MPU6050 for SAMPLE_WINDOW_MS and compute stats ---
  float rms = 0.0f;
  float peak = 0.0f;
  sampleVibrationWindow(SAMPLE_WINDOW_MS, SAMPLE_DELAY_US, &rms, &peak);

  // --- Build ThingSpeak URL (fields 1-5) ---
  String url = "http://api.thingspeak.com/update?api_key=";
  url += thingspeakApiKey;
  if (!isnan(tempC))   url += "&field1=" + String(tempC, 2);
  if (!isnan(humidity)) url += "&field2=" + String(humidity, 2);
  if (!isnan(heatIndexC)) url += "&field3=" + String(heatIndexC, 2);
  url += "&field4=" + String(rms, 6);   // vibration RMS (g)
  url += "&field5=" + String(peak, 6);  // vibration peak (g)

  Serial.println("=== Uploading aggregated data ===");
  Serial.printf("Temp: %s, Hum: %s, HeatIdx: %s, RMS: %.6f g, Peak: %.6f g\n",
                (isnan(tempC) ? "NaN" : String(tempC,2).c_str()),
                (isnan(humidity) ? "NaN" : String(humidity,2).c_str()),
                (isnan(heatIndexC) ? "NaN" : String(heatIndexC,2).c_str()),
                rms, peak);
  Serial.println(url);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.printf("ThingSpeak response code: %d, entry ID: %s\n", httpCode, payload.c_str());
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("WiFi not connected - skipped ThingSpeak upload");
  }

  // Wait until TOTAL_LOOP_MS has passed since loopStart
  unsigned long elapsed = millis() - loopStart;
  if (elapsed < TOTAL_LOOP_MS) {
    unsigned long waitMs = TOTAL_LOOP_MS - elapsed;
    Serial.printf("Waiting %.0lu ms until next cycle...\n", waitMs);
    delay(waitMs);
  } else {
    // If sampling + upload already took longer than TOTAL_LOOP_MS, small pause before next cycle
    delay(1000);
  }
}

// -------- FUNCTIONS --------
void connectWiFi() {
  // If already connected, do nothing.
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi already connected.");
    return;
  }

  Serial.printf("Connecting to WiFi '%s' ... ", ssid);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  const unsigned long timeout = 20000; // try for up to 20s
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected.");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect failed (will retry later).");
    // Do not block forever here; we'll return and try again in loop() if needed.
  }
}

// Samples accelerometer for 'windowMs' and returns RMS and Peak dynamic deviation (in g).
// - sampleDelayUs: delay between samples in microseconds.
// - rms_out, peak_out are written with results.
void sampleVibrationWindow(unsigned long windowMs, unsigned long sampleDelayUs, float* rms_out, float* peak_out) {
  unsigned long tStart = millis();
  unsigned long count = 0;
  double sumSq = 0.0;
  double peakAbs = 0.0;

  while (millis() - tStart < windowMs) {
    mpu.update();
    double ax = mpu.getAccX();
    double ay = mpu.getAccY();
    double az = mpu.getAccZ();

    double mag = sqrt(ax*ax + ay*ay + az*az); // total magnitude (g)
    double dyn = mag - 1.0;                    // remove 1g gravity -> dynamic part (g)
    double absDyn = fabs(dyn);

    sumSq += dyn * dyn;
    if (absDyn > peakAbs) peakAbs = absDyn;
    count++;

    // spacing between samples
    if (sampleDelayUs >= 1000) {
      delay(sampleDelayUs / 1000);
    } else {
      delayMicroseconds(sampleDelayUs);
    }
  }

  if (count == 0) {
    *rms_out = 0.0f;
    *peak_out = 0.0f;
    Serial.println("No samples captured!");
    return;
  }

  double meanSq = sumSq / (double)count;
  double rms = sqrt(meanSq); // in g

  *rms_out = (float)rms;
  *peak_out = (float)peakAbs;

  Serial.printf("Samples: %u, RMS: %.6f g, Peak: %.6f g\n", (unsigned int)count, rms, peakAbs);
}
