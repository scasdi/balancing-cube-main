#include "ESP/state_machine.h"
#include "components/imu_sensor.h"
#include "components/motor_driver.h"
#include "ESP/LQR.h"
#include "ESP/comms.h"
#include "ESP/storage.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Task boundary
// ---------------------------------------------------------------------------
// The control task runs at priority 3 pinned to core 1 and must have a
// worst-case execution time we can compute. That rules out any call whose
// duration is decided elsewhere: comms (the WiFi and Bluetooth stacks run on
// core 0 and hold their own locks), NVS writes, and heap allocation.
//
// So the task does only sensing, the control law, and the actuator write. When
// it needs the outside world it posts to a queue and moves on;
// state_machine_service() drains that queue from loop(). Both queue operations
// use a zero timeout, which makes them bounded by construction.
//
// Where the Arduino loop() actually runs: arduino-esp32 pins loopTask to
// core 1 at priority 1 - the same core as this task, not core 0. That is still
// correct here, because priority 3 preempts priority 1: the control task takes
// the core whenever it wakes, and the deferred work is what gets interrupted.
// One consequence survives it, though. Code executes in place from flash, so
// an NVS write disables the instruction cache for every core while it runs.
// Deferring the write stops it blocking inside the loop, but cannot stop it
// stalling the loop. That is why the only write sits at the end of Calibrate,
// a state which commands no torque.
// ---------------------------------------------------------------------------

static const uint32_t CONTROL_PERIOD_MS = 20;
static const uint32_t CONTROL_PERIOD_US = CONTROL_PERIOD_MS * 1000;

static const float FALL_THRESHOLD_RAD = 15.0f * (PI / 180.0f);

static const int CALIB_SAMPLE_COUNT = 100;

// Requests inbound to the control task. Depth 4: a human issuing commands
// cannot outrun a 50 Hz consumer, so this only has to absorb a burst.
static const int REQUEST_QUEUE_DEPTH = 4;

// Events outbound to loop(). Depth 16 covers a fault arriving while a few
// state changes are still unsent.
static const int EVENT_QUEUE_DEPTH = 16;

// What other tasks may ask the control task to do.
enum class ReqKind : uint8_t { EnterState, LatchFault, ClearFault, ResetWorst };

struct ControlRequest {
    ReqKind   kind;
    State     state;   // Valid when kind == EnterState
    FaultCode fault;   // Valid when kind == LatchFault
};

// What the control task asks loop() to do on its behalf.
enum class EventKind : uint8_t {
    StateEntered, FaultLatched, CalibrationDone, SavePitchOffset, DeadlineOverrun
};

struct ControlEvent {
    EventKind kind;
    State     state;       // Valid when kind == StateEntered
    FaultCode fault;       // Valid when kind == FaultLatched
    float     offset_rad;  // Valid when kind == SavePitchOffset
    uint32_t  cycle_us;    // Valid when kind == DeadlineOverrun
};

static QueueHandle_t request_queue = NULL;
static QueueHandle_t event_queue   = NULL;
static TaskHandle_t  control_task_handle = NULL;

// Written only by the control task, read from both cores. volatile is
// sufficient and a lock is not needed: each is a single naturally aligned word
// with exactly one writer, and no reader does a read-modify-write. Publishing a
// pair of values that must agree with each other would need more than this.
static volatile State     current_state    = State::Init;
static volatile FaultCode fault_reason     = FaultCode::None;
static volatile float     shared_pitch_rad = 0.0f;
static volatile uint32_t  worst_cycle_us   = 0;
static volatile State     worst_cycle_state = State::Init;

// Control-task private. No other task touches these.
static float pitch_offset_rad = 0.0f;
static int   calib_samples    = 0;
static float calib_sum        = 0.0f;

// ---------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Readers
// ---------------------------------------------------------------------------

State     get_state()           { return current_state; }
FaultCode get_fault()           { return fault_reason; }
float     get_shared_pitch_rad(){ return shared_pitch_rad; }
uint32_t  get_worst_cycle_us()  { return worst_cycle_us; }
State     get_worst_cycle_state(){ return worst_cycle_state; }

// ---------------------------------------------------------------------------
// Requests from other tasks
// ---------------------------------------------------------------------------

// Zero timeout: a caller must never be made to wait on the control task, and
// the control task must never be made to wait on a caller.
static void post_request(const ControlRequest& req) {
    if (request_queue == NULL) return;
    xQueueSend(request_queue, &req, 0);
}

void request_state(State s) {
    ControlRequest req = { ReqKind::EnterState, s, FaultCode::None };
    post_request(req);
}

