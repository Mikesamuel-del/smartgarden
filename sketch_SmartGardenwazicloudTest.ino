#include <HTTPClient.h>

#include <dummy.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <HTTPUpdate.h>
#include "secrets.h"

// === Wi-Fi Credentials ===
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// === OTA Firmware Update ===
const char* firmware_url = "http://yourserver.com/firmware.bin";  // replace with your URL
const unsigned long otaCheckInterval = 6L * 60L * 60L * 1000L;     // check OTA every 6 hours
unsigned long lastOtaCheck = 0;

// === WaziCloud Credentials ===
const String token = WAZI_TOKEN;
const String device_id = "SG001";

// === Sensor and Pin Definitions ===
#define DHTPIN 15
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define RELAY_PIN 5

DHT dht(DHTPIN, DHTTYPE);

// === Soil Moisture Ranges ===
const int drySoil = 20;
const int wetSoil = 30;

// === Timers ===
unsigned long lastWiFiAttempt = 0;
const unsigned long wifiTimeout = 10000; // 10 seconds timeout

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Pump OFF
  dht.begin();
}

void loop() {
  // === Read Sensors ===
  int rawMoisture = analogRead(SOIL_PIN);
  int soilMoisture = map(rawMoisture, 4095, 2095, 0, 100);

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  bool pumpStatus = digitalRead(RELAY_PIN) == LOW;

  // === Garden Logic ===
  if (soilMoisture < drySoil) {
    digitalWrite(RELAY_PIN, LOW);  // Pump ON
  } else if (soilMoisture > wetSoil) {
    digitalWrite(RELAY_PIN, HIGH); // Pump OFF
  }

  // === Try to connect WiFi briefly ===
  connectToWiFi();

  // === Send data if connected ===
  if (WiFi.status() == WL_CONNECTED) {
    sendToWazi("soilmoisture", soilMoisture);
    sendToWazi("temperature", temperature);
    sendToWazi("humidity", humidity);
    sendPumpToWazi("pump", pumpStatus);
  }

  // === Periodic OTA Check ===
  if (millis() - lastOtaCheck > otaCheckInterval) {
    lastOtaCheck = millis();
    if (WiFi.status() == WL_CONNECTED) {
      checkForOTA();
    }
  }

  delay(15000); // wait 15s
}

// === WiFi Connect (with timeout) ===
void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.begin(ssid, password);
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < wifiTimeout) {
    delay(500);
  }
}

// === Send Data to WaziCloud ===
void sendToWazi(String sensor_id, float value) {
  HTTPClient http;
  String url = "https://api.waziup.io/api/v2/devices/" + device_id + "/sensors/" + sensor_id + "/value";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", token);

  String payload = "{\"value\": " + String(value, 2) + "}";
  http.POST(payload);
  http.end();
}

// === Send Pump Status to WaziCloud ===
void sendPumpToWazi(String actuator_id, bool value) {
  HTTPClient http;
  String url = "https://api.waziup.io/api/v2/devices/" + device_id + "/actuators/" + actuator_id + "/value";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", token);

  String payload = "{\"value\": " + String(value) + "}";
  http.PUT(payload);
  http.end();
}

// === OTA Firmware Update ===

void checkForOTA() {
  Serial.println("[OTA] Checking for firmware update...");

  WiFiClient client;
  String fwURL = firmware_url;  // Convert to String

  t_httpUpdate_return ret = httpUpdate.update(client, fwURL);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA] Update failed. Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[OTA] No new firmware available.");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("[OTA] Update successful.");
      break;
  }
}




