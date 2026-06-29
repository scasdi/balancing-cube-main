/**
 * @file servo.cpp
 * @brief Implementation of servo motor control for the jump mechanism.
 */
#include "components/servo.h"
#include <ESP32Servo.h>
#include "ESP/pins.h"
#include <Arduino.h>

static Servo myServo;
static const int ANGLE_REST = 0;
static const int ANGLE_JUMP = 180;

static bool isJumped = false;
static unsigned long jumpTime = 0;
static const unsigned long JUMP_RESET_DELAY_MS = 3000;

/**
 * @brief Initializes the servo motor and attaches it to the configured pin.
 */
void servo_init() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2500);
  myServo.write(ANGLE_REST);
}

/**
 * @brief Non-blocking update function to reset the servo after a jump.
 * @param currentMillis Current system time in milliseconds.
 */
void servo_update(unsigned long currentMillis) {
  if (isJumped && (currentMillis - jumpTime >= JUMP_RESET_DELAY_MS)) {
    myServo.write(ANGLE_REST);
    isJumped = false;
    Serial.println("Reset: Servo returned to REST position (0).");
  }
}

/**
 * @brief Triggers the servo jump action if not already jumped.
 */
void servo_jump() {
  if (!isJumped) {
    myServo.write(ANGLE_JUMP);
    isJumped = true;
    jumpTime = millis();
    Serial.println("SNAP! Jump triggered (180). Waiting 3 seconds...");
  }
}

/**
 * @brief Checks if the servo is currently in the jumped state.
 * @return True if jumped, false otherwise.
 */
bool servo_is_jumped() {
  return isJumped;
}