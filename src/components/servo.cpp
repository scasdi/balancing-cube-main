#include "servo.h"
#include <ESP32Servo.h>
#include "pins.h"
#include <Arduino.h>

static Servo myServo;
static const int servoPin = 13;
static const int ANGLE_REST = 0;
static const int ANGLE_JUMP = 180;

static bool isJumped = false;
static unsigned long jumpTime = 0;
static const unsigned long JUMP_RESET_DELAY_MS = 3000;

void servo_init() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2500);
  myServo.write(ANGLE_REST);
}

void servo_update(unsigned long currentMillis) {
  if (isJumped && (currentMillis - jumpTime >= JUMP_RESET_DELAY_MS)) {
    myServo.write(ANGLE_REST);
    isJumped = false;
    Serial.println("Reset: Servo returned to REST position (0).");
  }
}

void servo_jump() {
  if (!isJumped) {
    myServo.write(ANGLE_JUMP);
    isJumped = true;
    jumpTime = millis();
    Serial.println("SNAP! Jump triggered (180). Waiting 3 seconds...");
  }
}

bool servo_is_jumped() {
  return isJumped;
}
