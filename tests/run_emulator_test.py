#!/usr/bin/env python3

import os
import sys
import subprocess
import time
import signal

# Configuration
# Assuming this script runs from build_tests directory
EMULATOR_EXE = "./car_system_emulator"
TESTER_EXE = "./emulator_tester"
IFACE = "vcan0"

# Colors
GREEN = '\033[0;32m'
RED = '\033[0;31m'
NC = '\033[0m'

def print_colored(text, color):
    print(f"{color}{text}{NC}")

def check_interface(iface):
    try:
        subprocess.check_output(["ip", "link", "show", iface], stderr=subprocess.STDOUT)
        return True
    except subprocess.CalledProcessError:
        return False

def main():
    # 1. Check vcan0
    if not check_interface(IFACE):
        print_colored(f"Interface {IFACE} not found! Please run setup_vcan.sh first.", RED)
        sys.exit(1)

    # 2. Start Emulator
    print("Starting Emulator...")
    
    if not os.path.exists(EMULATOR_EXE):
        print_colored("Emulator executable not found! Build first.", RED)
        sys.exit(1)

    emulator_proc = subprocess.Popen([EMULATOR_EXE, IFACE], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    time.sleep(1) # Wait for emulator to init

    # 3. Run Tester
    print("Running Tester...")
    ret_code = subprocess.run([TESTER_EXE, IFACE]).returncode

    # 4. Cleanup
    print("Stopping Emulator...")
    emulator_proc.send_signal(signal.SIGINT)
    emulator_proc.wait()

    if ret_code == 0:
        print_colored("EMULATOR TEST PASSED!", GREEN)
        sys.exit(0)
    else:
        print_colored("EMULATOR TEST FAILED!", RED)
        sys.exit(1)

if __name__ == "__main__":
    main()
