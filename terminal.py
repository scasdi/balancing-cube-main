import serial
import serial.tools.list_ports
import time
import sys
import socket

def find_esp32_port():
    print("Scanning for connected USB devices...")
    ports = serial.tools.list_ports.comports()
    
    if not ports:
        return None
        
    for port in ports:
        desc = port.description.lower()
        if "usb" in desc or "uart" in desc or "ch340" in desc or "cp210" in desc:
            return port.device
            
    return ports[0].device

def run_serial_terminal():
    com_port = find_esp32_port()
    if not com_port:
        print("Error: No USB device found. Check your connection.")
        return
        
    print(f"ESP32 detected on port: {com_port}")
    
    try:
        esp32 = serial.Serial(com_port, 115200, timeout=1)
        time.sleep(2) 
    except serial.SerialException as e:
        print(f"Error opening port {com_port}: {e}")
        print("Make sure PlatformIO Serial Monitor is CLOSED.")
        return

    print("\n=== ESP32 USB-Serial Terminal ===")
    print("Type 'exit' to quit.\n")

    while True:
        try:
            user_command = input("Serial-CMD> ") 
            
            if user_command.lower() == 'exit':
                print("Closing USB connection.")
                break
                
            if user_command.strip() == '':
                continue
                
            esp32.write((user_command + '\n').encode())
            
            time.sleep(0.1)
            if esp32.in_waiting > 0:
                response = esp32.read(esp32.in_waiting).decode().strip()
                print(f"Reply: {response}\n")
                
        except KeyboardInterrupt:
            print("\nClosing connection.")
            break

    esp32.close()

def run_wifi_terminal():
    UDP_IP = "192.168.4.1"
    UDP_PORT = 1234
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(1.0) 
    except Exception as e:
        print(f"Failed to create socket: {e}")
        return

    print("\n=== ESP32 WiFi (UDP) Terminal ===")
    print(f"Target: {UDP_IP}:{UDP_PORT}")
    
    # ==============================================================
    # כאן נמצאת שליחת ה-HELLO הראשונית (Handshake) <<<
    # ==============================================================
    print("Initiating connection to ESP32...")
    sock.sendto("HELLO\n".encode(), (UDP_IP, UDP_PORT))
    try:
        data, _ = sock.recvfrom(1024)
        print(f"[{UDP_IP}] says: {data.decode().strip()}")
    except socket.timeout:
        print(">>> WARNING: No response from ESP32! Are you connected to its WiFi? <<<")
    # ==============================================================

    print("\nType a command (Type 'exit' to quit).")

    try:
        while True:
            user_command = input("WiFi-CMD> ")
            
            if user_command.lower() == 'exit':
                print("Closing WiFi connection.")
                break
                
            if user_command.strip() == '':
                continue
                
            sock.sendto((user_command + '\n').encode(), (UDP_IP, UDP_PORT))
            
            try:
                data, _ = sock.recvfrom(1024)
                if data:
                    print(f"Reply: {data.decode().strip()}\n")
            except socket.timeout:
                pass
                
    except KeyboardInterrupt:
        print("\nClosing WiFi terminal.")
    finally:
        sock.close()

def main():
    # ==============================================================
    # כאן נמצאת ההשהיה שנותנת לבקר זמן לעלות אחרי הצריבה <<<
    # ==============================================================
    print("Waiting 3 seconds for ESP32 to boot...")
    time.sleep(3)

    print("===============================")
    print("   ESP32 Control Station       ")
    print("===============================")
    print("1. Connect via Serial (USB)")
    print("2. Connect via WiFi (UDP)")
    print("0. Exit")
    
    choice = input("\nSelect communication mode: ").strip()
    
    if choice == '1':
        run_serial_terminal()
    elif choice == '2':
        run_wifi_terminal()
    elif choice == '0':
        print("Exiting...")
        sys.exit(0)
    else:
        print("Invalid choice. Please run the script again.")

if __name__ == "__main__":
    main()