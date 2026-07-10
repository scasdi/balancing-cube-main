#pragma once

/**
 * @brief Initializes the hardware interfaces for motor control (PWM and UART).
 */
void motor_driver_init();

/**
 * @brief Sets the motor speed using hardware PWM mapped from percentage.
 * @param percent Target speed percentage (-100.0 to 100.0).
 */
void command_motor_pwm_speed(float percent);

/**
 * @brief Transmits a torque command to the motor driver via UART.
 * @param torque_nm Requested torque in Newton-meters.
 */
void command_motor_torque(float torque_nm);

/**
 * @brief Retrieves the current angular velocity of the motor from the driver.
 * @return Motor velocity in rad/s.
 */
float get_motor_velocity();