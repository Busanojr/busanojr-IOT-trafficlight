#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WIFI
const char* WIFI_SSID     = "POCO X6 Pro 5G";
const char* WIFI_PASSWORD = "pocox6pro5g";

// SUPABASE
const char* SUPABASE_URL = "https://dcptcsrznhhcokvywwoe.supabase.co";
const char* SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImRjcHRjc3J6bmhoY29rdnl3d29lIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzU2NDUwMzIsImV4cCI6MjA5MTIyMTAzMn0.b7HDKYwkczdTn7_ZC229Jt-CaundUrWEx8gaCieVzjE";

// SETTINGS
#define LED_PIN        2
#define POLL_INTERVAL  3000

unsigned long lastPollTime = 0;

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("ESP32 Supabase LED Controller");

  connectToWiFi();
}

// ================= LOOP =================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Reconnecting...");
    connectToWiFi();
    return;
  }

  unsigned long now = millis();
  if (now - lastPollTime >= POLL_INTERVAL) {
    lastPollTime = now;
    fetchLedStatus();
  }
}

// ================= WIFI CONNECT =================
void connectToWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.disconnect(true);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Failed!");
  }
}

// ================= FETCH FROM SUPABASE =================
void fetchLedStatus() {
  WiFiClientSecure client;
  client.setInsecure(); // allow HTTPS

  HTTPClient http;

  String endpoint = String(SUPABASE_URL) +
    "/rest/v1/device_control?id=eq.1&select=led_status";

  http.begin(client, endpoint);

  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();

    Serial.print("[Response] ");
    Serial.println(payload);

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      bool ledStatus = doc[0]["led_status"] | false;

      digitalWrite(LED_PIN, ledStatus ? HIGH : LOW);

      Serial.println(ledStatus ? "[LED] ON" : "[LED] OFF");
    } else {
      Serial.println("[ERROR] JSON Parse Failed");
    }

  } else {
    Serial.print("[HTTP ERROR] ");
    Serial.println(httpCode);
  }

  http.end();
}