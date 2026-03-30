// ================================================================
// ESP32 Traffic Light → Supabase (REST API + polling)
//
// ⚠ IMPORTANT: HTML dashboard ug ESP32 SAME ang Supabase URL/KEY
//
// Required Libraries (install via Arduino Library Manager):
//   ArduinoJson  →  by Benoit Blanchon  (v7 recommended)
//   HTTPClient   →  built-in with ESP32 Arduino core
// ================================================================
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ----------------------------------------------------------------
// Pin Definitions  (match sa imong breadboard wiring)
// ----------------------------------------------------------------
#define PIN_RED    25   // → RED LED    → 220Ω → GND
#define PIN_YELLOW 26   // → YELLOW LED → 220Ω → GND
#define PIN_GREEN  27   // → GREEN LED  → 220Ω → GND

// ----------------------------------------------------------------
// WiFi credentials
// ----------------------------------------------------------------
#define WIFI_SSID     "POCO X6 Pro 5G"
#define WIFI_PASSWORD "pocox6pro5g"

// ----------------------------------------------------------------
// ⚠ Supabase credentials — MUST MATCH ang HTML dashboard
// ----------------------------------------------------------------
#define SUPA_URL   "https://lcaphxnedhlktrtyghkr.supabase.co"
#define SUPA_KEY   "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImxjYXBoeG5lZGhsa3RydHlnaGtyIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzQ4NTE0NzAsImV4cCI6MjA5MDQyNzQ3MH0.6xyFUqhqjS4tghmvrD7zDho81JaJ-Ygs_ON42JokcsA"
#define SUPA_TABLE "traffic_light"
#define SUPA_ROW   "id=eq.1"

// How often to poll Supabase (ms)
#define POLL_INTERVAL 2000

// ----------------------------------------------------------------
// State
// ----------------------------------------------------------------
String        currentSignal = "red";
String        currentMode   = "auto";
int           durRed        = 30;
int           durYellow     = 5;
int           durGreen      = 25;
int           autoPhase     = 0;   // 0=red 1=yellow 2=green
unsigned long phaseStart    = 0;
unsigned long lastPoll      = 0;

const char* phaseNames[] = {"red", "yellow", "green"};

// ----------------------------------------------------------------
// Apply signal to LEDs
// ----------------------------------------------------------------
void applySignal(const String& signal) {
  digitalWrite(PIN_RED,    signal == "red"    ? HIGH : LOW);
  digitalWrite(PIN_YELLOW, signal == "yellow" ? HIGH : LOW);
  digitalWrite(PIN_GREEN,  signal == "green"  ? HIGH : LOW);
  Serial.println("[LED] Signal → " + signal);
}

int phaseDuration(int phase) {
  if (phase == 0) return durRed;
  if (phase == 1) return durYellow;
  return durGreen;
}

// ----------------------------------------------------------------
// Supabase PATCH — FIX: use ArduinoJson to build body safely
// ----------------------------------------------------------------
bool supaPatch(JsonDocument& body) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Not connected — skipping PATCH");
    return false;
  }

  String jsonStr;
  serializeJson(body, jsonStr);

  HTTPClient http;
  String url = String(SUPA_URL) + "/rest/v1/" + SUPA_TABLE + "?" + SUPA_ROW;
  http.begin(url);
  http.addHeader("Content-Type",  "application/json");
  http.addHeader("apikey",        SUPA_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPA_KEY));
  http.addHeader("Prefer",        "return=minimal");

  int code = http.PATCH(jsonStr);
  bool ok  = (code == 200 || code == 204);
  if (ok)  Serial.println("[Supabase] PATCH OK (" + String(code) + ") → " + jsonStr);
  else     Serial.println("[Supabase] PATCH FAILED (" + String(code) + ")");
  http.end();
  return ok;
}

// ----------------------------------------------------------------
// Supabase GET
// ----------------------------------------------------------------
bool supaGet(JsonDocument& doc) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String url = String(SUPA_URL) + "/rest/v1/" + SUPA_TABLE
               + "?" + SUPA_ROW + "&limit=1";
  http.begin(url);
  http.addHeader("apikey",        SUPA_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPA_KEY));
  http.addHeader("Accept",        "application/json");

  int code = http.GET();
  if (code != 200) {
    Serial.println("[Supabase] GET FAILED (" + String(code) + ")");
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  // Supabase returns an array even for 1 row
  JsonDocument arr;
  DeserializationError err = deserializeJson(arr, payload);
  if (err || !arr.is<JsonArray>() || arr.as<JsonArray>().size() == 0) {
    Serial.println("[Supabase] JSON parse error: " + String(err.c_str()));
    return false;
  }

  doc.set(arr[0]);
  Serial.println("[Supabase] GET OK");
  return true;
}

