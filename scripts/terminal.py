import os
import time
import json
import threading
import numpy as np
import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
from scipy.optimize import curve_fit


def r2_score(y_true, y_pred):
    """Simple R^2 (coefficient of determination) implementation."""
    y_true = np.array(y_true)
    y_pred = np.array(y_pred)
    ss_res = np.sum((y_true - y_pred) ** 2)
    ss_tot = np.sum((y_true - np.mean(y_true)) ** 2)
    return 1.0 - ss_res / ss_tot if ss_tot != 0 else 0.0

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "include", "network_config.h")
JSON_PATH = "cube_params.json"

def find_esp32_port():
    ports = serial.tools.list_ports.comports()
    if not ports: return None
    for port in ports:
        desc = port.description.lower()
        if "usb" in desc or "uart" in desc or "ch340" in desc or "cp210" in desc:
            return port.device
    return ports[0].device

def damped_sine(t, A, zeta, wn, phi, c):
    """ Mathematical model for damped harmonic oscillator. """
    wd = wn * np.sqrt(1 - zeta**2)
    return A * np.exp(-zeta * wn * t) * np.cos(wd * t + phi) + c

def motor_step_response(t, v_ss, tau):
    """ Mathematical model for first-order system step response. """
    return v_ss * (1 - np.exp(-t / tau))

def run_pendulum_sysid(esp32):
    print("\n--- PENDULUM TEST ($I_b$, $C_b$) ---")
    print("Instruction: Hang the body upside-down rigidly. Bring it to a ~15 degree angle.")
    
    if input("Type 'GO' when ready (or 'Q' to abort): ").strip().upper() != 'GO':
        return

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
                except (IndexError, ValueError): pass

    if len(pitches) < 50:
        print("[!] Error: Insufficient data. Test failed.")
        return

    times = np.array(times) - times[0]
    pitches = np.array(pitches)

    # Initial guesses: Amplitude, Damping Ratio, Natural Freq, Phase, Offset
    p0 = [(np.max(pitches) - np.min(pitches))/2, 0.05, 2.0 * np.pi / 1.0, 0, np.mean(pitches)]
    bounds = ([0, 0, 0, -np.pi, -180], [180, 1.0, 20.0, np.pi, 180])

    try:
        popt, _ = curve_fit(damped_sine, times, pitches, p0=p0, bounds=bounds)
        fitted_curve = damped_sine(times, *popt)
        r2 = r2_score(pitches, fitted_curve)
    except Exception as e:
        print(f"[!] Curve fitting failed: {e}")
        return

    print(f"\n--- Analysis Complete (Accuracy R^2: {r2:.4f}) ---")
    
    if r2 < 0.90:
        print("[!] WARNING: Data quality is poor (R^2 < 0.90). Check physical setup.")
    else:
        # Extract physical properties based on known constants
        params = json.load(open(JSON_PATH))['physical_params']
        M_term = (params['m_b'] * params['l_b'] + params['m_w'] * params['l']) * params['g']
        
        zeta, wn = popt[1], popt[2]
        I_tot = M_term / (wn**2)
        I_b = I_tot - (params['m_w'] * (params['l']**2))
        C_b = 2 * zeta * wn * I_tot

        print(f"-> Calculated Body Inertia ($I_b$): {I_b:.6f} kg*m^2")
        print(f"-> Calculated Damping Coeff ($C_b$): {C_b:.6f} kg*m^2/s")

    plt.figure(figsize=(10, 5))
    plt.plot(times, pitches, label="Raw Data")
    plt.plot(times, fitted_curve, 'r--', label="Mathematical Fit")
    plt.title(f"Pendulum Fit (R^2: {r2:.3f})")
    plt.legend()
    plt.show(block=False)

    if input("\nAccept and save these parameters to JSON? (Y/N): ").strip().upper() == 'Y':
        data = json.load(open(JSON_PATH))
        data['physical_params']['I_b'] = I_b
        data['physical_params']['C_b'] = C_b
        json.dump(data, open(JSON_PATH, 'w'), indent=4)
        print("Saved successfully.")
    
    plt.close()

def run_motor_sysid(esp32):
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

    # Assume a known constant input current U (e.g., 1.0 Ampere)
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
        params = json.load(open(JSON_PATH))['physical_params']
        I_w = params['I_w'] # Assuming Wheel Inertia is precisely known from CAD
        
        v_ss, tau = popt[0], popt[1]
        C_w = I_w / tau
        K_m = (v_ss * C_w) / U_step

        print(f"-> Calculated Wheel Damping ($C_w$): {C_w:.6f}")
        print(f"-> Calculated Motor Constant ($K_m$): {K_m:.4f} Nm/A")

    plt.figure(figsize=(10, 5))
    plt.plot(times, velocities, label="Raw Velocity")
    plt.plot(times, fitted_curve, 'r--', label="Step Response Fit")
    plt.title(f"Motor Step Response (R^2: {r2:.3f})")
    plt.legend()
    plt.show(block=False)

    if input("\nAccept and save these parameters to JSON? (Y/N): ").strip().upper() == 'Y':
        data = json.load(open(JSON_PATH))
        data['physical_params']['C_w'] = C_w
        data['physical_params']['K_m'] = K_m
        json.dump(data, open(JSON_PATH, 'w'), indent=4)
        print("Saved successfully.")
    
    plt.close()

def run_serial_terminal():
    com_port = find_esp32_port()
    if not com_port:
        print("Error: No USB device found.")
        return
        
    try:
        esp32 = serial.Serial(com_port, 115200, timeout=0.1)
        time.sleep(2) 
    except serial.SerialException as e:
        print(f"Error: {e}")
        return

    print("\n=== SYSTEM ID WIZARD ===")
    
    stop_event = threading.Event()
    wizard_active = [False]

    def receive_loop():
        while not stop_event.is_set():
            if not wizard_active[0]:
                try:
                    if esp32.in_waiting > 0:
                        line = esp32.readline().decode('utf-8', errors='ignore').strip()
                        if line:
                            print(f"\r[ESP] {line}\nCMD> ", end="", flush=True)
                except Exception: pass
            time.sleep(0.01)

    recv_thread = threading.Thread(target=receive_loop, daemon=True)
    recv_thread.start()

    while True:
        try:
            print("\nCommands: [1] Pendulum SysID | [2] Motor SysID | [3] Exit")
            cmd = input("CMD> ").strip()
            
            if cmd == '3' or cmd.lower() == 'exit':
                break
            elif cmd == '1':
                wizard_active[0] = True
                run_pendulum_sysid(esp32)
                wizard_active[0] = False
            elif cmd == '2':
                wizard_active[0] = True
                run_motor_sysid(esp32)
                wizard_active[0] = False
            elif cmd != '':
                esp32.write((cmd + '\n').encode())
                
        except KeyboardInterrupt:
            break

    stop_event.set()
    esp32.close()
    recv_thread.join(timeout=1.0)

if __name__ == "__main__":
    run_serial_terminal()