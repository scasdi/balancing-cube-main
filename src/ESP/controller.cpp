#include "ESP/controller.h"
#include "components/imu_sensor.h"
#include "components/motor_driver.h"
#include "ESP/LQR.h"
#include "ESP/comms.h"
#include <Arduino.h>

cube_state current_state = IDLE;
TaskHandle_t ControlTaskHandle = NULL;

// Thread-safe telemetry variable
static float shared_pitch_rad = 0.0f;
static portMUX_TYPE telemetryMux = vPortMUX_INITIALIZER_UNLOCKED;

// ~15 degrees in radians for fall detection
static const float FALL_THRESHOLD_RAD = 15.0f * (PI / 180.0f); 

void set_state(cube_state new_state) {
    if (current_state == new_state) return;
    current_state = new_state;
    
    switch (current_state) {
        case IDLE:           send_comm_message("STATE: IDLE"); break;
        case EDGE_BALANCE:   send_comm_message("STATE: EDGE_BALANCE"); break;
        case CORNER_BALANCE: send_comm_message("STATE: CORNER_BALANCE"); break;
        case FALL_TO_IDLE:   send_comm_message("STATE: FALL_TO_IDLE"); break;
        default: break;
    }
    
    if (current_state == FALL_TO_IDLE) {
        send_comm_message("WARNING: Fall detected! Disabling motor.");
        command_motor_torque(0.0f);
    }
    
    if (current_state == EDGE_BALANCE || current_state == CORNER_BALANCE) {
        lqr_init();
    }
}

// Thread-safe getter for the terminal/wifi to use
float get_shared_pitch_rad() {
    float val;
    taskENTER_CRITICAL(&telemetryMux);
    val = shared_pitch_rad;
    taskEXIT_CRITICAL(&telemetryMux);
    return val;
}

void control_loop_task(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50Hz rigid timing

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // 1. Exclusive IMU read in the real-time loop
        imu_data_t imu_raw = get_imu_data();
        
        // Update shared variable safely (takes just a few CPU cycles)
        taskENTER_CRITICAL(&telemetryMux);
        shared_pitch_rad = imu_raw.pitch;
        taskEXIT_CRITICAL(&telemetryMux);

        float theta_b     = imu_raw.pitch;
        float theta_b_dot = imu_raw.pitch_rate;

        // 2. Fall detection
        if (current_state == EDGE_BALANCE) {
            if (abs(theta_b) > FALL_THRESHOLD_RAD) {
                set_state(FALL_TO_IDLE);
            }
        }

        // 3. State Machine
        switch (current_state) {
            case IDLE:
            case FALL_TO_IDLE:
                command_motor_torque(0.0f);
                break;

            case EDGE_BALANCE: {
                // Get the wheel velocity from the isolated motor driver interface
                float theta_w_dot = get_motor_velocity(); 

                // Calculate required LQR torque
                float torque_req = calculate_lqr_torque(theta_b, theta_b_dot, theta_w_dot);
                
                // Send it to the driver
                command_motor_torque(torque_req);
                break;
            }

            default:
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
        3, // High Priority
        &ControlTaskHandle,
        1  // Core 1
    );
}