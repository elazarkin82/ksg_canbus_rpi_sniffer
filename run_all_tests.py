#!/usr/bin/env python3

import os
import sys
import subprocess
import multiprocessing
import shutil

# Configuration
BUILD_DIR = "build_tests"
TEST_EXECUTABLES = [
    "tcp_communication_tester", 
    "tcp_canbus_tester",
    "udp_communication_tester",
    "udp_canbus_tester",
    "run_emulator_test.py" # Python script
]

# Colors
GREEN = '\033[0;32m'
RED = '\033[0;31m'
NC = '\033[0m' # No Color

def print_colored(text, color):
    print(f"{color}{text}{NC}")

def run_command(command):
    try:
        # Run command in the current working directory (which will be BUILD_DIR)
        result = subprocess.run(
            command,
            check=False,
            shell=True
        )
        return result.returncode
    except Exception as e:
        print_colored(f"Error running command: {command}\n{e}", RED)
        return 1

def check_vcan():
    try:
        subprocess.check_output(["ip", "link", "show", "vcan0"], stderr=subprocess.STDOUT)
        return True
    except subprocess.CalledProcessError:
        return False

def main():
    print("========================================")
    print("       RpiCanbusSniffer Test Runner     ")
    print("========================================")

    # 0. Check vcan0 (Prerequisite for emulator test)
    vcan_ready = check_vcan()
    if not vcan_ready:
        print_colored("WARNING: vcan0 interface not found!", RED)
        print("Some tests (emulator) will fail or be skipped.")
        print("Please run: sudo ./car_system_canbus_emulator/setup_vcan.sh")
        print("----------------------------------------")

    # 1. Create Build Directory
    if not os.path.exists(BUILD_DIR):
        print(f"Creating build directory: {BUILD_DIR}")
        os.makedirs(BUILD_DIR)

    # 2. Enter Build Directory
    try:
        os.chdir(BUILD_DIR)
    except OSError as e:
        print_colored(f"Failed to enter build directory: {e}", RED)
        sys.exit(1)

    # 3. Configure (CMake)
    print("Configuring with CMake...")
    if run_command("cmake ..") != 0:
        print_colored("CMake configuration failed!", RED)
        sys.exit(1)

    # 4. Build (Make)
    print("Building...")
    num_cores = multiprocessing.cpu_count()
    if run_command(f"make -j{num_cores}") != 0:
        print_colored("Build failed!", RED)
        sys.exit(1)

    # 5. Copy Python Test Scripts
    print("Copying test scripts...")
    try:
        shutil.copy("../tests/run_emulator_test.py", ".")
        os.chmod("run_emulator_test.py", 0o755)
    except Exception as e:
        print_colored(f"Failed to copy test script: {e}", RED)

    print("Build successful. Running tests...")
    print("----------------------------------------")

    total_tests = 0
    passed_tests = 0
    failed_tests = 0
    failed_list = []

    # 6. Run Tests
    for test_exe in TEST_EXECUTABLES:
        # Skip emulator test if vcan0 is missing
        if test_exe == "run_emulator_test.py" and not vcan_ready:
            print_colored(f"Skipping {test_exe} (vcan0 missing)", RED)
            continue

        if os.path.exists(f"./{test_exe}"):
            print(f"Running {test_exe}...")
            
            cmd = f"./{test_exe}"
            if test_exe.endswith(".py"):
                cmd = f"python3 {test_exe}"

            ret_code = run_command(cmd)
            
            total_tests += 1
            
            if ret_code == 0:
                passed_tests += 1
            else:
                failed_tests += 1
                failed_list.append(test_exe)
            
            print("----------------------------------------")
        else:
            print_colored(f"Test executable not found: {test_exe}", RED)
            failed_tests += 1
            failed_list.append(f"{test_exe} (Not Found)")

    # 7. Summary
    print("========================================")
    print("             TEST SUMMARY               ")
    print("========================================")
    print(f"Total Suites: {total_tests}")
    
    print(f"Passed:       {GREEN}{passed_tests}{NC}")
    print(f"Failed:       {RED}{failed_tests}{NC}")

    if failed_tests == 0:
        print_colored("ALL TESTS PASSED!", GREEN)
        sys.exit(0)
    else:
        print_colored("SOME TESTS FAILED:", RED)
        for failed in failed_list:
            print_colored(f" - {failed}", RED)
        sys.exit(1)

if __name__ == "__main__":
    main()
