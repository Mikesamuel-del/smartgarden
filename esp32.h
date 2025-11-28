#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// ------------------------
// CONFIGURATION
// ------------------------
#define DHTPIN 4           // DHT11 data pin
#define DHTTYPE DHT11
#define MOIST_PIN 34       // Soil moisture analog pin
#define RELAY_PIN 26       // Relay for irrigation

DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Supabase REST endpoint
const char* SUPA_URL = "https://YOUR_PROJECT.supabase.co/rest/v1";
const char* SUPA_KEY = "YOUR_SUPABASE_KEY";

// Garden ID for this ESP32
const char* GARDEN_ID = "00000000-0000-0000-0000-000000000001";

const int LOG_INTERVAL = 5000; // 5 seconds
const int INTERNET_RETRY = 5000; // Retry every 5 sec

// ------------------------
// GLOBAL VARIABLES
// ------------------------
float opt_temp = 25;
float opt_hum = 65;
float opt_moist = 40;
int total_days = 30;
String crop_name = "onions";
String start_date = "2025-11-28T00:00:00Z";

// ------------------------
// SETUP
// ------------------------
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(MOIST_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Ensure irrigation off at start

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Fetch initial optimum conditions
  fetchOptimumConditions();
}

// ------------------------
// MAIN LOOP
// ------------------------
void loop() {
  // Read sensors
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int moisture = analogRead(MOIST_PIN); // 0-4095 for ESP32

  // Convert moisture to 0-100%
  float moisture_percent = map(moisture, 4095, 0, 0, 100);

  // Compute days remaining
  int remaining_days = computeRemainingDays();

  // Determine status
  String status = determineStatus(temperature, humidity, moisture_percent);

  // Actuation
  if (moisture_percent < opt_moist - 3) {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Irrigation ON");
  } else if (moisture_percent > opt_moist + 3) {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Irrigation OFF");
  }

  // Print readings
  Serial.printf("Temp: %.2f, Hum: %.2f, Moist: %.2f, Status: %s\n",
                temperature, humidity, moisture_percent, status.c_str());

  // Send to Supabase
  sendLog(temperature, humidity, moisture_percent, status);

  // Check for crop changes periodically
  static unsigned long last_fetch = 0;
  if (millis() - last_fetch > 30000) { // Every 30 seconds
    fetchOptimumConditions();
    last_fetch = millis();
  }

  delay(LOG_INTERVAL);
}

// ------------------------
// HELPER FUNCTIONS
// ------------------------

// Fetch latest optimum_conditions from Supabase
void fetchOptimumConditions() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(SUPA_URL) + "/optimum_conditions?garden_id=eq." + GARDEN_ID + "&select=*";
  http.begin(url);
  http.addHeader("apikey", SUPA_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPA_KEY);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Optimum conditions fetched: " + payload);

    // Parse JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    if (doc.size() > 0) {
      JsonObject obj = doc[0];
      crop_name = String(obj["crop_name"].as<const char*>());
      opt_temp = float(obj["optimum_temp"]);
      opt_hum = float(obj["optimum_hum"]);
      opt_moist = float(obj["optimum_moist"]);
      total_days = int(obj["total_days"]);
      start_date = String(obj["start_date"].as<const char*>());
    }
  }
  http.end();
}

// Compute days remaining
int computeRemainingDays() {
  // Convert start_date string to epoch
  struct tm tm;
  strptime(start_date.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm);
  time_t start_epoch = mktime(&tm);
  time_t now_epoch = time(NULL);

  int elapsed = int(difftime(now_epoch, start_epoch) / (24*60*60));
  return max(0, total_days - elapsed);
}

// Determine status message
String determineStatus(float temp, float hum, float moist) {
  String status = "";

  if (temp < opt_temp - 3) status += "temperature low -> enhancing insulation; ";
  else if (temp > opt_temp + 3) status += "temperature high -> fan ON; ";

  if (moist < opt_moist - 3) status += "moisture low -> irrigation ON; ";
  else if (moist > opt_moist + 3) status += "moisture high -> irrigation OFF; ";

  if (hum < opt_hum - 3) status += "humidity low -> reduce ventilation; ";
  else if (hum > opt_hum + 3) status += "humidity high -> increase ventilation; ";

  if (status.length() == 0) status = "within range";
  return status;
}

// Send log to Supabase with retry
void sendLog(float temp, float hum, float moist, String status) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi offline, skipping upload");
    return;
  }

  HTTPClient http;
  String url = String(SUPA_URL) + "/garden_logs";
  http.begin(url);
  http.addHeader("apikey", SUPA_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPA_KEY);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(512);
  doc["garden_id"] = GARDEN_ID;
  doc["crop_name"] = crop_name;
  doc["current_temp"] = temp;
  doc["current_hum"] = hum;
  doc["current_moist"] = moist;
  doc["remaining_days"] = computeRemainingDays();
  doc["status_message"] = status;
  doc["internet_status"] = "online";

  String jsonStr;
  serializeJson(doc, jsonStr);

  int retries = 0;
  bool success = false;
  while (retries < 3 && !success) {
    int httpCode = http.POST(jsonStr);
    if (httpCode == 201 || httpCode == 200) {
      success = true;
      Serial.println("Log uploaded successfully");
    } else {
      Serial.printf("Upload failed (%d). Retrying in %d sec...\n", httpCode, INTERNET_RETRY/1000);
      delay(INTERNET_RETRY);
      retries++;
    }
  }
  http.end();
}
