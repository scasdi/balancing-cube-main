#ifndef ESP_STATE_MACHINE_H
#define ESP_STATE_MACHINE_H

#include <stdint.h>

// Operating states. The control task owns the current value; nothing else
// writes it. Scoped enum so these names cannot collide with the Arduino and
// ESP-IDF macros that occupy the global namespace, and so a State can never
// be silently compared against an int or against FaultCode below.
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

extern State current_state;

const char* state_name(State s);
const char* fault_name(FaultCode f);

// Latest fault reason. FaultCode::None whenever state is not Fault.
FaultCode current_fault();

// Request a state change. Ignored while latched in Fault; the only way out of
// Fault is clear_fault().
void set_state(State new_state);

// Enter Fault with a reason and report it. Zero torque, latched.
void latch_fault(FaultCode reason);

// Leave Fault and return to Idle. Does nothing in any other state.
void clear_fault();

float get_shared_pitch_rad();
void  init_controller();

#endif // ESP_STATE_MACHINE_H
