#include <iostream>

// The control loop function (runs at your sample rate, e.g., 100Hz)
void control_loop(double theta, double theta_dot, double psi, double psi_dot) {
    // 1. Paste the K values your Python script printed out here
    // Example values assuming your Python script output: K = [-31.62, -5.20, 0.00, -0.31]
    const double K[4] = {-31.6228, -5.2015, 0.0000, -0.3162};

    // 2. State vector from your sensors
    double x[4] = {theta, theta_dot, psi, psi_dot};

    // 3. Target state (we want all zeros to balance upright)
    double x_ref[4] = {0.0, 0.0, 0.0, 0.0};

    // 4. Calculate control input (Motor Torque): u = -K * (x - x_ref)
    double u = 0;
    for (int i = 0; i < 4; ++i) {
        u += -K[i] * (x[i] - x_ref[i]);
    }

    // 5. Send 'u' to your motor driver
    std::cout << "Commanded Motor Torque: " << u << " Nm\n";
}