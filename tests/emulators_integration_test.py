#!/usr/bin/env python3

import ctypes
import time
import os
import sys
import threading

try:
    import tkinter as tk
    from tkinter import ttk
except ImportError:
    tk = None

# Load Library
lib_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../build/libemulator_wrapper.so'))
# Adjust path if build directory is different
if not os.path.exists(lib_path):
    # Try current directory or standard build locations
    lib_path = os.path.abspath('./libemulator_wrapper.so')

if not os.path.exists(lib_path):
    print(f"Error: Library not found at {lib_path}")
    sys.exit(1)

lib = ctypes.CDLL(lib_path)

# Define Argument Types
lib.start_system_emulator.argtypes = [ctypes.c_char_p]
lib.start_computer_emulator.argtypes = [ctypes.c_char_p]
lib.wrapper_get_rpm.restype = ctypes.c_double
lib.wrapper_get_speed.restype = ctypes.c_double
lib.wrapper_get_throttle.restype = ctypes.c_double
lib.wrapper_get_brake.restype = ctypes.c_double
lib.wrapper_get_steering.restype = ctypes.c_double

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

def run_test(gui=False):
    print("Starting Emulators Integration Test...")
    
    vcan0 = b"vcan0"
    
    lib.start_system_emulator(vcan0)
    lib.start_computer_emulator(vcan0) 

    print("Emulators running. Monitoring...")
    
    root = None
    app = None

    if gui and tk:
        root = tk.Tk()
        app = EmulatorGUI(root)
    elif gui and not tk:
        print("Warning: tkinter not available, running in headless mode.")
        gui = False

    start_time = time.time()
    
    try:
        while time.time() - start_time < 20: # Run for 20 seconds with GUI
            rpm = lib.wrapper_get_rpm()
            speed = lib.wrapper_get_speed()
            throttle = lib.wrapper_get_throttle()
            brake = lib.wrapper_get_brake()
            steering = lib.wrapper_get_steering()
            
            if gui:
                app.update(rpm, speed, throttle, brake, steering)
                root.update()
            else:
                print(f"Time: {time.time()-start_time:.1f}s | RPM: {rpm:.0f} | Speed: {speed:.1f} | Thr: {throttle:.0f}% | Brk: {brake:.0f} | Str: {steering:.1f}")
                time.sleep(0.5)
            
            if gui:
                time.sleep(0.05) # Faster update for GUI

    except KeyboardInterrupt:
        print("Interrupted by user")

    print("Stopping...")
    lib.stop_computer_emulator()
    lib.stop_system_emulator()
    
    if gui:
        root.destroy()

    print("Test Passed")

if __name__ == "__main__":
    gui_mode = "--gui" in sys.argv
    run_test(gui_mode)
