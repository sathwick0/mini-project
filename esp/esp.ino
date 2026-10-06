#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

const char* WIFI_SSID = "net raatle";
const char* WIFI_PASS = "123456791";

const char* SERVER    = "https://smartscaleguard.onrender.com"; 

#define LIGHT 500 


String chip_id; 
bool is_open;    


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

 
  connect_wifi();


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
  delay(200); 
}
