#!/usr/bin/env python3

import os
import sys
import subprocess
import multiprocessing

# Configuration
BUILD_DIR = "build_tests"
TEST_EXECUTABLES = ["tcp_communication_tester", "tcp_canbus_tester"]

# Colors
GREEN = '\033[0;32m'
RED = '\033[0;31m'
NC = '\033[0m' # No Color

def print_colored(text, color):
    print(f"{color}{text}{NC}")

def run_command(command, cwd=None):
    try:
        result = subprocess.run(
            command,
            cwd=cwd,
            check=False, # We handle return code manually
            shell=True
        )
        return result.returncode
    except Exception as e:
        print_colored(f"Error running command: {command}\n{e}", RED)
        return 1

def main():
    print("========================================")
    print("       RpiCanbusSniffer Test Runner     ")
    print("========================================")

    # 1. Create Build Directory
    if not os.path.exists(BUILD_DIR):
        print(f"Creating build directory: {BUILD_DIR}")
        os.makedirs(BUILD_DIR)

    # 2. Configure (CMake)
    print("Configuring with CMake...")
    if run_command("cmake ..", cwd=BUILD_DIR) != 0:
        print_colored("CMake configuration failed!", RED)
        sys.exit(1)

    # 3. Build (Make)
    print("Building...")
    num_cores = multiprocessing.cpu_count()
    if run_command(f"make -j{num_cores}", cwd=BUILD_DIR) != 0:
        print_colored("Build failed!", RED)
        sys.exit(1)

    print("Build successful. Running tests...")
    print("----------------------------------------")

    total_tests = 0
    passed_tests = 0
    failed_tests = 0
    failed_list = []

    # 4. Run Tests
    for test_exe in TEST_EXECUTABLES:
        test_path = os.path.join(BUILD_DIR, test_exe)

        if os.path.exists(test_path):
            print(f"Running {test_exe}...")
            ret_code = run_command(f"./{test_exe}", cwd=BUILD_DIR)
            
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

    # 5. Summary
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
