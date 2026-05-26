#include "controller.h"

cube_state current_state = IDLE;
hw_timer_t * timer = NULL;


void set_state(cube_state new_state) {
    current_state = new_state;
    Serial.print("State changed to: ");
    switch (current_state) {
        case IDLE:           Serial.println("IDLE"); break;
        case EDGE_BALANCE:   Serial.println("EDGE_BALANCE"); break;
        case CORNER_BALANCE: Serial.println("CORNER_BALANCE"); break;
        case JUMP_TO_EDGE:   Serial.println("JUMP_TO_EDGE"); break;
        case JUMP_TO_CORNER: Serial.println("JUMP_TO_CORNER"); break;
        case FALL_TO_EDGE:   Serial.println("FALL_TO_EDGE"); break;
        case FALL_TO_IDLE:   Serial.println("FALL_TO_IDLE"); break;
    }
}

void IRAM_ATTR control_loop_ISR() {
    switch (current_state) {
        case IDLE:
            // Implement idle control logic here
            break;
        case EDGE_BALANCE:
            // Implement edge balancing control logic here
            break;
        case JUMP_TO_EDGE:
            // Implement jump to edge control logic here
            break;
        case JUMP_TO_CORNER:
            // Implement jump to corner control logic here
            break;
        case CORNER_BALANCE:
            // Implement corner balancing control logic here
            break;
        case FALL_TO_EDGE:
            // Implement fall to edge control logic here
            break;
        case FALL_TO_IDLE:
            // Implement fall to idle control logic here
            break;
    }
}


void init_controller() {     
    timerBegin(0, 80, true); // Timer 0, prescaler 80 (1 tick = 1 microsecond)

    timerAttachInterrupt(0, &control_loop_ISR, true); // Attach the ISR to the timer

    timerAlarmWrite(0, 2000, true); // Set the timer to trigger every 2 ms

    timerAlarmEnable(0); // Enable the timer alarm
}