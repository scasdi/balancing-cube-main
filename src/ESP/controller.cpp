#include "ESP/controller.h"
#include "components/imu_sensor.h"
#include "components/motor_driver.h"
#include "ESP/LQR.h"
#include "ESP/comms.h"
#include "ESP/storage.h"
#include <Arduino.h>

cube_state current_state = IDLE;
TaskHandle_t ControlTaskHandle = NULL;

static float shared_pitch_rad = 0.0f;
static float pitch_offset_rad = 0.0f;
static portMUX_TYPE telemetryMux = portMUX_INITIALIZER_UNLOCKED;

static const float FALL_THRESHOLD_RAD = 15.0f * (PI / 180.0f); 

float get_shared_pitch_rad() {
    float val;
    taskENTER_CRITICAL(&telemetryMux);
    val = shared_pitch_rad;
    taskEXIT_CRITICAL(&telemetryMux);
    return val;
}

void set_state(cube_state new_state) {
    if (current_state == new_state) return;
    current_state = new_state;
    
    switch (current_state) {
        case IDLE:           send_comm_message("STATE: IDLE"); break;
        case EDGE_BALANCE:   send_comm_message("STATE: EDGE_BALANCE"); break;
        case FALL_TO_IDLE:   send_comm_message("STATE: FALL_TO_IDLE"); break;
        case SYS_ID_TEST:    send_comm_message("STATE: SYS_ID_TEST"); break;
        case CALIBRATING:    send_comm_message("STATE: CALIBRATING ZERO..."); break;
        default: break;
    }
    
    if (current_state == FALL_TO_IDLE || current_state == SYS_ID_TEST) {
        command_motor_torque(0.0f);
    }
}

void control_loop_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20); 

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        imu_data_t imu_raw = get_imu_data();
        
        // Subtract static offset for all control logic
        float theta_b = imu_raw.pitch - pitch_offset_rad; 
        float theta_b_dot = imu_raw.pitch_rate;

        taskENTER_CRITICAL(&telemetryMux);
        shared_pitch_rad = theta_b;
        taskEXIT_CRITICAL(&telemetryMux);

        if (current_state == EDGE_BALANCE && fabs(theta_b) > FALL_THRESHOLD_RAD) {
            set_state(FALL_TO_IDLE);
        }

        switch (current_state) {
            case IDLE:
            case FALL_TO_IDLE:
            case SYS_ID_TEST:
                command_motor_torque(0.0f);
                break;
                
            case CALIBRATING: {
                static int calib_samples = 0;
                static float calib_sum = 0.0f;
                
                if (calib_samples == 0) calib_sum = 0.0f;
                
                calib_sum += imu_raw.pitch; // Accumulate raw data
                calib_samples++;
                
                if (calib_samples >= 100) { // 100 samples @ 50Hz = 2 seconds
                    pitch_offset_rad = calib_sum / 100.0f;
                    storage_save_pitch_offset(pitch_offset_rad);
                    calib_samples = 0;
                    send_comm_message("CALIBRATION_DONE");
                    set_state(IDLE);
                }
                break;
            }

            case EDGE_BALANCE: {
                float theta_w_dot = get_motor_velocity(); 
                float torque_req = calculate_lqr_torque(theta_b, theta_b_dot, theta_w_dot);
                command_motor_torque(torque_req);
                break;
            }
            default:
                break;
        }
    }
}

void init_controller() {
    storage_init();
    pitch_offset_rad = storage_load_pitch_offset();
    lqr_init();
         
    xTaskCreatePinnedToCore(
        control_loop_task, "ControlLoopTask", 4096, NULL, 3, &ControlTaskHandle, 1 
    );
}