#pragma once
#include <stdint.h>

// Tunable numbers and the data that flows through one control cycle.
// Kept apart from state_machine.h so that "what are the trip points?" and
// "what does the loop see?" can be answered without reading any logic.
//
// Units are SI throughout this project: radians, radians per second, newton
// metres, volts, microseconds. Degrees appear only in telemetry output.

// Everything the state machine and the control law see, sampled once per cycle.
struct ControlInput {
    float    tilt_rad;         // Body angle. Zero = upright, offset removed.
    float    tilt_rate_rads;   // Body angular rate, from the gyro.
    float    wheel_rate_rads;  // Reaction wheel rate, reported by the driver.
    float    bus_volts;        // Battery voltage, reported by the driver.
    uint32_t sample_age_us;    // Age of the newest sensor sample.
    bool     imu_valid;        // False when the IMU has produced no fresh data.
    bool     driver_valid;     // False when the driver has stopped reporting.
};

// What the control cycle hands to the actuator layer.
struct ControlOutput {
    float torque_nm;   // Commanded wheel torque.
    bool  saturated;   // True when the request was clipped by max_torque_nm.
};

// Every threshold compared against by the state machine and the safety checks.
// One struct so the whole trip envelope is reviewable in one place.
//
// Sign convention for tilt_rad and torque_nm is fixed on the bench during
// bring-up and recorded here once measured.
struct Limits {
    float    fall_tilt_rad;        // Beyond this, balancing is abandoned.
    float    max_tilt_rad;         // Beyond this, the reading is not believable.
    float    max_wheel_rate_rads;  // Wheel overspeed trip.
    float    wheel_stopped_rads;   // Below this, spindown is complete.
    float    max_torque_nm;        // Hard clamp on commanded torque.
    uint32_t max_sample_age_us;    // Older than this, the sample is stale.
    uint32_t spindown_timeout_ms;  // Spindown must finish inside this.
};
