#pragma once

void lqr_init();
void lqr_set_gains(float k1, float k2, float k3);
float calculate_lqr_torque(float theta_b, float theta_b_dot, float theta_w_dot);