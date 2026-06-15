#include <Arduino.h>
#include <ESP32Servo.h>
#include "imu_sensor.h"
#include "wifi_ap.h"
#include "pins.h"
#include "commands.h"
#include "controller.h"
#include "LQR.h"

static bool lastPressed = false;
static bool stablePressed = false;
static bool lastRawPressed = false;
static unsigned long lastDebounceMillis = 0;
static constexpr unsigned long BUTTON_DEBOUNCE_MS = 20u; 

// ==========================================
// משתני תזמון עבור הדפסת נתוני החיישן
// ==========================================
static unsigned long lastImuPrintMillis = 0;
static constexpr unsigned long IMU_PRINT_INTERVAL_MS = 50u;

// ==========================================
// משתני תזמון ושליטה עבור מנוע הסרוו (מנגנון קפיצה)
// ==========================================
Servo myServo;
const int servoPin = 13;          
const int ANGLE_REST = 0;       // זווית מנוחה (פתוח)
const int ANGLE_JUMP = 180;     // זווית קפיצה/חסימה (סגור)

bool isJumped = false;          // דגל ששומר האם אנחנו עכשיו במצב "קפיצה"
unsigned long jumpTime = 0;     // שמירת הרגע שבו התבצעה הקפיצה
const unsigned long JUMP_RESET_DELAY_MS = 3000; // 3 שניות המתנה לפני חזרה

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10); 
  delay(1000); 
  
  init_pins();
  
  // ==========================================
  // אתחול מנוע הסרוו למצב מנוחה
  // ==========================================
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2500);
  
  myServo.write(ANGLE_REST); // מוודאים שהסרוו מתחיל במצב פתוח
  
  init_wifi_AP();

  if (!init_IMU()) {
    Serial.println("CRITICAL ERROR: IMU initialization failed!");
  }
}

void loop() {
  unsigned long currentMillis = millis(); 

  // ==========================================
  // 1. ניהול חזרה אוטומטית של הסרוו (אחרי 3 שניות)
  // ==========================================
  // אם אנחנו במצב קפיצה ועברו 3000 מילישניות - מחזירים לאפס
  if (isJumped && (currentMillis - jumpTime >= JUMP_RESET_DELAY_MS)) {
      myServo.write(ANGLE_REST);
      isJumped = false;
      Serial.println("Reset: Servo returned to REST position (0).");
  }

  // ==========================================
  // 2. האזנה לכפתור והפעלת הקפיצה
  // ==========================================
  bool rawPressed = digitalRead(BUTTON) == LOW;
  if (rawPressed != lastRawPressed) {
    lastDebounceMillis = currentMillis;
  }

  if ((currentMillis - lastDebounceMillis) > BUTTON_DEBOUNCE_MS) {
    if (rawPressed != stablePressed) {
      stablePressed = rawPressed;
      
      // *** זיהוי לחיצה חדשה (Rising Edge) ***
      if (stablePressed == true && lastPressed == false) {
        
        // מוודאים שאנחנו לא כבר באמצע קפיצה כדי לא לאפס את הטיימר סתם
        if (!isJumped) {
            myServo.write(ANGLE_JUMP);  // מכת כוח ליעד!
            isJumped = true;            // מעדכנים שאנחנו באוויר
            jumpTime = currentMillis;   // מתחילים למדוד 3 שניות מעכשיו
            Serial.println("SNAP! Jump triggered (180). Waiting 3 seconds...");
        } else {
            Serial.println("Ignored: Already in jump state.");
        }
      }
      
      if (stablePressed != lastPressed) {
        const char* cmd = stablePressed ? "ON_O" : "OFF_O";
        String reply = process_command(String(cmd));
        if (send_wifi_message(reply.c_str())) {
          // Serial.printf("Wi-Fi: %s sent -> %s\n", cmd, reply.c_str());
        }
        lastPressed = stablePressed;
      }
    }
  }
  lastRawPressed = rawPressed;
  digitalWrite(LED_O, stablePressed ? HIGH : LOW);

  // ==========================================
  // 3. פיענוח פקודות Serial
  // ==========================================
  if (Serial.available()) {
    String incoming_cmd = Serial.readStringUntil('\n');
    incoming_cmd.trim();
    
    if (incoming_cmd.length() > 0) {
      // אם תרצה להפעיל קפיצה גם דרך הטרמינל (ללא כפתור)
      if (incoming_cmd == "JUMP") {
         if (!isJumped) {
            myServo.write(ANGLE_JUMP);
            isJumped = true;
            jumpTime = currentMillis;
            Serial.println("SNAP! Terminal Jump triggered. Waiting 3 seconds...");
         }
      } 
      else {
        String reply = process_command(incoming_cmd);
        Serial.println(reply);
      }
    }
  }

  // ==========================================
  // 4. האזנה לערוץ האלחוטי (Wi-Fi UDP)
  // ==========================================
  check_wifi_commands();
  
  // ==========================================
  // 5. קריאת חיישן והדפסה לטרמינל
  // ==========================================
  float current_pitch = get_pitch_angle();

  if ((currentMillis - lastImuPrintMillis) > IMU_PRINT_INTERVAL_MS) {
    lastImuPrintMillis = currentMillis; 
    Serial.printf("Pitch: %.2f\n", current_pitch);
  }
}