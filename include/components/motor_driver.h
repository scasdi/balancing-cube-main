#pragma once

void motor_driver_init();
float get_motor_velocity();
void command_motor_torque(float torque_nm);