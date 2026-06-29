"""!
@file terminal.py
@brief Serial terminal and system ID wizard for the balancing cube.
"""

import os
import time
import json
import threading
import numpy as np
import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import control as ct
from scipy.optimize import curve_fit

## Absolute path to the script directory.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
## Absolute path to the configuration JSON file.
CONFIG_FILE = os.path.join(SCRIPT_DIR, "cube_params.json")

def load_params():
    """!
    @brief Loads the physical and control parameters from the JSON configuration file.
    @return Dictionary containing the configuration data.
    @throws FileNotFoundError If the configuration file does not exist.
    """
    if not os.path.exists(CONFIG_FILE):
        raise FileNotFoundError(f"Missing {CONFIG_FILE}")
    with open(CONFIG_FILE, 'r') as f:
        return json.load(f)

def save_and_sync_params(esp32, data):
    """!
    @brief Saves parameters to the JSON file and transmits the updated gains to the ESP32.
    @param esp32 Active serial connection object.
    @param data Dictionary containing the parameters to save and sync.
    """
    with open(CONFIG_FILE, 'w') as f:
        json.dump(data, f, indent=4)
    
    k = data["calculated_gains"]["K"]
    sync_cmd = f"SET_GAINS:{k[0]},{k[1]},{k[2]}\n"
    if esp32 and esp32.is_open:
        esp32.write(sync_cmd.encode('utf-8'))
        print(f"[PC] Synced to ESP -> {sync_cmd.strip()}")
    else:
        print("[PC] JSON saved, but ESP32 not connected for sync.")

def calculate_lqr_and_sync(esp32):
    """!
    @brief Calculates optimal LQR gains based on physical parameters and penalties, then syncs them.
    @param esp32 Active serial connection object.
    """
    print("\n--- CALCULATING LQR GAINS ---")
    data = load_params()
    p = data["physical_params"]
    q = data["lqr_penalties"]

    g, m_b, m_w = p["g"], p["m_b"], p["m_w"]
    l_b, l = p["l_b"], p["l"]
    I_b, I_w = p["I_b"], p["I_w"]
    C_b, C_w = p["C_b"], p["C_w"]
    K_m = p["K_m"]

    M_term = (m_b * l_b + m_w * l) * g
    I_tot = I_b + m_w * (l ** 2)

    A = np.array([
        [0, 1, 0],
        [M_term / I_tot, -C_b / I_tot, C_w / I_tot],
        [-M_term / I_tot, C_b / I_tot, -C_w * (I_b + I_w + m_w * l**2) / (I_w * I_tot)]
    ])

    B = np.array([
        [0],
        [-K_m / I_tot],
        [K_m * (I_b + I_w + m_w * l**2) / (I_w * I_tot)]
    ])

    Q = np.diag([q["Q_theta"], q["Q_theta_dot"], q["Q_wheel_dot"]])
    R = np.array([[q["R_motor"]]])

    try:
        K, _, _ = ct.lqr(A, B, Q, R)
        k_list = [float(K[0][0]), float(K[0][1]), float(K[0][2])]
        data["calculated_gains"]["K"] = k_list
        save_and_sync_params(esp32, data)
        print(f"LQR Gains updated: {k_list}")
    except Exception as e:
        print(f"[!] LQR Calculation failed: {e}")

def r2_score(y_true, y_pred):
    """!
    @brief Calculates the R-squared statistical measure of fit.
    @param y_true Array of actual observed values.
    @param y_pred Array of predicted values from the model.
    @return Float representing the R-squared score.
    """
    y_true = np.array(y_true)
    y_pred = np.array(y_pred)
    ss_res = np.sum((y_true - y_pred) ** 2)
    ss_tot = np.sum((y_true - np.mean(y_true)) ** 2)
    return 1.0 - ss_res / ss_tot if ss_tot != 0 else 0.0

def damped_sine(t, A, zeta, wn, phi, c):
    """!
    @brief Mathematical model function for a damped harmonic oscillator.
    @param t Time array.
    @param A Initial amplitude.
    @param zeta Damping ratio.
    @param wn Natural frequency.
    @param phi Phase angle.
    @param c Vertical offset.
    @return Array of calculated values matching the time array.
    """
    wd = wn * np.sqrt(1 - zeta**2)
    return A * np.exp(-zeta * wn * t) * np.cos(wd * t + phi) + c

