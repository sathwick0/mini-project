#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// change this to your Render URL before flashing each board
const char* SERVER = "https://your-app-name.onrender.com";

// LDR threshold: above this value = light = casing opened
// calibrate this per scale (see README)
#define LIGHT 500

// chip_id is set from hardware at boot - unique for every ESP8266
String chip_id;
bool is_open;

// posts this ESP's unique chip id to the server as a tamper alert
// the server matches chip_id to the registered scale in the database
void send_alert() {
  WiFiClientSecure client;
  client.setInsecure();  // skip cert check, acceptable for a college project
  HTTPClient http;
  String url = String(SERVER) + "/alert";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  String body = "{\"esp_id\":\"" + chip_id + "\"}";
  int code = http.POST(body);
  http.end();
  // retry once if the first attempt fails (e.g. temporary network drop)
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

  // WiFiManager: on first boot this ESP opens a hotspot called "SmartScale"
  // connect your phone to "SmartScale" and enter the local WiFi name + password
  // the ESP saves the credentials and connects automatically on every boot after that
  // NO WiFi credentials are hardcoded - each board connects to its own local network
  WiFiManager wm;
  wm.autoConnect("SmartScale");

  // read the unique hardware chip id burned into this ESP8266 at the factory
  // every board has a different id - copy this from Serial Monitor and paste
  // it into the dashboard when adding this scale
  chip_id = String(ESP.getChipId(), HEX);
  Serial.println("=== SmartScaleGuard ===");
  Serial.println("ESP Chip ID: " + chip_id);
  Serial.println("Copy this ID into the dashboard -> Add Scale -> ESP ID field");
  Serial.println("Server: " + String(SERVER));

  // read the LDR now so a board booted in light does not fire a false alert
  is_open = analogRead(A0) > LIGHT;
}

void loop() {
  int val = analogRead(A0);
  bool now_open = (val > LIGHT);

  // only fire when state changes from closed (dark) to open (light)
  // not on every loop, not when already open - one alert per opening event
  if (now_open && !is_open) {
    send_alert();
  }

  is_open = now_open;
  delay(200);  // check every 200 ms
}
