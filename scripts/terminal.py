import os
import re
import serial
import serial.tools.list_ports
import time
import sys
import socket
import threading
import json
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import find_peaks

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "include", "network_config.h")
JSON_PATH = "cube_params.json" # הקובץ שישמור את הפרמטרים

def load_network_config():
    default_ip = "192.168.4.1"
    default_port = 1234
    if not os.path.exists(CONFIG_PATH):
        return default_ip, default_port
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        text = f.read()
    ip_match = re.search(r'WIFI_AP_IP\]\s*=\s*"([^"]+)"', text)
    port_match = re.search(r'UDP_PORT\s*=\s*([0-9]+)', text)
    ip = ip_match.group(1) if ip_match else default_ip
    port = int(port_match.group(1)) if port_match else default_port
    return ip, port

def find_esp32_port():
    ports = serial.tools.list_ports.comports()
    if not ports: return None
    for port in ports:
        desc = port.description.lower()
        if "usb" in desc or "uart" in desc or "ch340" in desc or "cp210" in desc:
            return port.device
    return ports[0].device

# ==========================================
# אשף הטסטים (System ID Wizard)
# ==========================================
def run_sysid_wizard(esp32):
    print("\n" + "="*50)
    print("   SYSTEM IDENTIFICATION WIZARD")
    print("="*50)
    print("\nFinding parameter: I_f (Frame Moment of Inertia)")
    print("Pendulum test: Hang the frame from the axis and move to a 10-degree angle.")
    
    user_input = input("Type 'GO' to start the test, or anything else to abort: ")
    if user_input.strip().lower() != 'go':
        print("Test aborted. Returning to terminal.")
        return

    print("\nStarting test... Recording data for 10 seconds. DO NOT TOUCH!")
    
    # ניקוי חוצץ התקשורת ושליחת פקודת התחלה
    esp32.reset_input_buffer()
    esp32.write(b'START_PENDULUM\n')
    
    times = []
    pitches = []
    
    start_wait = time.time()
    while time.time() - start_wait < 12.0: # קצת ספר לביטחון
        if esp32.in_waiting > 0:
            line = esp32.readline().decode('utf-8', errors='ignore').strip()
            if line == "TEST_DONE":
                break
            if line.startswith("TEST_DATA:"):
                parts = line.replace("TEST_DATA:", "").split(",")
                if len(parts) == 2:
                    try:
                        times.append(int(parts[0]) / 1000.0) # המרה לשניות
                        pitches.append(float(parts[1]))
                    except ValueError:
                        pass

    if len(pitches) < 50:
        print("Error: Not enough data received. Check sensor connection.")
        return

    print("Data collection complete! Analyzing physics...")

    # יישור ציר הזמן לאפס
    times = np.array(times) - times[0]
    pitches = np.array(pitches)

    # מציאת נקודות הקיצון (Peaks) כדי לחשב את זמן המחזור
    # משתמשים במרחק מינימלי של 50 דגימות כדי לא לתפוס רעשים
    peaks, _ = find_peaks(pitches, distance=50) 
    
    if len(peaks) < 2:
        print("Error: Could not detect clear oscillations. Did you swing it?")
        return

    # חישוב זמן המחזור הממוצע (T)
    peak_times = times[peaks]
    T_avg = np.mean(np.diff(peak_times))
    print(f"-> Measured Oscillation Period (T): {T_avg:.3f} seconds")

    # שליפת המשתנים הפיזיקליים מקובץ ה-JSON (אם קיים)
    m_f = 0.5   # משקל שלדה חלופי אם אין JSON
    l_f = 0.075 # מרכז כובד חלופי אם אין JSON
    g = 9.81
    
    if os.path.exists(JSON_PATH):
        try:
            with open(JSON_PATH, 'r') as f:
                data = json.load(f)
                m_f = data.get("physical_params", {}).get("m_f", m_f)
                l_f = data.get("physical_params", {}).get("l_f", l_f)
        except: pass

    # המתמטיקה של המטוטלת
    I_f = ( (T_avg**2) * m_f * g * l_f ) / (4 * (np.pi**2))
    
    print(f"-> Using m_f={m_f}kg, l_f={l_f}m")
    print(f"-> Calculated I_f: {I_f:.6f} kg*m^2")

    # מראה ליוזר גרף כדי להוכיח שהמדידה מדויקת
    print("\nOpening graph. Close the graph window to continue and save...")
    plt.figure(figsize=(10, 5))
    plt.title("Pendulum Test - Pitch Angle Over Time")
    plt.plot(times, pitches, label="Raw Sensor Data", color="blue")
    plt.plot(times[peaks], pitches[peaks], "rx", markersize=10, label="Detected Peaks (Period T)")
    plt.xlabel("Time (Seconds)")
    plt.ylabel("Pitch Angle (Degrees)")
    plt.grid(True)
    plt.legend()
    plt.show() # התוכנית תעצור כאן עד שתסגור את החלון

    # שמירה ל-JSON
    params = {}
    if os.path.exists(JSON_PATH):
        with open(JSON_PATH, 'r') as f:
            params = json.load(f)
    
    if "physical_params" not in params:
        params["physical_params"] = {}
        
    params["physical_params"]["I_f"] = I_f
    
    with open(JSON_PATH, 'w') as f:
        json.dump(params, f, indent=4)
        
    print("\nSaved successfully to cube_params.json!")
    print("Moving back to terminal mode...\n")

# ==========================================
# הטרמינל הראשי (נשאר דומה, עם תוספת ההאזנה ל-get_params)
# ==========================================
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

    print("\n=== ESP32 USB-Serial Terminal ===")
    print("Type 'get_params' to enter System Identification Wizard.")
    print("Type 'show'/'hide' for sensor stream. Type 'exit' to quit.\n")

    stop_event = threading.Event()
    show_sensor = [False] 
    wizard_active = [False] # דגל שמונע מהתהליכון להדפיס תוך כדי טסט

    def receive_loop():
        while not stop_event.is_set():
            if not wizard_active[0]:
                try:
                    if esp32.in_waiting > 0:
                        line = esp32.readline().decode('utf-8', errors='ignore').strip()
                        if line:
                            if line.startswith("Pitch:"):
                                if show_sensor[0]:
                                    print(f"\r[Sensor] {line}\nCMD> ", end="", flush=True)
                            else:
                                print(f"\r[ESP] {line}\nCMD> ", end="", flush=True)
                except Exception:
                    pass
            time.sleep(0.01)

    recv_thread = threading.Thread(target=receive_loop, daemon=True)
    recv_thread.start()

    while True:
        try:
            user_command = input("CMD> ") 
            
            if user_command.lower() == 'exit':
                break
            elif user_command.lower() == 'hide':
                show_sensor[0] = False
                continue
            elif user_command.lower() == 'show':
                show_sensor[0] = True
                continue
            elif user_command.lower() == 'get_params':
                wizard_active[0] = True # משתיק את ההדפסות ברקע
                run_sysid_wizard(esp32)
                wizard_active[0] = False # מחזיר את הטרמינל לחיים
                continue
                
            if user_command.strip() != '':
                esp32.write((user_command + '\n').encode())
                
        except KeyboardInterrupt:
            break

    stop_event.set()
    esp32.close()
    recv_thread.join(timeout=1.0)

# פונקציות ה-WIFI וה-MAIN נשארות ללא שינוי (כמו בקובץ ששלחת)
# ...