def motor_step_response(t, v_ss, tau):
    """!
    @brief Mathematical model function for a first-order system step response.
    @param t Time array.
    @param v_ss Steady-state velocity.
    @param tau Time constant.
    @return Array of calculated values matching the time array.
    """
    return v_ss * (1 - np.exp(-t / tau))

def run_pendulum_sysid(esp32):
    """!
    @brief Executes the pendulum system identification routine to calculate body inertia and damping.
    @param esp32 Active serial connection object.
    """
    print("\n--- PENDULUM TEST ($I_b$, $C_b$) ---")
    print("Instruction: Hang the body upside-down rigidly. Bring it to a ~15 degree angle.")
    if input("Type 'GO' when ready (or 'Q' to abort): ").strip().upper() != 'GO': return

    print("Executing... Capturing data for 10 seconds. DO NOT TOUCH.")
    esp32.reset_input_buffer()
    esp32.write(b'START_PENDULUM\n')
    
    times, pitches = [], []
    start_wait = time.time()
    
    while time.time() - start_wait < 12.0:
        if esp32.in_waiting > 0:
            line = esp32.readline().decode('utf-8', errors='ignore').strip()
            if line == "TEST_DONE": break
            if line.startswith("TEST_DATA:"):
                try:
                    parts = line.split(":", 1)[1].split(",")
                    times.append(int(parts[0]) / 1000.0)
                    pitches.append(float(parts[1]))
                except: pass

    if len(pitches) < 50:
        print("[!] Error: Insufficient data.")
        return

    times = np.array(times) - times[0]
    pitches = np.array(pitches)

    p0 = [(np.max(pitches) - np.min(pitches))/2, 0.05, 2.0 * np.pi / 1.0, 0, np.mean(pitches)]
    bounds = ([0, 0, 0, -np.pi, -180], [180, 1.0, 20.0, np.pi, 180])

    try:
        popt, _ = curve_fit(damped_sine, times, pitches, p0=p0, bounds=bounds)
        fitted_curve = damped_sine(times, *popt)
        r2 = r2_score(pitches, fitted_curve)
    except Exception as e:
        print(f"[!] Curve fitting failed: {e}")
        return
    
    if r2 >= 0.90:
        data = load_params()
        params = data['physical_params']
        M_term = (params['m_b'] * params['l_b'] + params['m_w'] * params['l']) * params['g']
        
        zeta, wn = popt[1], popt[2]
        I_tot = M_term / (wn**2)
        I_b = I_tot - (params['m_w'] * (params['l']**2))
        C_b = 2 * zeta * wn * I_tot

        print(f"Calculated I_b: {I_b:.6f} | C_b: {C_b:.6f}")
        if input("Save and recalculate LQR? (Y/N): ").strip().upper() == 'Y':
            data['physical_params']['I_b'] = I_b
            data['physical_params']['C_b'] = C_b
            save_and_sync_params(esp32, data)
            calculate_lqr_and_sync(esp32)

def run_motor_sysid(esp32):
    """!
    @brief Executes the motor system identification routine to calculate wheel damping and motor constant.
    @param esp32 Active serial connection object.
    """
    print("\n--- MOTOR TEST ($K_m$, $C_w$) ---")
    print("Instruction: Rigidly FIX the pendulum body so it CANNOT move.")
    
    if input("Type 'GO' when ready (or 'Q' to abort): ").strip().upper() != 'GO':
        return

    print("Executing step response... DO NOT TOUCH.")
    esp32.reset_input_buffer()
    esp32.write(b'START_MOTOR_TEST\n')
    
    times, velocities = [], []
    start_wait = time.time()
    
    while time.time() - start_wait < 5.0:
        if esp32.in_waiting > 0:
            line = esp32.readline().decode('utf-8', errors='ignore').strip()
            if line == "TEST_DONE": break
            if line.startswith("MOTOR_DATA:"):
                try:
                    parts = line.split(":", 1)[1].split(",")
                    times.append(int(parts[0]) / 1000.0)
                    velocities.append(float(parts[1]))
                except (IndexError, ValueError): pass

    if len(velocities) < 20:
        print("[!] Error: Insufficient motor data.")
        return

    times = np.array(times) - times[0]
    velocities = np.array(velocities)

    U_step = 1.0 

    p0 = [np.max(velocities), 0.1]
    try:
        popt, _ = curve_fit(motor_step_response, times, velocities, p0=p0)
        fitted_curve = motor_step_response(times, *popt)
        r2 = r2_score(velocities, fitted_curve)
    except Exception as e:
        print(f"[!] Curve fitting failed: {e}")
        return

    print(f"\n--- Motor Analysis Complete (Accuracy R^2: {r2:.4f}) ---")
    
    if r2 < 0.90:
        print("[!] WARNING: Poor data fit. Is the body rigidly fixed?")
    else:
        data = load_params()
        params = data['physical_params']
        I_w = params['I_w'] 
        
        v_ss, tau = popt[0], popt[1]
        C_w = I_w / tau
        K_m = (v_ss * C_w) / U_step

        print(f"-> Calculated Wheel Damping (C_w): {C_w:.6f}")
        print(f"-> Calculated Motor Constant (K_m): {K_m:.4f} Nm/A")

    plt.figure(figsize=(10, 5))
    plt.plot(times, velocities, label="Raw Velocity")
    plt.plot(times, fitted_curve, 'r--', label="Step Response Fit")
    plt.title(f"Motor Step Response (R^2: {r2:.3f})")
    plt.legend()
    plt.show(block=False)

    if input("\nAccept and save these parameters to JSON? (Y/N): ").strip().upper() == 'Y':
        data['physical_params']['C_w'] = C_w
        data['physical_params']['K_m'] = K_m
        save_and_sync_params(esp32, data)
        calculate_lqr_and_sync(esp32)
    
    plt.close()

