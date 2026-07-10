#pragma once

static constexpr int SCL_imu = 22;
static constexpr int SDA_imu = 21;
static constexpr int LED_Y = 23;
static constexpr int LED_O = 2;
static constexpr int BUTTON = 36;
static const int servoPin = 13;

static constexpr int ESC_RX_PIN = 16;
static constexpr int ESC_TX_PIN = 17;
static constexpr int ESC_PWM_PIN = 4;
static constexpr int ESC_DIR_PIN = 5;

void init_pins();