#pragma once
#include "app/states.h"
#include "app/types.h"

// Result of advancing the machine by one cycle.
struct Transition {
    State     next;
    FaultCode fault;    // Meaningful only when next == State::Fault.
    bool      changed;  // True when next differs from the state passed in.
};

// Advance the state machine by one control cycle.
//
// Pure function: no globals, no hardware, no side effects, no clock reads.
// Everything it needs arrives in the arguments. That is what lets the whole
// transition table be exercised by unit tests on a host machine, with no ESP32
// and no motor.
//
// `ev` carries only what the loop cannot see for itself: operator commands and
// the completion of a sub-process. Falls, overspeed and stale samples are
// derived here from `in` and `lim`.
//
// `now_ms` is passed in rather than read, so tests can drive time directly.
// It is used only for the spindown timeout.
Transition state_step(State     current,
                      Event     ev,
                      const ControlInput& in,
                      const Limits&       lim,
                      uint32_t  now_ms,
                      uint32_t  state_entered_ms);

// Torque policy for a state. Kept next to the transition logic so that the
// answer to "can this state move the motor?" is visible in one file.
//   Balance  - torque comes from the control law
//   Spindown - rate-limited braking
//   SysId    - torque comes from the identification test
//   all else - zero
bool state_allows_torque(State s);
