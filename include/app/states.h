#pragma once
#include <stdint.h>

// Operating states of the cube.
// The control task owns this value exclusively; nothing else writes it.
enum class State : uint8_t {
    Init,       // Hardware bring-up. Entered once at boot.
    Idle,       // Safe. Zero torque. Accepts commands.
    Calibrate,  // Capturing the tilt zero offset. Body must be held still.
    SysId,      // Running a parameter identification test. Exits on its own.
    Balance,    // Control loop live.
    Spindown,   // Rate-limited braking of the wheel back to rest.
    Fault,      // Latched. Zero torque until explicitly cleared.
};

// Things that arrive from outside the control loop, delivered as a queue.
// Conditions the loop can see for itself - a fall, an overspeed, a stale
// sample - are NOT events; they are derived from the inputs each cycle.
enum class Event : uint8_t {
    None,
    CmdCalibrate,
    CmdStartSysId,
    CmdBalance,
    CmdStop,
    CmdClearFault,
    InitOk,          // Posted by the control task once hardware is up
    InitFailed,      // Posted by the control task if bring-up failed
    CalibrateDone,   // Offset captured
    SysIdDone,       // Identification test finished
};

// Why the machine latched into Fault. Sent over comms the moment it latches,
// so a fault is always debuggable after the fact.
enum class FaultCode : uint8_t {
    None,
    ImuInitFailed,
    ImuStale,            // No fresh sample within the allowed age
    DeadlineMissed,      // Control cycle overran its period
    TiltOutOfRange,      // Angle beyond anything physically believable
    WheelOverspeed,
    SpindownTimeout,     // Wheel did not come to rest in time
    DriverTimeout,       // Motor driver stopped reporting
    DriverReportedFault, // Motor driver raised its own fault
    BusUndervoltage,
    BusOvervoltage,
};

const char* state_name(State s);
const char* fault_name(FaultCode f);
const char* event_name(Event e);
