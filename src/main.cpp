#include <Arduino.h>
#include <ESP32Servo.h> // הספרייה של הסרוו
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
static constexpr unsigned long BUTTON_DEBOUNCE_MS = 50u;

// ==========================================
// משתני תזמון עבור הדפסת נתוני החיישן
// ==========================================
static unsigned long lastImuPrintMillis = 0;
static constexpr unsigned long IMU_PRINT_INTERVAL_MS = 50u; // הדפסה כל 50 מילישניות (20 הרץ) כדי לא להציף את התקשורת

// ==========================================
// משתני תזמון עבור מנוע הסרוו
// ==========================================
Servo myServo;
const int servoPin = 13;          // פין בטוח לשימוש (לא פין 1 שמתנגש עם ה-Serial)
int servoPos = 0;                 // זווית נוכחית
int servoDirection = 1;           // כיוון תנועה: 1 (למעלה) או -1 (למטה)
unsigned long lastServoTime = 0;  // שמירת הזמן האחרון שהסרוו זז
bool isServoPaused = false;       // דגל שמעיד אם אנחנו בהשהיה בקצוות (0 או 180)


void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for Serial to initialize
  
  init_pins();
  
  // ==========================================
  // אתחול מנוע הסרוו
  // ==========================================
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2500);
  
  // מדליקים את רשת ה-Wi-Fi (פותחים את פורט ה-UDP להאזנה בתוכה)
  init_wifi_AP();

  // אתחול החיישן (הורדנו את ההערות!)
  if (!init_IMU()) {
    Serial.println("CRITICAL ERROR: IMU initialization failed!");
    // בשלב הפיתוח אפשר לתת למערכת להמשיך לרוץ גם אם נכשל, או לתקוע אותה כאן עם while(1)
  }
}

void loop() {
  unsigned long currentMillis = millis(); // לקיחת דגימת זמן אחת בתחילת הלולאה

  // ==========================================
  // 0. הפעלת הסרוו (Non-Blocking)
  // ==========================================
  if (isServoPaused) {
    // אם הגענו לקצה (0 או 180), מחכים 500 מילישניות
    if (currentMillis - lastServoTime >= 500) {
      isServoPaused = false;
      lastServoTime = currentMillis;
    }
  } else {
    // אם לא בהשהיה, מתקדמים מעלה אחת כל 15 מילישניות
    if (currentMillis - lastServoTime >= 15) {
      lastServoTime = currentMillis;
      
      myServo.write(servoPos);
      servoPos += servoDirection;

      // בדיקה אם הגענו לאחד הקצוות כדי להפוך כיוון ולהתחיל השהיה
      if (servoPos >= 180 || servoPos <= 0) {
        servoDirection = -servoDirection; 
        isServoPaused = true;
      }
    }
  }

  // ==========================================
  // 1. האזנה לערוץ הקווי (USB Serial) ולכפתור
  // ==========================================
  bool rawPressed = digitalRead(BUTTON) == LOW;
  if (rawPressed != lastRawPressed) {
    lastDebounceMillis = currentMillis; // עודכן להשתמש ב-currentMillis
  }

  if ((currentMillis - lastDebounceMillis) > BUTTON_DEBOUNCE_MS) {
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
  check_wifi_commands();
  
  // ==========================================
  // 3. קריאת חיישן והדפסה לטרמינל (Telemetry)
  // ==========================================
  float current_pitch = get_pitch_angle();

  if ((currentMillis - lastImuPrintMillis) > IMU_PRINT_INTERVAL_MS) {
    lastImuPrintMillis = currentMillis; // עודכן להשתמש ב-currentMillis
    
    // הדפסה לטרמינל ה-Serial הקווי
    Serial.printf("Pitch: %.2f\n", current_pitch);
    
    /* אופציונלי: אם תרצה לראות את הערכים גם בטרמינל ה-Wi-Fi (UDP) בפייתון,
    תוריד את ההערות משתי השורות הבאות.
    (שים לב שזה עלול לייצר עומס טקסט בטרמינל האלחוטי שלך).
    */
    // char telemetry_msg[32];
    // snprintf(telemetry_msg, sizeof(telemetry_msg), "Pitch: %.2f", current_pitch);
    // send_wifi_message(telemetry_msg);
  }

  // ==========================================
  // כאן בהמשך יישב הקוד של בקרת המנועים
  // ==========================================
}