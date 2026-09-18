#include "ESP/state_machine.h"
#include "components/imu_sensor.h"
#include "components/motor_driver.h"
#include "ESP/LQR.h"
#include "ESP/comms.h"
#include "ESP/storage.h"
#include <Arduino.h>

State current_state = State::Init;
TaskHandle_t ControlTaskHandle = NULL;

static FaultCode fault_reason = FaultCode::None;

static float shared_pitch_rad  = 0.0f;
static float pitch_offset_rad  = 0.0f;
static portMUX_TYPE telemetryMux = portMUX_INITIALIZER_UNLOCKED;

// Calibration accumulator. File scope so entering Calibrate can reset it;
// as a function-local static it kept a partial sum across aborted attempts.
static int   calib_samples = 0;
static float calib_sum     = 0.0f;
static const int CALIB_SAMPLE_COUNT = 100;

static const float FALL_THRESHOLD_RAD = 15.0f * (PI / 180.0f);

const char* state_name(State s) {
    switch (s) {
        case State::Init:      return "INIT";
        case State::Idle:      return "IDLE";
        case State::Calibrate: return "CALIBRATE";
        case State::SysId:     return "SYSID";
        case State::Balance:   return "BALANCE";
        case State::Spindown:  return "SPINDOWN";
        case State::Fault:     return "FAULT";
    }
    return "?";
}

const char* fault_name(FaultCode f) {
    switch (f) {
        case FaultCode::None:                return "NONE";
        case FaultCode::ImuInitFailed:       return "IMU_INIT_FAILED";
        case FaultCode::ImuStale:            return "IMU_STALE";
        case FaultCode::DeadlineMissed:      return "DEADLINE_MISSED";
        case FaultCode::TiltOutOfRange:      return "TILT_OUT_OF_RANGE";
        case FaultCode::WheelOverspeed:      return "WHEEL_OVERSPEED";
        case FaultCode::SpindownTimeout:     return "SPINDOWN_TIMEOUT";
        case FaultCode::DriverTimeout:       return "DRIVER_TIMEOUT";
        case FaultCode::DriverReportedFault: return "DRIVER_FAULT";
        case FaultCode::BusUndervoltage:     return "BUS_UNDERVOLTAGE";
        case FaultCode::BusOvervoltage:      return "BUS_OVERVOLTAGE";
    }
    return "?";
}

FaultCode current_fault() { return fault_reason; }

// States in which the motor may be driven. Every other state commands zero
// torque on entry and holds it.
static bool state_allows_torque(State s) {
    return s == State::Balance;
}

float get_shared_pitch_rad() {
    float val;
    taskENTER_CRITICAL(&telemetryMux);
    val = shared_pitch_rad;
    taskEXIT_CRITICAL(&telemetryMux);
    return val;
}

void set_state(State new_state) {
    // Fault is latched: only clear_fault() leaves it.
    if (current_state == State::Fault && new_state != State::Fault) return;
    if (current_state == new_state) return;

    current_state = new_state;

    if (new_state == State::Calibrate) {
        calib_samples = 0;
        calib_sum     = 0.0f;
    }

    if (!state_allows_torque(new_state)) {
        command_motor_torque(0.0f);
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "STATE: %s", state_name(new_state));
    send_comm_message(buf);
}

void latch_fault(FaultCode reason) {
    if (current_state == State::Fault) return;

    fault_reason  = reason;
    current_state = State::Fault;
    command_motor_torque(0.0f);

    char buf[64];
    snprintf(buf, sizeof(buf), "FAULT: %s", fault_name(reason));
    send_comm_message(buf);
}

void clear_fault() {
    if (current_state != State::Fault) return;

    fault_reason  = FaultCode::None;
    current_state = State::Idle;
    command_motor_torque(0.0f);
    send_comm_message("STATE: IDLE");
}

void control_loop_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        imu_data_t imu_raw = get_imu_data();

        // Subtract static offset for all control logic
        float theta_b     = imu_raw.pitch - pitch_offset_rad;
        float theta_b_dot = imu_raw.pitch_rate;

        taskENTER_CRITICAL(&telemetryMux);
        shared_pitch_rad = theta_b;
        taskEXIT_CRITICAL(&telemetryMux);

        if (current_state == State::Balance && fabs(theta_b) > FALL_THRESHOLD_RAD) {
            set_state(State::Spindown);
        }

        switch (current_state) {
            case State::Init:
            case State::Idle:
            case State::SysId:
            case State::Fault:
                command_motor_torque(0.0f);
                break;

            // Wheel is left to coast. Spindown cannot yet detect that the
            // wheel has stopped, because get_motor_velocity() returns a
            // constant; it therefore stays here until commanded out. Wiring
            // wheel telemetry from the driver is what completes this state.
            case State::Spindown:
                command_motor_torque(0.0f);
                break;

            case State::Calibrate: {
                calib_sum += imu_raw.pitch;   // Accumulate raw, un-offset data
                calib_samples++;

                if (calib_samples >= CALIB_SAMPLE_COUNT) {
                    pitch_offset_rad = calib_sum / CALIB_SAMPLE_COUNT;
                    storage_save_pitch_offset(pitch_offset_rad);
                    calib_samples = 0;
                    calib_sum     = 0.0f;
                    send_comm_message("CALIBRATION_DONE");
                    set_state(State::Idle);
                }
                break;
            }

            case State::Balance: {
                float theta_w_dot = get_motor_velocity();
                float torque_req  = calculate_lqr_torque(theta_b, theta_b_dot, theta_w_dot);
                command_motor_torque(torque_req);
                break;
            }
        }
    }
}

void init_controller() {
    storage_init();
    pitch_offset_rad = storage_load_pitch_offset();
    lqr_init();

    // The control task is started either way, so that state and fault
    // reporting still work when bring-up failed.
    xTaskCreatePinnedToCore(
        control_loop_task, "ControlLoopTask", 4096, NULL, 3, &ControlTaskHandle, 1
    );

    if (!init_IMU()) {
        latch_fault(FaultCode::ImuInitFailed);
        return;
    }

    set_state(State::Idle);
}
