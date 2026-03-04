#!/usr/bin/env python3

import ctypes
import os
import sys
import subprocess
import time
import threading
import socket
import struct

# --- Constants ---
CMD_SET_PARAMS = 0x1007
CMD_EXTERNAL_SERVICE_LOGGING_ON = 0x1004

# --- Load Libraries ---

def load_lib(name):
    lib_path = os.path.abspath(f"../build_tests/{name}")
    if not os.path.exists(lib_path):
        lib_path = os.path.abspath(f"./{name}")
    
    if not os.path.exists(lib_path):
        print(f"Error: Library {name} not found")
        sys.exit(1)
    return ctypes.CDLL(lib_path)

lib_service = load_lib("libmain_service_lib.so")
lib_client = load_lib("libsniffer_client.so")

# --- Define Argument Types ---

lib_service.service_create.argtypes = [ctypes.c_char_p]
lib_service.service_create.restype = ctypes.c_void_p
lib_service.service_run.argtypes = [ctypes.c_void_p]
lib_service.service_stop.argtypes = [ctypes.c_void_p]
lib_service.service_destroy.argtypes = [ctypes.c_void_p]

lib_client.client_create.argtypes = [ctypes.c_char_p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint32]
lib_client.client_create.restype = ctypes.c_void_p
lib_client.client_start.argtypes = [ctypes.c_void_p]
lib_client.client_start.restype = ctypes.c_int
lib_client.client_stop.argtypes = [ctypes.c_void_p]
lib_client.client_destroy.argtypes = [ctypes.c_void_p]
lib_client.client_send_log_enable.argtypes = [ctypes.c_void_p, ctypes.c_bool]
lib_client.client_send_raw_command.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]

# --- Helper Functions ---

def create_config(filename, port):
    with open(filename, "w") as f:
        f.write(f"car_system_can_name=vcan0\n")
        f.write(f"car_computer_can_name=vcan1\n")
        f.write(f"external_service_port={port}\n")

def run_test():
    print("=== Starting MainService Integration Test ===")

    # 1. Setup Emulators
    print("Starting Emulators...")
    car_system_exe = "../build_tests/car_system_emulator"
    car_computer_exe = "../build_tests/car_computer_emulator"
    
    if not os.path.exists(car_system_exe) or not os.path.exists(car_computer_exe):
        print("Emulators not found. Build them first.")
        sys.exit(1)

    proc_sys = subprocess.Popen([car_system_exe, "vcan0"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    proc_comp = subprocess.Popen([car_computer_exe, "vcan1"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    time.sleep(1)

    # 2. Setup MainService
    config_file = "test_sniffer.prop"
    create_config(config_file, 9095)
    
    print("Starting MainService...")
    service_handle = lib_service.service_create(config_file.encode('utf-8'))
    
    service_thread = threading.Thread(target=lib_service.service_run, args=(service_handle,))
    service_thread.start()
    
    time.sleep(1) # Wait for Sniffer to start

    # 3. Test Connectivity & Logging
    print("Testing Connectivity (Port 9095)...")
    client = lib_client.client_create(b"127.0.0.1", 9095, 9096, 500)
    if not client:
        print("Failed to create client")
        sys.exit(1)
        
    if lib_client.client_start(client) != 0:
        print("Failed to start client")
        sys.exit(1)

    print("Enabling Logging...")
    lib_client.client_send_log_enable(client, True)
    
    # Wait a bit to verify no crash (real verification would need reading messages)
    time.sleep(2)
    
    # 4. Test Dynamic Reconfiguration
    print("Testing Dynamic Reconfiguration (Switch to Port 9099)...")
    new_params = b"external_service_port=9099"
    data_arr = (ctypes.c_uint8 * len(new_params))(*new_params)
    
    lib_client.client_send_raw_command(client, CMD_SET_PARAMS, data_arr, len(new_params))
    
    print("Waiting for Restart...")
    time.sleep(3) # Give it time to restart
    
    # Old client should be disconnected/useless now.
    lib_client.client_stop(client)
    lib_client.client_destroy(client)
    
    # 5. Verify New Port
    print("Connecting to New Port (9099)...")
    client_new = lib_client.client_create(b"127.0.0.1", 9099, 9097, 500)
    
    if lib_client.client_start(client_new) != 0:
        print("FAILED: Could not connect to new port 9099")
        # Cleanup
        lib_service.service_stop(service_handle)
        service_thread.join()
        proc_sys.terminate()
        proc_comp.terminate()
        sys.exit(1)
        
    print("SUCCESS: Connected to new port")
    
    # 6. Cleanup
    print("Cleaning up...")
    lib_client.client_stop(client_new)
    lib_client.client_destroy(client_new)
    
    lib_service.service_stop(service_handle)
    service_thread.join()
    lib_service.service_destroy(service_handle)
    
    proc_sys.terminate()
    proc_comp.terminate()
    proc_sys.wait()
    proc_comp.wait()
    
    if os.path.exists(config_file):
        os.remove(config_file)

    print("=== Test Passed ===")

if __name__ == "__main__":
    run_test()