void request_fault(FaultCode reason) {
    ControlRequest req = { ReqKind::LatchFault, State::Fault, reason };
    post_request(req);
}

void request_clear_fault() {
    ControlRequest req = { ReqKind::ClearFault, State::Idle, FaultCode::None };
    post_request(req);
}

void request_reset_worst() {
    ControlRequest req = { ReqKind::ResetWorst, State::Init, FaultCode::None };
    post_request(req);
}

// ---------------------------------------------------------------------------
// Control task internals
// ---------------------------------------------------------------------------

// States in which the motor may be driven. Every other state commands zero
// torque on entry and holds it.
static bool state_allows_torque(State s) {
    return s == State::Balance;
}

// A full event queue means loop() is starved. Dropping telemetry is the right
// trade against missing a control cycle, and the deadline monitor below is what
// reports the starvation itself.
static void post_event(const ControlEvent& ev) {
    if (event_queue == NULL) return;
    xQueueSend(event_queue, &ev, 0);
}

static void post_state_event(State s) {
    ControlEvent ev = {};
    ev.kind  = EventKind::StateEntered;
    ev.state = s;
    post_event(ev);
}

// Control task only. Applies a transition immediately.
static void enter_state(State next) {
    if (current_state == State::Fault) return;  // Latched; only a clear leaves
    if (current_state == next) return;

    current_state = next;

    if (next == State::Calibrate) {
        calib_samples = 0;
        calib_sum     = 0.0f;
    }

    if (!state_allows_torque(next)) {
        command_motor_torque(0.0f);
    }

    post_state_event(next);
}

// Control task only. Zeroes torque and latches.
static void do_latch_fault(FaultCode reason) {
    if (current_state == State::Fault) return;

    // Reason before state, so a reader that observes Fault also observes why.
    fault_reason  = reason;
    current_state = State::Fault;
    command_motor_torque(0.0f);

    ControlEvent ev = {};
    ev.kind  = EventKind::FaultLatched;
    ev.fault = reason;
    post_event(ev);
}

static void do_clear_fault() {
    if (current_state != State::Fault) return;

    fault_reason  = FaultCode::None;
    current_state = State::Idle;
    command_motor_torque(0.0f);
    post_state_event(State::Idle);
}

// Drains the inbound queue. Bounded by the queue depth, so one cycle cannot be
// spent servicing requests.
static void apply_requests() {
    ControlRequest req;
    for (int i = 0; i < REQUEST_QUEUE_DEPTH; ++i) {
        if (xQueueReceive(request_queue, &req, 0) != pdTRUE) break;

        switch (req.kind) {
            case ReqKind::EnterState: enter_state(req.state);      break;
            case ReqKind::LatchFault: do_latch_fault(req.fault);   break;
            case ReqKind::ClearFault: do_clear_fault();            break;

            case ReqKind::ResetWorst:
                worst_cycle_us    = 0;
                worst_cycle_state = current_state;
                break;
        }
    }
}

// How long this cycle's work took. An overrun means the loop did not finish
// inside its period, so Ts was not constant - which invalidates the discrete
// LQR the gains were computed for. Unsigned arithmetic makes the micros()
// rollover at ~71 minutes harmless.
static void check_cycle_time(uint32_t cycle_start_us) {
    const uint32_t elapsed_us = micros() - cycle_start_us;
    const bool     new_worst  = elapsed_us > worst_cycle_us;

    // Recording the state alongside the time is what makes the number
    // actionable: a slow cycle in Calibrate and a slow cycle in Balance have
    // completely different causes and consequences.
    if (new_worst) {
        worst_cycle_us    = elapsed_us;
        worst_cycle_state = current_state;
    }
    if (elapsed_us <= CONTROL_PERIOD_US) return;

    // In Balance a late cycle breaks the control law's timing assumption while
    // torque is live, so it is a fault. In every other state no torque is being
    // commanded, so report it and keep running: that report is how bring-up
    // finds a blocking call before it can matter.
    if (current_state == State::Balance) {
        do_latch_fault(FaultCode::DeadlineMissed);
        return;
    }

    if (new_worst) {
        ControlEvent ev = {};
        ev.kind     = EventKind::DeadlineOverrun;
        ev.cycle_us = elapsed_us;
        post_event(ev);
    }
}

