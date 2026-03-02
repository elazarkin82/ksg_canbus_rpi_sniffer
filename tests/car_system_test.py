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
    print("Starting Car System Emulator Test...")
    
    exe_path = "../build_tests/car_system_emulator"
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
        # 1. Test Broadcast
        print("Waiting for broadcast messages...")
        start_time = time.time()
        received_rpm = False
        received_speed = False
        
        while time.time() - start_time < 2:
            readable, _, _ = select.select([sock], [], [], 0.1)
            if readable:
                frame_data = sock.recv(16)
                can_id, dlc, data = parse_can_frame(frame_data)
                
                if can_id == 0x100: # RPM
                    print(f"Received RPM Broadcast: ID=0x{can_id:X}")
                    received_rpm = True
                elif can_id == 0x200: # Speed
                    print(f"Received Speed Broadcast: ID=0x{can_id:X}")
                    received_speed = True
            
            if received_rpm and received_speed:
                break
        
        if not (received_rpm and received_speed):
            print("FAILED: Did not receive broadcast messages")
            proc.terminate()
            sys.exit(1)

        # 2. Test OBD Request
        print("Sending OBD RPM Request...")
        # ID 0x7DF, Data: [02, 01, 0C, 00...] (Len=2, Mode=1, PID=0C=RPM)
        req_frame = create_can_frame(0x7DF, b'\x02\x01\x0C')
        sock.send(req_frame)
        
        start_time = time.time()
        received_response = False
        
        while time.time() - start_time < 1:
            readable, _, _ = select.select([sock], [], [], 0.1)
            if readable:
                frame_data = sock.recv(16)
                can_id, dlc, data = parse_can_frame(frame_data)
                
                if can_id == 0x7E8: # Response ID
                    # Check if it's response to RPM (Mode 41, PID 0C)
                    # Data: [Len, 41, 0C, A, B...]
                    if data[1] == 0x41 and data[2] == 0x0C:
                        print(f"Received OBD Response: {list(data)}")
                        received_response = True
                        break
        
        if not received_response:
            print("FAILED: Did not receive OBD response")
            proc.terminate()
            sys.exit(1)

        print("PASSED: Car System Emulator Test")

    except Exception as e:
        print(f"Error: {e}")
        proc.terminate()
        sys.exit(1)
    finally:
        proc.terminate()
        proc.wait()

if __name__ == "__main__":
    run_test()
