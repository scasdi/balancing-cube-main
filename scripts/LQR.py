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

    # שליפת פרמטרים פיזיקליים
    g, m_f, m_w = p["g"], p["m_f"], p["m_w"]
    l_f, l_w = p["l_f"], p["l_w"]
    I_f, I_w = p["I_f"], p["I_w"]

    # חישובי עזר
    I_tot = I_f + (m_f * l_f**2) + I_w + (m_w * l_w**2)
    C_g = (m_f * l_f + m_w * l_w) * g
    denominator = I_tot - I_w

    # מטריצת מצב
    A = np.array([
        [0, 1, 0, 0],
        [C_g / denominator, 0, 0, 0],
        [0, 0, 0, 1],
        [-C_g / denominator, 0, 0, 0]
    ])

    # וקטור כניסה
    B = np.array([
        [0],
        [-1 / denominator],
        [0],
        [I_tot / (I_w * denominator)]
    ])

    # מטריצות מחיר
    Q = np.diag([q["Q_theta"], q["Q_theta_dot"], q["Q_psi"], q["Q_psi_dot"]])
    R = np.array([[q["R_motor"]]])

    # פתרון ובדיקת יציבות
    K, S, E = ct.lqr(A, B, Q, R)

    # עדכון המערכת ושמירה אוטומטית
    k_list = [float(K[0][0]), float(K[0][1]), float(K[0][2]), float(K[0][3])]
    data["calculated_gains"]["K"] = k_list
    save_params(data)

    print("=== System Updated Successfully ===")
    print(f"New K Matrix saved to JSON: {k_list}")

if __name__ == "__main__":
    calculate_lqr()