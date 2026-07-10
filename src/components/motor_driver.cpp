#include "components/motor_driver.h"
#include "ESP/pins.h"
#include <ESP32Servo.h>
#include <Arduino.h>

static Servo esc_pwm;

void motor_driver_init() {
    Serial2.begin(115200, SERIAL_8N1, ESC_RX_PIN, ESC_TX_PIN);
    
    ESP32PWM::allocateTimer(2);
    
    esc_pwm.setPeriodHertz(490);
    esc_pwm.attach(ESC_PWM_PIN, 800, 2000);
    
    esc_pwm.writeMicroseconds(800);
    
    pinMode(ESC_DIR_PIN, OUTPUT);
    digitalWrite(ESC_DIR_PIN, LOW);
}

void command_motor_pwm_speed(float percent) {
    if (percent < 0.0f) {
        digitalWrite(ESC_DIR_PIN, HIGH);
        percent = -percent;
    } else {
        digitalWrite(ESC_DIR_PIN, LOW);
    }
    
    if (percent > 100.0f) {
        percent = 100.0f;
    }

    if (percent == 0.0f) {
        esc_pwm.writeMicroseconds(800);
    } else {
        int pulse_width = 1060 + (int)((percent / 100.0f) * (1860 - 1060));
        esc_pwm.writeMicroseconds(pulse_width);
    }
}

void command_motor_torque(float torque_nm) {
    Serial2.print("T");
    Serial2.println(torque_nm);
}

float get_motor_velocity() {
    return 0.0f;
}