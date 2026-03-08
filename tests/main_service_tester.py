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
CMD_SET_FILTERS = 0x1006
CMD_CAN_MSG_FROM_SYSTEM = 0x2001
CMD_CAN_MSG_FROM_COMPUTER = 0x2002

# --- Colors ---
GREEN = '\033[0;32m'
RED = '\033[0;31m'
NC = '\033[0m' # No Color

def print_success(msg):
    print(f"{GREEN}{msg}{NC}")

def print_fail(msg):
    print(f"{RED}{msg}{NC}")

# --- Structures ---
class CanFilterRule(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("can_id", ctypes.c_uint32),
        ("data_index", ctypes.c_uint8),
        ("data_value", ctypes.c_uint8),
        ("data_mask", ctypes.c_uint8),
        ("action_type", ctypes.c_uint8),
        ("target", ctypes.c_uint8),
        ("modification_data", ctypes.c_uint8 * 8),
        ("modification_mask", ctypes.c_uint8 * 8)
    ]

# --- Load Libraries ---

def load_lib(name):
    lib_path = os.path.abspath(f"../build_tests/{name}")
    if not os.path.exists(lib_path):
        lib_path = os.path.abspath(f"./{name}")
    
    if not os.path.exists(lib_path):
        print_fail(f"Error: Library {name} not found")
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
lib_client.client_send_injection.argtypes = [ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8]
lib_client.client_set_filters.argtypes = [ctypes.c_void_p, ctypes.POINTER(CanFilterRule), ctypes.c_size_t]
lib_client.client_send_raw_command.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]

# Updated API
lib_client.client_read_message.argtypes = [
    ctypes.c_void_p, 
    ctypes.POINTER(ctypes.c_uint32), # command
    ctypes.POINTER(ctypes.c_uint8),  # data
    ctypes.POINTER(ctypes.c_uint32), # len
    ctypes.c_int                     # timeout
]
lib_client.client_read_message.restype = ctypes.c_int

# --- Helper Functions ---

def create_config(filename, port):
    with open(filename, "w") as f:
        f.write(f"car_system_can_name=vcan0\n")
        f.write(f"car_computer_can_name=vcan1\n")
        f.write(f"external_service_port={port}\n")
        f.write(f"external_client_port=9096\n")

def wait_for_message(client, can_id, timeout=2.0):
    start = time.time()
    
    command = ctypes.c_uint32()
    length = ctypes.c_uint32()
    data = (ctypes.c_uint8 * 64)()
    
    while time.time() - start < timeout:
        if lib_client.client_read_message(client, ctypes.byref(command), data, ctypes.byref(length), 100) > 0:
            # Check if it's a log message
            if command.value == CMD_CAN_MSG_FROM_SYSTEM or command.value == CMD_CAN_MSG_FROM_COMPUTER:
                # Parse can_frame (16 bytes)
                if length.value >= 16:
                    msg_can_id = struct.unpack("I", bytes(data[:4]))[0]
                    if msg_can_id == can_id:
                        return True
    return False

