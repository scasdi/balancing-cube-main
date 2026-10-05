#ifndef ESP_STATE_MACHINE_H
#define ESP_STATE_MACHINE_H

#include <stdint.h>

// Operating states. The control task is the only writer of the current state;
// every other task asks for a change through request_state() below. Scoped enum
// so these names cannot collide with the Arduino and ESP-IDF macros that occupy
// the global namespace, and so a State can never be compared against an int or
// against FaultCode.
enum class State : uint8_t {
    Init,       // Hardware bring-up. Entered once at boot.
    Idle,       // Safe. Zero torque. Accepts commands.
    Calibrate,  // Capturing the tilt zero offset. Body must be held still.
    SysId,      // Parameter identification test.
    Balance,    // Control loop live.
    Spindown,   // Bringing the wheel to rest after a fall or a stop command.
    Fault,      // Latched. Zero torque until explicitly cleared.
};

// Why the machine latched into Fault. Reported over comms on entry, so a trip
// is always debuggable after the fact.
enum class FaultCode : uint8_t {
    None,
    ImuInitFailed,
    ImuStale,            // No fresh sample within the allowed age
    DeadlineMissed,      // Control cycle overran its period
    TiltOutOfRange,      // Angle beyond anything physically believable
    WheelOverspeed,
    SpindownTimeout,     // Wheel did not come to rest in time
    DriverTimeout,       // Motor driver stopped reporting
    DriverReportedFault,
    BusUndervoltage,
    BusOvervoltage,
};

const char* state_name(State s);
const char* fault_name(FaultCode f);

// --- Reading. Safe from any task on either core. ---
State     get_state();
FaultCode get_fault();            // FaultCode::None whenever state is not Fault
float     get_shared_pitch_rad(); // Offset-corrected body angle, newest cycle
uint32_t  get_worst_cycle_us();   // Longest control cycle seen since boot

// --- Changing. Safe from any task; never blocks. ---
// The control task owns the state and is its only writer. These post a request
// and return immediately; the task applies it at the top of its next cycle, so
// a request takes effect within one control period. They are the only way for
// core 0 to move the machine.
void request_state(State s);
void request_fault(FaultCode reason);
void request_clear_fault();       // Only exit from Fault. Returns to Idle.

// --- Lifecycle ---
void state_machine_init();        // Once, from setup(), on core 0
void state_machine_service();     // Every loop() iteration, on core 0

#endif // ESP_STATE_MACHINE_H
