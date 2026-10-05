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

// Called every control cycle, so it must have a bounded duration.
// Serial2.print(float) formats through a String, which allocates from the heap;
// heap operations have no bounded worst case and must not run in the loop. This
// formats into a stack buffer and issues one write instead.
void command_motor_torque(float torque_nm) {
    char      buf[24];
    const int len = snprintf(buf, sizeof(buf), "T%.4f\n", torque_nm);
    if (len <= 0 || len >= (int)sizeof(buf)) return;

    // The UART drains at the baud rate regardless of what the driver does: no
    // hardware flow control is configured, so TX cannot be held off. A ~12 byte
    // message every 20 ms is under 6% of 115200 baud against a 128 byte FIFO,
    // so this guard should never fire. If it ever does, the line is not
    // draining and that is a finding, not back-pressure to wait on.
    if (Serial2.availableForWrite() < len) return;

    Serial2.write((const uint8_t*)buf, (size_t)len);
}

// Not implemented: the driver does not report wheel speed over the UART link
// yet. Returning a constant means the LQR's third state is always zero and
// Spindown cannot detect that the wheel has stopped. Wiring this up is a
// prerequisite for Balance, not an optimisation.
float get_motor_velocity() {
    return 0.0f;
}