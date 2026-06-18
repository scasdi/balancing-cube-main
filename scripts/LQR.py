import numpy as np
import control as ct
import json
import os

CONFIG_FILE = "cube_params.json"

def load_params():
    if not os.path.exists(CONFIG_FILE):
        raise FileNotFoundError(f"Missing {CONFIG_FILE}")
    with open(CONFIG_FILE, 'r') as f:
        return json.load(f)

def save_params(data):
    with open(CONFIG_FILE, 'w') as f:
        json.dump(data, f, indent=4)

def calculate_lqr():
    data = load_params()
    p = data["physical_params"]
    q = data["lqr_penalties"]

    # Extract dynamic parameters
    g, m_b, m_w = p["g"], p["m_b"], p["m_w"]
    l_b, l = p["l_b"], p["l"]
    I_b, I_w = p["I_b"], p["I_w"]
    C_b, C_w = p["C_b"], p["C_w"]
    K_m = p["K_m"]

    # Pre-calculate common denominator terms
    M_term = (m_b * l_b + m_w * l) * g
    I_tot = I_b + m_w * (l ** 2)

    # State matrix A (3x3)
    A = np.array([
        [0, 1, 0],
        [M_term / I_tot, -C_b / I_tot, C_w / I_tot],
        [-M_term / I_tot, C_b / I_tot, -C_w * (I_b + I_w + m_w * l**2) / (I_w * I_tot)]
    ])

    # Input matrix B (3x1)
    B = np.array([
        [0],
        [-K_m / I_tot],
        [K_m * (I_b + I_w + m_w * l**2) / (I_w * I_tot)]
    ])

    # Cost matrices
    Q = np.diag([q["Q_theta"], q["Q_theta_dot"], q["Q_wheel_dot"]])
    R = np.array([[q["R_motor"]]])

    # LQR Calculation
    K, S, E = ct.lqr(A, B, Q, R)

    # Save format
    k_list = [float(K[0][0]), float(K[0][1]), float(K[0][2])]
    data["calculated_gains"]["K"] = k_list
    save_params(data)

    print("\n=== System Updated Successfully ===")
    print(f"Calculated A Matrix:\n{A}")
    print(f"Calculated B Matrix:\n{B}")
    print(f"New K Matrix saved: {k_list}")

if __name__ == "__main__":
    calculate_lqr()