#!/usr/bin/env python3

import ctypes
import os
import sys
import subprocess
import time

def run_test():
    # 1. Start Emulators
    print("Starting Emulators...")
    car_system_exe = "../build_tests/car_system_emulator"
    car_computer_exe = "../build_tests/car_computer_emulator"
    
    if not os.path.exists(car_system_exe) or not os.path.exists(car_computer_exe):
        print("Emulators not found. Build them first.")
        sys.exit(1)

    proc_sys = subprocess.Popen([car_system_exe, "vcan0"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    proc_comp = subprocess.Popen([car_computer_exe, "vcan1"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    time.sleep(1) # Wait for emulators to start

    # 2. Load Test Library
    lib_path = os.path.abspath("../build_tests/libsniffer_test_lib.so")
    if not os.path.exists(lib_path):
        # Try current dir
        lib_path = os.path.abspath("./libsniffer_test_lib.so")
    
    if not os.path.exists(lib_path):
        print(f"Library not found: {lib_path}")
        proc_sys.terminate()
        proc_comp.terminate()
        sys.exit(1)

    lib = ctypes.CDLL(lib_path)
    lib.run_all_sniffer_tests.restype = ctypes.c_int

    # 3. Run Tests
    print("Running Sniffer Tests...")
    result = lib.run_all_sniffer_tests()

    # 4. Cleanup
    print("Stopping Emulators...")
    proc_sys.terminate()
    proc_comp.terminate()
    proc_sys.wait()
    proc_comp.wait()

    if result == 0:
        print("SUCCESS")
        sys.exit(0)
    else:
        print("FAILURE")
        sys.exit(1)

if __name__ == "__main__":
    run_test()
