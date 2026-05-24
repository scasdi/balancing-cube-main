#include "wifi AP.h"

const char* ssid = "um... banana?";
const char* password = "yes banana";

void init_wifi_AP() {
  Serial.println("Initializing WiFi Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("Access Point \"");
  Serial.print(ssid);
  IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    Serial.println("Wi-Fi AP Started successfully!");
  Serial.println("\" initialized successfully.");
}
