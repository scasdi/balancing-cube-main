#include "ESP/controller.h"
#include "components/imu_sensor.h"
#include "ESP/LQR.h"
#include <Arduino.h>

cube_state current_state = IDLE;
TaskHandle_t ControlTaskHandle = NULL;

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

void control_loop_task(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(20);

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        switch (current_state) {
            case IDLE:
                break;

            case EDGE_BALANCE: {
                imu_data_t imu_raw = get_imu_data();
                
                double theta     = static_cast<double>(imu_raw.pitch);
                double theta_dot = static_cast<double>(imu_raw.pitch_rate);
                double psi       = 0.0; 
                double psi_dot   = 0.0; 

                control_loop(theta, theta_dot, psi, psi_dot);
                break;
            }

            case JUMP_TO_EDGE:
                break;

            case JUMP_TO_CORNER:
                break;

            case CORNER_BALANCE:
                break;

            case FALL_TO_EDGE:
                break;

            case FALL_TO_IDLE:
                break;
        }
    }
}

void init_controller() {     
    xTaskCreatePinnedToCore(
        control_loop_task,
        "ControlLoopTask",
        4096,
        NULL,
        3,
        &ControlTaskHandle,
        1
    );
}