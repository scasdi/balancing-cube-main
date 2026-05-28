#include <Arduino.h>
#include "IMU sensor.h"
#include "wifi AP.h"
#include "pins.h"

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for Serial to initialize
  
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  
  // מדליקים את רשת ה-Wi-Fi (ופותחים את פורט ה-UDP להאזנה בתוכה)
  init_wifi_AP();

  // if (!init_IMU()) {
  //   while (1); // Halt if IMU initialization fails
  // }
}

void loop() {
  // ==========================================
  // 1. האזנה לערוץ הקווי (USB Serial)
  // ==========================================
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "LED_ON") {
      digitalWrite(LED, HIGH);
      Serial.println("LED_ON_OK");
    } else if (command == "LED_OFF") {
      digitalWrite(LED, LOW);
      Serial.println("LED_OFF_OK");
    } else if (command.length() > 0) {
      Serial.print("UNKNOWN_CMD: ");
      Serial.println(command);
    }
  }

  // ==========================================
  // 2. האזנה לערוץ האלחוטי (Wi-Fi UDP)
  // ==========================================
  // הפונקציה הזו חייבת להיקרא בכל סיבוב של הלופ!
  // היא בודקת אם הגיעה פקטה באוויר, ואם כן - מפענחת אותה, 
  // מדליקה את הלד, ושולחת תשובה חזרה ללפטופ.
  check_wifi_commands();
  
  // ==========================================
  // כאן בהמשך יישב הקוד של בקרת המנועים
  // ==========================================
}