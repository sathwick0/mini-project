#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

const char* SERVER = "https://your-app.onrender.com";
#define LIGHT 500

String chip_id;
bool is_open;

void send_alert() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(SERVER) + "/alert";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  String body = "{\"esp_id\":\"" + chip_id + "\"}";
  int code = http.POST(body);
  http.end();
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

  WiFiManager wm;
  wm.autoConnect("SmartScale");

  chip_id = String(ESP.getChipId(), HEX);
  Serial.println("ESP Chip ID: " + chip_id);

  is_open = analogRead(A0) > LIGHT;
}

void loop() {
  int val = analogRead(A0);
  bool now_open = (val > LIGHT);

  if (now_open && !is_open) {
    send_alert();
  }

  is_open = now_open;
  delay(200);
}