def find_esp32_port():
    """!
    @brief Scans available serial ports and identifies the one connected to the ESP32.
    @return String representing the COM port, or None if not found.
    """
    ports = serial.tools.list_ports.comports()
    for port in ports:
        desc = port.description.lower()
        if "usb" in desc or "uart" in desc or "ch340" in desc or "cp210" in desc:
            return port.device
    return None

def print_help_menu():
    """!
    @brief Displays the list of available commands in the terminal interface.
    """
    print("\n--- ESP32 CONTROL TERMINAL ---")
    print(" [1] Pendulum SysID Test")
    print(" [2] Motor SysID Test")
    print(" [3] Recalculate & Sync LQR")
    print(" [4] Calibrate Zero Offset")
    print(" [5] Get Current Pitch")
    print(" [6] Start Balancing")
    print(" [7] Stop Motors")
    print(" [8] Exit")
    print("------------------------------")

def run_serial_terminal():
    """!
    @brief Main routine for the serial terminal, handling communication threads and user input.
    """
    com_port = find_esp32_port()
    if not com_port:
        print("Error: No ESP32 detected via USB.")
        return
        
    esp32 = serial.Serial(com_port, 115200, timeout=0.1)
    time.sleep(2) 
    
    print("\n[PC] Syncing initial JSON parameters to ESP32...")
    data = load_params()
    save_and_sync_params(esp32, data)
    
    print_help_menu()
    stop_event = threading.Event()
    wizard_active = [False]

    def receive_loop():
        """!
        @brief Background thread task to continuously read and print incoming serial messages.
        """
        while not stop_event.is_set():
            if not wizard_active[0]:
                try:
                    if esp32.in_waiting > 0:
                        line = esp32.readline().decode('utf-8', errors='ignore').strip()
                        if line: print(f"\r[ESP] {line}\nCMD> ", end="", flush=True)
                except: pass
            time.sleep(0.01)

    recv_thread = threading.Thread(target=receive_loop, daemon=True)
    recv_thread.start()

    while True:
        try:
            cmd = input("CMD> ").strip()
            if cmd in ['8', 'exit']: break
            
            elif cmd == '1':
                wizard_active[0] = True
                run_pendulum_sysid(esp32)
                wizard_active[0] = False
                
            elif cmd == '2':
                wizard_active[0] = True
                run_motor_sysid(esp32)
                wizard_active[0] = False
                
            elif cmd == '3':
                calculate_lqr_and_sync(esp32)
                
            elif cmd == '4':
                esp32.write(b'CALIBRATE_ZERO\n')
                
            elif cmd == '5':
                esp32.write(b'GET_PITCH\n')
                
            elif cmd == '6':
                esp32.write(b'BALANCE\n')
                
            elif cmd == '7':
                esp32.write(b'STOP\n')
                
            elif cmd != '':
                esp32.write((cmd + '\n').encode())
                
        except KeyboardInterrupt:
            break

    stop_event.set()
    recv_thread.join(timeout=1.0)
    esp32.close()

if __name__ == "__main__":
    run_serial_terminal()