#!/usr/bin/env python3

import os
import sys
import subprocess
import multiprocessing
import shutil

# Configuration
BUILD_DIR = "build_tests"
RELEASE_DIR = "release"
TEST_EXECUTABLES = [
    "params_tester",
    "tcp_communication_tester", 
    "tcp_canbus_tester",
    "udp_communication_tester",
    "udp_canbus_tester",
    "car_system_test.py",
    "car_computer_test.py",
    "emulators_integration_test.py",
    "run_sniffer_test.py",
    "main_service_tester.py"
]

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
            check=False,
            shell=True,
            cwd=cwd
        )
        return result.returncode
    except Exception as e:
        print_colored(f"Error running command: {command}\n{e}", RED)
        return 1

def check_vcan():
    try:
        subprocess.check_output(["ip", "link", "show", "vcan0"], stderr=subprocess.STDOUT)
        subprocess.check_output(["ip", "link", "show", "vcan1"], stderr=subprocess.STDOUT)
        return True
    except subprocess.CalledProcessError:
        return False

def create_release(debug_mode=False):
    print("========================================")
    print("           Creating Release             ")
    print("========================================")
    
    # We are inside BUILD_DIR
    release_root = os.path.abspath("../release")
    project_root = os.path.abspath("..")
    
    if os.path.exists(release_root):
        shutil.rmtree(release_root)
    os.makedirs(release_root)

    # 1. Sniffer Service (Ubuntu)
    ubuntu_dir = os.path.join(release_root, "sniffer_service_release", "ubuntu")
    os.makedirs(ubuntu_dir)
    
    if os.path.exists("sniffer_service"):
        shutil.copy("sniffer_service", ubuntu_dir)
        print(f"Copied sniffer_service (Ubuntu) to {ubuntu_dir}")
    else:
        print_colored("sniffer_service not found in build directory!", RED)

    # 2. Sniffer Service (RPi)
    rpi_dir = os.path.join(release_root, "sniffer_service_release", "rpi")
    os.makedirs(rpi_dir)
    
    # Check for toolchain (relative to build dir)
    toolchain_file = os.path.abspath("../toolchain-rpi.cmake")
    if os.path.exists(toolchain_file):
        print("Building for RPi...")
        build_rpi_dir = "build_rpi"
        if not os.path.exists(build_rpi_dir):
            os.makedirs(build_rpi_dir)
        
        cmake_cmd = f"cmake -DCMAKE_TOOLCHAIN_FILE={toolchain_file} {project_root}"
        if debug_mode:
            cmake_cmd += " -DCMAKE_BUILD_TYPE=Debug -DDEBUG=1"

        # Use project_root instead of ..
        if run_command(cmake_cmd, cwd=build_rpi_dir) == 0:
            if run_command("make sniffer_service", cwd=build_rpi_dir) == 0:
                shutil.copy(os.path.join(build_rpi_dir, "sniffer_service"), rpi_dir)
                print(f"Copied sniffer_service (RPi) to {rpi_dir}")
            else:
                print_colored("Failed to build for RPi", RED)
        else:
            print_colored("Failed to configure for RPi", RED)
    else:
        print("Toolchain file not found, skipping RPi build.")

    # 3. External Service
    ext_dir = os.path.join(release_root, "external_service_release")
    os.makedirs(ext_dir)
    
    # Copy Python code
    src_python = "../external_server/python"
    if os.path.exists(src_python):
        shutil.copytree(src_python, ext_dir, dirs_exist_ok=True)
    
    # Copy Library
    if os.path.exists("libsniffer_client.so"):
        backend_dir = os.path.join(ext_dir, "backend")
        if not os.path.exists(backend_dir):
            os.makedirs(backend_dir)
        shutil.copy("libsniffer_client.so", backend_dir)
        print(f"Copied libsniffer_client.so to {backend_dir}")
    else:
        print_colored("libsniffer_client.so not found!", RED)

    print_colored("Release created successfully!", GREEN)

def main():
    print("========================================")
    print("       RpiCanbusSniffer Test Runner     ")
    print("========================================")

    debug_mode = False
    if len(sys.argv) > 1 and sys.argv[1] == "debug":
        debug_mode = True
        print_colored("DEBUG MODE ENABLED", GREEN)

    # 0. Check vcan0/1 (Prerequisite for emulator test)
    vcan_ready = check_vcan()
    if not vcan_ready:
        print_colored("WARNING: vcan0/vcan1 interface not found!", RED)
        print("Some tests (emulator) will fail or be skipped.")
        print("Please run: sudo ./emulators/setup_vcan.sh")
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
    cmake_cmd = "cmake .."
    if debug_mode:
        cmake_cmd += " -DCMAKE_BUILD_TYPE=Debug -DDEBUG=1"
    
    if run_command(cmake_cmd) != 0:
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
    scripts = ["car_system_test.py", "car_computer_test.py", "emulators_integration_test.py", "run_sniffer_test.py", "main_service_tester.py"]
    for script in scripts:
        try:
            shutil.copy(f"../tests/{script}", ".")
            os.chmod(script, 0o755)
        except Exception as e:
            print_colored(f"Failed to copy {script}: {e}", RED)

    print("Build successful. Running tests...")
    print("----------------------------------------")

    total_tests = 0
    passed_tests = 0
    failed_tests = 0
    failed_list = []

    # 6. Run Tests
    for test_exe in TEST_EXECUTABLES:
        # Skip emulator tests if vcan is missing
        if test_exe.endswith(".py") and not vcan_ready:
            print_colored(f"Skipping {test_exe} (vcan missing)", RED)
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
        create_release(debug_mode) # Create release if tests passed
        sys.exit(0)
    else:
        print_colored("SOME TESTS FAILED:", RED)
        for failed in failed_list:
            print_colored(f" - {failed}", RED)
        sys.exit(1)

if __name__ == "__main__":
    main()
