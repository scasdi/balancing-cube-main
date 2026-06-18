#pragma once

// Initializes or resets the LQR controller states
void lqr_init();

// Calculates the required motor torque based on the current state vector
// theta_b: Body pitch angle from BNO08x (radians)
// theta_b_dot: Body pitch angular velocity from BNO08x (rad/s)
// theta_w_dot: Wheel angular velocity from motor encoder (rad/s)
// Returns: Commanded motor torque u (Nm)
float calculate_lqr_torque(float theta_b, float theta_b_dot, float theta_w_dot);