static void control_loop_task(void *pvParameters) {
    TickType_t       last_wake = xTaskGetTickCount();
    const TickType_t period    = pdMS_TO_TICKS(CONTROL_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&last_wake, period);

        const uint32_t cycle_start_us = micros();

        apply_requests();

        imu_data_t imu_raw = get_imu_data();

        // Subtract the static offset for all control logic.
        const float theta_b     = imu_raw.pitch - pitch_offset_rad;
        const float theta_b_dot = imu_raw.pitch_rate;

        shared_pitch_rad = theta_b;

        if (current_state == State::Balance && fabsf(theta_b) > FALL_THRESHOLD_RAD) {
            enter_state(State::Spindown);
        }

        switch (current_state) {
            case State::Init:
            case State::Idle:
            case State::SysId:
            case State::Fault:
                command_motor_torque(0.0f);
                break;

            // The wheel is left to coast. Spindown cannot yet detect that it
            // has stopped, because get_motor_velocity() returns a constant; it
            // therefore stays here until commanded out. Wheel telemetry from
            // the driver is what completes this state.
            case State::Spindown:
                command_motor_torque(0.0f);
                break;

            case State::Calibrate: {
                calib_sum += imu_raw.pitch;   // Accumulate raw, un-offset data
                calib_samples++;

                if (calib_samples >= CALIB_SAMPLE_COUNT) {
                    pitch_offset_rad = calib_sum / CALIB_SAMPLE_COUNT;
                    calib_samples    = 0;
                    calib_sum        = 0.0f;

                    // The flash write is handed to loop(): an NVS put can
                    // erase a 4 kB sector, which takes longer than a control
                    // period. It still stalls this loop through the disabled
                    // instruction cache, which is survivable only because
                    // Calibrate commands no torque.
                    ControlEvent save = {};
                    save.kind       = EventKind::SavePitchOffset;
                    save.offset_rad = pitch_offset_rad;
                    post_event(save);

                    ControlEvent done = {};
                    done.kind = EventKind::CalibrationDone;
                    post_event(done);

                    enter_state(State::Idle);
                }
                break;
            }

            case State::Balance: {
                const float theta_w_dot = get_motor_velocity();
                const float torque_req  = calculate_lqr_torque(theta_b, theta_b_dot, theta_w_dot);
                command_motor_torque(torque_req);
                break;
            }
        }

        check_cycle_time(cycle_start_us);
    }
}

// ---------------------------------------------------------------------------
// Lifecycle. Both are called from the Arduino task.
// ---------------------------------------------------------------------------

void state_machine_init() {
    storage_init();
    pitch_offset_rad = storage_load_pitch_offset();
    lqr_init();

    request_queue = xQueueCreate(REQUEST_QUEUE_DEPTH, sizeof(ControlRequest));
    event_queue   = xQueueCreate(EVENT_QUEUE_DEPTH,   sizeof(ControlEvent));

    if (request_queue == NULL || event_queue == NULL) {
        // Without these the control task could neither receive commands nor
        // report a fault. Refuse to start it: no task means no torque.
        Serial.println("FATAL: control queues not created");
        return;
    }

    const bool imu_ok = init_IMU();

    xTaskCreatePinnedToCore(
        control_loop_task, "ControlLoop", 4096, NULL, 3, &control_task_handle, 1
    );

    // Queued here, applied by the task on its first cycle. The task is started
    // either way, so that state and fault reporting still work when bring-up
    // failed.
    if (imu_ok) request_state(State::Idle);
    else        request_fault(FaultCode::ImuInitFailed);
}

// Everything the control task deferred happens here: comms and flash. Runs at
// the Arduino task's priority (1), so the control task preempts it freely.
void state_machine_service() {
    if (event_queue == NULL) return;

    ControlEvent ev;
    char         buf[72];

    // Bounded so a burst cannot monopolise loop(). Anything left is taken on
    // the next iteration.
    for (int i = 0; i < EVENT_QUEUE_DEPTH; ++i) {
        if (xQueueReceive(event_queue, &ev, 0) != pdTRUE) break;

        switch (ev.kind) {
            case EventKind::StateEntered:
                snprintf(buf, sizeof(buf), "STATE: %s", state_name(ev.state));
                send_comm_message(buf);
                break;

            case EventKind::FaultLatched:
                snprintf(buf, sizeof(buf), "FAULT: %s", fault_name(ev.fault));
                send_comm_message(buf);
                break;

            case EventKind::CalibrationDone:
                send_comm_message("CALIBRATION_DONE");
                break;

            case EventKind::SavePitchOffset:
                storage_save_pitch_offset(ev.offset_rad);
                break;

            case EventKind::DeadlineOverrun:
                snprintf(buf, sizeof(buf), "WARN: CYCLE %luus EXCEEDS %luus",
                         (unsigned long)ev.cycle_us, (unsigned long)CONTROL_PERIOD_US);
                send_comm_message(buf);
                break;
        }
    }
}
