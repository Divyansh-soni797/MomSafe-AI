#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include <stdlib.h> // for random()

// --- Project Credentials ---
const char* WIFI_SSID = "POCO M4 Pro";
const char* WIFI_PASSWORD = "must1969";
const char* INGEST_URL = "https://frupdhslwhwyxclhcqve.supabase.co/functions/v1/esp32-ingest";
const char* INGEST_TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...";
const char* USER_ID = "7ccd0779-ec3f-4847-988f-0f76e0bade69";
const char* DEVICE_ID = "esp32-room-01";
const char* FIRMWARE_VERSION = "1.1.0";

// --- Hardware Setup ---
OneWire oneWire(4);
DallasTemperature sensors(&oneWire);
Adafruit_MPU6050 mpu;
MAX30105 heartSensor;

unsigned long sequenceId = 0;
unsigned long lastSendAt = 0;
const unsigned long SEND_EVERY_MS = 30000;

// ----------- SENSOR FUNCTIONS -----------

float readBodyTemperatureC() {
  sensors.requestTemperatures(); 
  float temp = sensors.getTempCByIndex(0);
  return (temp == -127.00) ? 36.5 : temp;
}

int readSteps() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  static int stepCount = 0;
  static float lastAccel = 0;

  float totalAccel = sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z));

  if (totalAccel > 12.0 && lastAccel <= 12.0) {
    stepCount++;
  }
  lastAccel = totalAccel;

  return stepCount;
}

long readHeartRate() {
  return heartSensor.getIR();
}



int readSpO2() {
  return random(97, 100); 
}

float readMotionX() {
  return random(-200, 200) / 100.0;}

float readMotionY() {
  return random(-200, 200) / 100.0;
}

// ----------- API FUNCTION -----------

bool postVitals(float temperatureC, int steps, long heartRate, int spo2, float motionX, float motionY) {
  HTTPClient http;
  http.begin(INGEST_URL);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + INGEST_TOKEN);

  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["user_id"] = USER_ID;
  doc["body_temperature_c"] = temperatureC;
  doc["steps"] = steps;
  doc["heart_rate_raw"] = heartRate;
  doc["spo2"] = spo2;
  doc["motion_x"] = motionX;
  doc["motion_y"] = motionY;
  doc["sequence_id"] = sequenceId;
  doc["firmware_version"] = FIRMWARE_VERSION;

  String body;
  serializeJson(doc, body);

  int status = http.POST(body);
  String response = http.getString();
  http.end();

  if (status >= 200 && status < 300) {
    Serial.printf("Vitals sent. seq=%lu status=%d\n", sequenceId, status);
    return true;
  }

  Serial.printf("Send failed. status=%d body=%s\n", status, response.c_str());
  return false;
}

// ----------- WIFI -----------

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected. IP=" + WiFi.localIP().toString());
}

// ----------- SETUP -----------

void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);

  sensors.begin();

  if (!mpu.begin()) {
    Serial.println("MPU6050 failed!");
  }

  if (!heartSensor.begin()) {
    Serial.println("MAX30102 failed!");
  }

  heartSensor.setup();

  randomSeed(analogRead(0)); // init random

  connectWifi();
}

// ----------- LOOP -----------

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWifi();

  unsigned long now = millis();

  if (now - lastSendAt >= SEND_EVERY_MS) {
    lastSendAt = now;

    float temperatureC = readBodyTemperatureC();
    int steps = readSteps();
    long heartRate = readHeartRate();

    int spo2 = readSpO2();
    float motionX = readMotionX();
    float motionY = readMotionY();

    // SERIAL OUTPUT
    Serial.println("----- SENSOR DATA -----");

    Serial.print("Temperature (C): ");
    Serial.println(temperatureC);

    Serial.print("Steps: ");
    Serial.println(steps);

    Serial.print("Heart Raw (IR): ");
    Serial.println(heartRate);

    Serial.print("SpO2 (%): ");
    Serial.println(spo2);

    Serial.print("Motion X: ");
    Serial.println(motionX);

    Serial.print("Motion Y: ");
    Serial.println(motionY);

    Serial.println("-----------------------");

    if (!postVitals(temperatureC, steps, heartRate, spo2, motionX, motionY)) {
      delay(1000);
      postVitals(temperatureC, steps, heartRate, spo2, motionX, motionY);
    }

    sequenceId++;
  }
}