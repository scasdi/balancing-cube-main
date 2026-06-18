#include "ESP/LQR.h"

// The K matrix gains. These values should be updated via JSON 
// after running the Python System ID wizard.
// Current array matches the 3-state vector: [K_theta, K_theta_dot, K_wheel_dot]
static const float K[3] = { -0.5f, -0.1f, -0.01f }; // Default dummy values

// Offset correction filter state (represents the mechanical/mounting offset)
static float x_f = 0.0f;

// Filter coefficient (alpha = 0.02) as proven stable in the IROS 2012 paper
static const float alpha = 0.02f; 

void lqr_init() {
    // Reset the mechanical offset filter when jumping up or starting a balance maneuver
    x_f = 0.0f;
}

float calculate_lqr_torque(float theta_b, float theta_b_dot, float theta_w_dot) {
    // 1. Update the offset correction low-pass filter (Paper Equation 8)
    // This slowly tracks the true mechanical balance point over time
    x_f = (1.0f - alpha) * x_f + alpha * theta_b;

    // 2. Correct the body angle measurement (Paper Equation 9)
    float theta_b_corrected = theta_b - x_f;

    // 3. Assemble the state vector x = (theta_b, theta_b_dot, theta_w_dot)
    float x[3] = { theta_b_corrected, theta_b_dot, theta_w_dot };

    // 4. Calculate control input u = -K * x
    float u = 0.0f;
    for (int i = 0; i < 3; ++i) {
        u -= K[i] * x[i];
    }

    // 5. Return the calculated torque (Nm) to be commanded to the motor driver
    return u;
}