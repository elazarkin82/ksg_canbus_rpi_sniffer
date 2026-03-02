#!/usr/bin/env python3

import subprocess
import time
import os
import sys
import socket
import struct
import select

# CAN Frame Structure
CAN_FRAME_FMT = "=IB3x8s"

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

def create_can_frame(can_id, data):
    can_dlc = len(data)
    data = data.ljust(8, b'\x00')
    return struct.pack(CAN_FRAME_FMT, can_id, can_dlc, data)

def run_test():
    print("Starting Car Computer Emulator Test...")
    
    exe_path = "../build_tests/car_computer_emulator"
    if not os.path.exists(exe_path):
        print(f"Error: {exe_path} not found")
        sys.exit(1)

    vcan = "vcan0"
    
    print(f"Launching {exe_path} on {vcan}")
    proc = subprocess.Popen([exe_path, vcan], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    sock = open_can_socket(vcan)
    if not sock:
        proc.terminate()
        sys.exit(1)

    try:
        # 1. Send Speed & RPM (Simulate System)
        print("Sending Speed (100 km/h) and RPM (3000)...")
        
        # Speed ID 0x200, Data [100]
        sock.send(create_can_frame(0x200, b'\x64'))
        
        # RPM ID 0x100, Data [High, Low] (3000 * 4 = 12000 = 0x2EE0)
        sock.send(create_can_frame(0x100, b'\x2E\xE0'))
        
        # 2. Wait for Control Commands
        print("Waiting for control commands...")
        start_time = time.time()
        received_throttle = False
        received_steering = False
        
        while time.time() - start_time < 2:
            readable, _, _ = select.select([sock], [], [], 0.1)
            if readable:
                frame_data = sock.recv(16)
                can_id, dlc, data = parse_can_frame(frame_data)
                
                if can_id == 0x400: # Throttle/Brake
                    print(f"Received Throttle/Brake Command: {list(data)}")
                    received_throttle = True
                elif can_id == 0x401: # Steering
                    print(f"Received Steering Command: {list(data)}")
                    received_steering = True
            
            if received_throttle and received_steering:
                break
        
        if not (received_throttle and received_steering):
            print("FAILED: Did not receive control commands")
            proc.terminate()
            sys.exit(1)

        print("PASSED: Car Computer Emulator Test")

    except Exception as e:
        print(f"Error: {e}")
        proc.terminate()
        sys.exit(1)
    finally:
        proc.terminate()
        proc.wait()

if __name__ == "__main__":
    run_test()
