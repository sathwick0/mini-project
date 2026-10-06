#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// ─── CONFIGURE THESE BEFORE FLASHING ──────────────────────────────────────────

const char* WIFI_SSID = "your_wifi_name";       // local wifi near the scale
const char* WIFI_PASS = "your_wifi_password";   // local wifi password

const char* SERVER    = "https://smartscaleguard.onrender.com"; // render url

#define LIGHT 500  // ldr threshold: above this = casing opened (tune after calibration)

// ──────────────────────────────────────────────────────────────────────────────

String chip_id;  // unique id of this esp board, read from hardware
bool is_open;    // tracks the last known state of the casing

// connects to wifi and waits until connected
void connect_wifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  Serial.println("IP: " + WiFi.localIP().toString());
}

// posts this esp's chip id to the server when light is detected
// the server looks up which scale owns this esp id and shows the alert
void send_alert() {
  WiFiClientSecure client;
  client.setInsecure();  // skip tls cert check (ok for college project)
  HTTPClient http;
  String url = String(SERVER) + "/alert";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  String body = "{\"esp_id\":\"" + chip_id + "\"}";
  int code = http.POST(body);
  http.end();
  // retry once on network failure
  if (code < 0) {
    delay(500);
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.POST(body);
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // connect to the local wifi using the credentials above
  connect_wifi();

  // chip id is burned into the hardware - every esp8266 has a different one
  // copy this from serial monitor and paste it into dashboard -> add scale -> esp id
  chip_id = String(ESP.getChipId(), HEX);
  Serial.println("\n=== SmartScaleGuard ===");
  Serial.println("ESP Chip ID : " + chip_id);
  Serial.println("Server      : " + String(SERVER));
  Serial.println("Register this ESP ID in the dashboard under Add Scale.");
  Serial.println("=======================");

  // read ldr once at boot so a board powered on in light does not false-fire
  is_open = analogRead(A0) > LIGHT;
}

void loop() {
  // reconnect if wifi drops
  if (WiFi.status() != WL_CONNECTED) {
    connect_wifi();
  }

  int val = analogRead(A0);
  bool now_open = (val > LIGHT);

  // fire only when casing changes from closed (dark) to open (light)
  if (now_open && !is_open) {
    Serial.println("Tamper detected! Sending alert...");
    send_alert();
  }

  is_open = now_open;
  delay(200);  // poll ldr every 200 ms
}
