#pragma once

// ==========================================
// Hardware Pin Mapping (ESP32 DOIT DevKit V1)
// ==========================================

// System Indicators
#define LED 22

// I2C Bus (BNO085 IMU)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// Motor A (Example)
#define MOTOR_A_PWM_PIN 15
#define MOTOR_A_DIR_PIN 2

// Encoders (Example)
#define ENCODER_A_PIN 34 // Input Only pin
#define ENCODER_B_PIN 35 // Input Only pin