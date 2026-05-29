#include <Arduino.h>
#include "imu_sensor.h"
#include "wifi_ap.h"
#include "pins.h"
#include "commands.h"
#include "controller.h"

static bool lastPressed = false;
static bool stablePressed = false;
static bool lastRawPressed = false;
static unsigned long lastDebounceMillis = 0;
static constexpr unsigned long BUTTON_DEBOUNCE_MS = 50u;

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for Serial to initialize
  
  init_pins();
  // מדליקים את רשת ה-Wi-Fi (פותחים את פורט ה-UDP להאזנה בתוכה)
  init_wifi_AP();

  // if (!init_IMU()) {
  //   while (1); // Halt if IMU initialization fails
  // }
}

void loop() {
  // ==========================================
  // 1. האזנה לערוץ הקווי (USB Serial)
  // ==========================================
  bool rawPressed = digitalRead(BUTTON) == LOW;
  if (rawPressed != lastRawPressed) {
    lastDebounceMillis = millis();
  }

  if ((millis() - lastDebounceMillis) > BUTTON_DEBOUNCE_MS) {
    if (rawPressed != stablePressed) {
      stablePressed = rawPressed;
      if (stablePressed != lastPressed) {
        const char* cmd = stablePressed ? "ON_O" : "OFF_O";
        String reply = process_command(String(cmd));
        if (send_wifi_message(reply.c_str())) {
          Serial.printf("Wi-Fi: %s sent -> %s\n", cmd, reply.c_str());
        } else {
          Serial.printf("Wi-Fi: no client available for %s\n", cmd);
        }
        lastPressed = stablePressed;
      }
    }
  }
  lastRawPressed = rawPressed;

  digitalWrite(LED_O, stablePressed ? HIGH : LOW);

  if (Serial.available()) {
    String incoming_cmd = Serial.readStringUntil('\n');
    incoming_cmd.trim();
    if (incoming_cmd.length() > 0) {
      String reply = process_command(incoming_cmd);
      Serial.println(reply);
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