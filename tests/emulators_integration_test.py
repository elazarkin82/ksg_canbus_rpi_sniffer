#!/usr/bin/env python3

import subprocess
import time
import os
import sys
import threading
import socket
import struct
import select

try:
    import tkinter as tk
    from tkinter import ttk
except ImportError:
    tk = None

# CAN Frame Structure (Standard)
# struct can_frame {
#     canid_t can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
#     __u8    can_dlc; /* frame payload length in byte (0 .. 8) */
#     __u8    __pad;   /* padding */
#     __u8    __res0;  /* reserved / padding */
#     __u8    __res1;  /* reserved / padding */
#     __u8    data[8] __attribute__((aligned(8)));
# };
CAN_FRAME_FMT = "=IB3x8s"

class EmulatorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Car Emulator Dashboard")
        self.root.geometry("400x300")

        # Variables
        self.rpm_var = tk.StringVar(value="RPM: 0")
        self.speed_var = tk.StringVar(value="Speed: 0 km/h")
        self.steering_var = tk.StringVar(value="Steering: 0 deg")
        
        # Layout
        main_frame = ttk.Frame(root, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Speed & RPM
        ttk.Label(main_frame, textvariable=self.speed_var, font=("Arial", 16)).pack(pady=5)
        ttk.Label(main_frame, textvariable=self.rpm_var, font=("Arial", 12)).pack(pady=5)

        # Throttle
        ttk.Label(main_frame, text="Throttle").pack(anchor=tk.W)
        self.throttle_bar = ttk.Progressbar(main_frame, orient=tk.HORIZONTAL, length=300, mode='determinate')
        self.throttle_bar.pack(pady=5)

        # Brake
        ttk.Label(main_frame, text="Brake").pack(anchor=tk.W)
        self.brake_bar = ttk.Progressbar(main_frame, orient=tk.HORIZONTAL, length=300, mode='determinate')
        self.brake_bar.pack(pady=5)

        # Steering
        ttk.Label(main_frame, textvariable=self.steering_var).pack(pady=10)
        self.steering_canvas = tk.Canvas(main_frame, width=200, height=50, bg="white")
        self.steering_canvas.pack()
        self.steering_line = self.steering_canvas.create_line(100, 50, 100, 0, width=3, fill="blue")

    def update(self, rpm, speed, throttle, brake, steering):
        self.rpm_var.set(f"RPM: {rpm:.0f}")
        self.speed_var.set(f"Speed: {speed:.1f} km/h")
        self.steering_var.set(f"Steering: {steering:.1f} deg")
        
        self.throttle_bar['value'] = throttle
        self.brake_bar['value'] = brake

        # Update steering visual (simple line)
        # Map -30..30 degrees to x coordinates
        x_offset = (steering / 30.0) * 80 # +/- 80 pixels
        self.steering_canvas.coords(self.steering_line, 100, 50, 100 + x_offset, 0)

def open_can_socket(interface):
    try:
        s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        s.bind((interface,))
        return s
    except Exception as e:
        print(f"Error opening CAN socket on {interface}: {e}")
        return None

def parse_can_frame(data):
    can_id, can_dlc, payload = struct.unpack(CAN_FRAME_FMT, data)
    return can_id, can_dlc, payload[:can_dlc]

def run_test(gui=False):
    print("Starting Emulators Integration Test...")
    
    # Paths to executables
    # Assuming running from build_tests directory
    car_system_exe = "../build_tests/car_system_emulator"
    car_computer_exe = "../build_tests/car_computer_emulator"
    
    if not os.path.exists(car_system_exe):
        print(f"Error: {car_system_exe} not found")
        return
    if not os.path.exists(car_computer_exe):
        print(f"Error: {car_computer_exe} not found")
        return

    vcan0 = "vcan0"
    vcan1 = "vcan1"
    
    # Start Emulators
    # System on vcan0
    # Computer on vcan1
    # We will bridge them in Python
    
    print(f"Launching {car_system_exe} on {vcan0}")
    proc_system = subprocess.Popen([car_system_exe, vcan0], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    print(f"Launching {car_computer_exe} on {vcan1}")
    proc_computer = subprocess.Popen([car_computer_exe, vcan1], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # Open Sockets
    sock0 = open_can_socket(vcan0)
    sock1 = open_can_socket(vcan1)
    
    if not sock0 or not sock1:
        proc_system.terminate()
        proc_computer.terminate()
        return

    print("Emulators running. Bridging and Monitoring...")
    
    root = None
    app = None

    if gui and tk:
        root = tk.Tk()
        app = EmulatorGUI(root)
    elif gui and not tk:
        print("Warning: tkinter not available, running in headless mode.")
        gui = False

    # State for GUI
    state = {
        'rpm': 0, 'speed': 0, 'throttle': 0, 'brake': 0, 'steering': 0
    }

    running = True
    start_time = time.time()
    
    try:
        while running and (time.time() - start_time < 20): # Run for 20 seconds
            
            # Select on sockets
            readable, _, _ = select.select([sock0, sock1], [], [], 0.01)
            
            for s in readable:
                frame_data = s.recv(16) # sizeof(struct can_frame) = 16
                can_id, dlc, data = parse_can_frame(frame_data)
                
                # Bridge
                if s == sock0:
                    # From System (vcan0) -> To Computer (vcan1)
                    sock1.send(frame_data)
                    
                    # Parse Data for GUI
                    if can_id == 0x100: # RPM
                        val = (data[0] << 8) | data[1]
                        state['rpm'] = val / 4.0
                    elif can_id == 0x200: # Speed
                        state['speed'] = data[0]
                    elif can_id == 0x201: # Steering Angle
                        val = (data[0] << 8) | data[1]
                        state['steering'] = (val - 32768)
                        
                elif s == sock1:
                    # From Computer (vcan1) -> To System (vcan0)
                    sock0.send(frame_data)
                    
                    # Parse Data for GUI
                    if can_id == 0x400: # Throttle/Brake
                        state['throttle'] = data[0]
                        state['brake'] = data[1]
            
            # Update GUI
            if gui:
                app.update(state['rpm'], state['speed'], state['throttle'], state['brake'], state['steering'])
                root.update()
            elif time.time() % 1.0 < 0.02: # Print every ~1s
                 print(f"RPM: {state['rpm']:.0f} | Speed: {state['speed']:.1f} | Thr: {state['throttle']:.0f}% | Brk: {state['brake']:.0f} | Str: {state['steering']:.1f}")

    except KeyboardInterrupt:
        print("Interrupted by user")
    except Exception as e:
        print(f"Error: {e}")

    print("Stopping...")
    proc_system.terminate()
    proc_computer.terminate()
    proc_system.wait()
    proc_computer.wait()
    
    if gui:
        root.destroy()

    print("Test Passed")

if __name__ == "__main__":
    gui_mode = "--gui" in sys.argv
    run_test(gui_mode)
