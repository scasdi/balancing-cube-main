#include "ESP/LQR.h"

// Dynamic gains array [K_theta, K_theta_dot, K_wheel_dot]
static float K[3] = { 0.0f, 0.0f, 0.0f };

static float x_f = 0.0f;
static const float alpha = 0.02f; 

void lqr_init() {
    x_f = 0.0f;
}

void lqr_set_gains(float k1, float k2, float k3) {
    K[0] = k1;
    K[1] = k2;
    K[2] = k3;
}

float calculate_lqr_torque(float theta_b, float theta_b_dot, float theta_w_dot) {
    // Offset correction filter (tracks mechanical mounting offsets)
    x_f = (1.0f - alpha) * x_f + alpha * theta_b;

    float theta_b_corrected = theta_b - x_f;
    float x[3] = { theta_b_corrected, theta_b_dot, theta_w_dot };

    float u = 0.0f;
    for (int i = 0; i < 3; ++i) {
        u -= K[i] * x[i];
    }

    return u;
}