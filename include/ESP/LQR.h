#ifndef LQR_H
#define LQR_H

/**
 * @brief Initializes the LQR controller and loads saved gains from storage.
 */
void lqr_init();

/**
 * @brief Updates the LQR gains dynamically in RAM.
 */
void lqr_set_gains(float k1, float k2, float k3);

/**
 * @brief Calculates the required motor torque based on current state variables.
 * @param theta_b Pitch angle of the body (radians).
 * @param theta_b_dot Pitch angular velocity of the body (rad/s).
 * @param theta_w_dot Angular velocity of the reaction wheel (rad/s).
 * @return Commanded motor torque (Nm).
 */
float calculate_lqr_torque(float theta_b, float theta_b_dot, float theta_w_dot);

#endif