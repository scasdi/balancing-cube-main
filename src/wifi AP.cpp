#include <WiFi.h>
#include <WiFiUdp.h>
#include "wifi AP.h"

const char* ssid = "um... banana?";
const char* password = "yes banana";

WiFiUDP udp;
const int udp_port = 1234; // הפורט שדרכו נדבר

void init_wifi_AP() {
    Serial.println("Initializing WiFi Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    Serial.println("Wi-Fi AP Started successfully!");
    
    // הפעלת שרת ה-UDP מיד אחרי שהרשת עלתה
    udp.begin(udp_port);
    Serial.printf("Listening for UDP packets on port %d\n", udp_port);
}

void check_wifi_commands() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        char incomingPacket[255];
        int len = udp.read(incomingPacket, 255);
        if (len > 0) {
            incomingPacket[len] = '\0'; // סגירת המחרוזת
        }
        
        String command = String(incomingPacket);
        command.trim(); // מנקה רווחים וירידות שורה

        // 1. כותבים את כתובת הנמען לתשובה (הלפטופ שלך)
        udp.beginPacket(udp.remoteIP(), udp.remotePort());

        // 2. בודקים מה הייתה הפקודה ומגיבים בהתאם
        if (command == "HELLO") {
            udp.print("banana confirmed sir.");
        } 
        else if (command == "LED_ON") {
            digitalWrite(LED, HIGH);
            udp.print("ESP32: LED is ON_OK");
        } 
        else if (command == "LED_OFF") {
            digitalWrite(LED, LOW);
            udp.print("ESP32: LED is OFF_OK");
        } 
        else {
            udp.print("ESP32: UNKNOWN_CMD");
        }

        // 3. סוגרים את ה"מעטפה" ושולחים באוויר
        udp.endPacket(); 
    }
}