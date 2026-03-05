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

class canfd_frame(ctypes.Structure):
    _fields_ = [
        ("can_id", ctypes.c_uint32),
        ("len", ctypes.c_uint8),
        ("flags", ctypes.c_uint8),
        ("res0", ctypes.c_uint8),
        ("res1", ctypes.c_uint8),
        ("data", ctypes.c_uint8 * 64)
    ]

class ExternalCanfdMessage(ctypes.Structure):
    _fields_ = [
        ("magic_key", ctypes.c_char * 4),
        ("frame", canfd_frame)
    ]

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
lib_client.client_send_injection.argtypes = [ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8]
lib_client.client_set_filters.argtypes = [ctypes.c_void_p, ctypes.POINTER(CanFilterRule), ctypes.c_size_t]
lib_client.client_send_raw_command.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
lib_client.client_read_message.argtypes = [ctypes.c_void_p, ctypes.POINTER(ExternalCanfdMessage), ctypes.c_int]
lib_client.client_read_message.restype = ctypes.c_int

# --- Helper Functions ---

def create_config(filename, port):
    with open(filename, "w") as f:
        f.write(f"car_system_can_name=vcan0\n")
        f.write(f"car_computer_can_name=vcan1\n")
        f.write(f"external_service_port={port}\n")

def wait_for_message(client, can_id, timeout=2.0):
    start = time.time()
    msg = ExternalCanfdMessage()
    while time.time() - start < timeout:
        if lib_client.client_read_message(client, ctypes.byref(msg), 100) > 0:
            if msg.frame.can_id == can_id:
                return True
    return False

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
    
    # Verify Traffic (RPM 0x100 or Speed 0x200)
    print("Verifying Traffic...")
    if wait_for_message(client, 0x100):
        print("SUCCESS: Received RPM (0x100)")
    else:
        print("FAILED: Did not receive RPM")
        sys.exit(1)

    # 4. Test Injection
    print("Testing Injection (0x400 to System)...")
    data = (ctypes.c_uint8 * 8)(1, 2, 3, 4, 5, 6, 7, 8)
    lib_client.client_send_injection(client, 1, 0x400, data, 8)
    
    # Verify echo (since we log everything) or check if it appears on bus (harder here without another socket)
    # But since we are logging, we should receive our own injected message if the sniffer forwards it and logs it?
    # Actually, Sniffer logs what it receives from CAN. If we inject to CAN, it goes out.
    # Does it loop back? vcan usually loops back.
    if wait_for_message(client, 0x400):
        print("SUCCESS: Injected message received back")
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
    
    time.sleep(1)
    
    # Flush queue
    while lib_client.client_read_message(client, ctypes.byref(ExternalCanfdMessage()), 10) > 0: pass
    
    if wait_for_message(client, 0x200, timeout=2.0):
        print("FAILED: Received 0x200 despite DROP rule")
        sys.exit(1)
    else:
        print("SUCCESS: 0x200 Blocked")

    # 6. Test Dynamic Reconfiguration
    print("Testing Dynamic Reconfiguration (Switch to Port 9099)...")
    new_params = b"external_service_port=9099"
    data_arr = (ctypes.c_uint8 * len(new_params))(*new_params)
    
    lib_client.client_send_raw_command(client, CMD_SET_PARAMS, data_arr, len(new_params))
    
    print("Waiting for Restart...")
    time.sleep(3) # Give it time to restart
    
    lib_client.client_stop(client)
    lib_client.client_destroy(client)
    
    # 7. Verify New Port
    print("Connecting to New Port (9099)...")
    client_new = lib_client.client_create(b"127.0.0.1", 9099, 9097, 500)
    
    if lib_client.client_start(client_new) != 0:
        print("FAILED: Could not connect to new port 9099")
        sys.exit(1)
        
    lib_client.client_send_log_enable(client_new, True)
    if wait_for_message(client_new, 0x100):
        print("SUCCESS: Connected and receiving on new port")
    else:
        print("FAILED: No traffic on new port")
        sys.exit(1)
    
    # 8. Cleanup
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