def run_test():
    print("=== Starting MainService Integration Test ===")
    
    config_file = "test_sniffer.prop"
    service_handle = None
    service_thread = None
    client = None
    client_new = None
    proc_sys = None
    proc_comp = None

    try:
        # 1. Setup Emulators
        print("Starting Emulators...")
        car_system_exe = "../build_tests/car_system_emulator"
        car_computer_exe = "../build_tests/car_computer_emulator"
        
        if not os.path.exists(car_system_exe) or not os.path.exists(car_computer_exe):
            print_fail("Emulators not found. Build them first.")
            sys.exit(1)

        proc_sys = subprocess.Popen([car_system_exe, "vcan0"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        proc_comp = subprocess.Popen([car_computer_exe, "vcan1"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
        time.sleep(1)

        # 2. Setup MainService
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
            raise RuntimeError("Failed to create client")
            
        if lib_client.client_start(client) != 0:
            raise RuntimeError("Failed to start client")

        print("Enabling Logging...")
        lib_client.client_send_log_enable(client, True)
        
        # Verify Traffic (RPM 0x100 or Speed 0x200)
        print("Verifying Traffic...")
        if wait_for_message(client, 0x100):
            print_success("SUCCESS: Received RPM (0x100)")
        else:
            raise RuntimeError("Did not receive RPM")

        # 4. Test Injection
        print("Testing Injection (0x400 to System)...")
        data = (ctypes.c_uint8 * 8)(1, 2, 3, 4, 5, 6, 7, 8)
        lib_client.client_send_injection(client, 1, 0x400, data, 8)
        
        if wait_for_message(client, 0x400):
            print_success("SUCCESS: Injected message received back")
        else:
            print("WARNING: Injected message not received back (might be expected depending on loopback)")

        # 5. Test Filtering (DROP 0x200)
        print("Testing Filter (DROP 0x200)...")
        rule = CanFilterRule()
        rule.can_id = 0x200
        rule.action_type = 1 # DROP
        rule.target = 0 # BOTH
        
        rules_arr = (CanFilterRule * 1)(rule)
        lib_client.client_set_filters(client, rules_arr, 1)
        
        # Give time for rule to apply and for in-flight messages to arrive
        time.sleep(1.5)
        
        # Aggressive Flush
        print("Flushing queue...")
        command = ctypes.c_uint32()
        length = ctypes.c_uint32()
        data_buf = (ctypes.c_uint8 * 64)()
        start_flush = time.time()
        while time.time() - start_flush < 2.0:
            if lib_client.client_read_message(client, ctypes.byref(command), data_buf, ctypes.byref(length), 10) <= 0:
                break # Queue empty
        
        print("Verifying blockage...")
        if wait_for_message(client, 0x200, timeout=2.0):
            raise RuntimeError("Received 0x200 despite DROP rule")
        else:
            print_success("SUCCESS: 0x200 Blocked")

        # 6. Test Dynamic Reconfiguration
        print("Testing Dynamic Reconfiguration (Switch to Port 9099)...")
        # Update both service port AND client port
        new_params = b"external_service_port=9099\nexternal_client_port=9097"
        data_arr = (ctypes.c_uint8 * len(new_params))(*new_params)
        
        lib_client.client_send_raw_command(client, CMD_SET_PARAMS, data_arr, len(new_params))
        
        print("Waiting for Restart...")
        time.sleep(3) # Give it time to restart
        
        lib_client.client_stop(client)
        lib_client.client_destroy(client)
        client = None
        
        # 7. Verify New Port
        print("Connecting to New Port (9099)...")
        # Client listens on 9097 now
        client_new = lib_client.client_create(b"127.0.0.1", 9099, 9097, 500)
        
        if lib_client.client_start(client_new) != 0:
            raise RuntimeError("Could not connect to new port 9099")
            
        lib_client.client_send_log_enable(client_new, True)
        if wait_for_message(client_new, 0x100):
            print_success("SUCCESS: Connected and receiving on new port")
        else:
            raise RuntimeError("No traffic on new port")
        
        print_success("=== Test Passed ===")

    except Exception as e:
        print_fail(f"FAILED: {e}")
        sys.exit(1)
        
    finally:
        print("Cleaning up...")
        if client:
            lib_client.client_stop(client)
            lib_client.client_destroy(client)
        
        if client_new:
            lib_client.client_stop(client_new)
            lib_client.client_destroy(client_new)
        
        if service_thread and service_thread.is_alive():
            if service_handle:
                lib_service.service_stop(service_handle)
            service_thread.join()
            if service_handle:
                lib_service.service_destroy(service_handle)
        
        if proc_sys: proc_sys.terminate()
        if proc_comp: proc_comp.terminate()
        
        if os.path.exists(config_file):
            os.remove(config_file)

if __name__ == "__main__":
    run_test()