// ================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n=== OOO Traffic Light System ===");

  // Init LEDs
  pinMode(PIN_RED,    OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_GREEN,  OUTPUT);

  // Boot blink test — all 3 light up then off
  Serial.println("[INIT] LED blink test...");
  digitalWrite(PIN_RED,    HIGH); delay(200);
  digitalWrite(PIN_YELLOW, HIGH); delay(200);
  digitalWrite(PIN_GREEN,  HIGH); delay(200);
  digitalWrite(PIN_RED,    LOW);
  digitalWrite(PIN_YELLOW, LOW);
  digitalWrite(PIN_GREEN,  LOW);
  delay(300);

  // Connect WiFi
  Serial.print("[WiFi] Connecting to: "); Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500); Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] FAILED — check SSID/password");
    // Continue anyway — will retry in loop
  }

  // Write initial state to Supabase
  JsonDocument initBody;
  initBody["signal"]       = currentSignal;
  initBody["mode"]         = currentMode;
  initBody["dur_red"]      = durRed;
  initBody["dur_yellow"]   = durYellow;
  initBody["dur_green"]    = durGreen;
  initBody["esp32_online"] = true;
  initBody["ip"]           = WiFi.localIP().toString();
  initBody["uptime"]       = 0;

  if (supaPatch(initBody)) {
    Serial.println("[INIT] Supabase init OK");
  } else {
    Serial.println("[INIT] Supabase init FAILED — check table/key/network");
  }

  applySignal(currentSignal);
  phaseStart = millis();
  Serial.println("[INIT] System ready!\n");
}

// ================================================================
void loop() {

  // ── Poll Supabase for commands ────────────────────────────
  if (millis() - lastPoll >= POLL_INTERVAL) {
    lastPoll = millis();

    JsonDocument row;
    if (supaGet(row)) {

      // Sync mode
      if (!row["mode"].isNull()) {
        String newMode = row["mode"].as<String>();
        if (newMode != currentMode) {
          Serial.println("[Mode] Changed: " + currentMode + " → " + newMode);
          currentMode = newMode;
          // Reset phase timer on mode switch
          phaseStart = millis();
        }
      }

      // Sync durations
      if (!row["dur_red"].isNull())    durRed    = row["dur_red"].as<int>();
      if (!row["dur_yellow"].isNull()) durYellow = row["dur_yellow"].as<int>();
      if (!row["dur_green"].isNull())  durGreen  = row["dur_green"].as<int>();

      // Apply manual signal ONLY in manual mode
      if (currentMode == "manual" && !row["signal"].isNull()) {
        String demanded = row["signal"].as<String>();
        if (demanded != currentSignal) {
          Serial.println("[Manual] Signal demanded: " + demanded);
          currentSignal = demanded;
          applySignal(currentSignal);
        }
      }

    } else {
      Serial.println("[Supabase] Poll failed");
    }
  }

  // ── Auto cycle ────────────────────────────────────────────
  if (currentMode == "auto") {
    unsigned long elapsed = (millis() - phaseStart) / 1000;
    if (elapsed >= (unsigned long)phaseDuration(autoPhase)) {
      autoPhase     = (autoPhase + 1) % 3;
      phaseStart    = millis();
      currentSignal = phaseNames[autoPhase];
      applySignal(currentSignal);

      // Push new signal to dashboard
      JsonDocument sigBody;
      sigBody["signal"] = currentSignal;
      supaPatch(sigBody);
    }
  }

  // ── Heartbeat every 10s ───────────────────────────────────
  static unsigned long lastHB = 0;
  if (millis() - lastHB > 10000) {
    lastHB = millis();
    JsonDocument hb;
    hb["uptime"]       = (int)(millis() / 1000);
    hb["esp32_online"] = true;
    hb["ip"]           = WiFi.localIP().toString();
    supaPatch(hb);
  }

  // ── WiFi watchdog ─────────────────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected — reconnecting...");
    WiFi.reconnect();
    delay(3000);
  }
}
