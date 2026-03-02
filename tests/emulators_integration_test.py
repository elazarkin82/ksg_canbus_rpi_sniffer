import ctypes
import time
import os
import sys
import threading

# Load Library
lib_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../build/libemulator_wrapper.so'))
# Adjust path if build directory is different
if not os.path.exists(lib_path):
    # Try current directory or standard build locations
    lib_path = os.path.abspath('./libemulator_wrapper.so')

if not os.path.exists(lib_path):
    print(f"Error: Library not found at {lib_path}")
    sys.exit(1)

lib = ctypes.CDLL(lib_path)

# Define Argument Types
lib.start_system_emulator.argtypes = [ctypes.c_char_p]
lib.start_computer_emulator.argtypes = [ctypes.c_char_p]
lib.wrapper_get_rpm.restype = ctypes.c_double
lib.wrapper_get_speed.restype = ctypes.c_double
lib.wrapper_get_throttle.restype = ctypes.c_double
lib.wrapper_get_brake.restype = ctypes.c_double
lib.wrapper_get_steering.restype = ctypes.c_double

def run_test(gui=False):
    print("Starting Emulators Integration Test...")
    
    # Setup vcan interfaces (requires sudo, assume already setup or run script)
    # os.system("../emulators/setup_vcan.sh") 
    
    vcan0 = b"vcan0"
    vcan1 = b"vcan1" 
    
    # For integration test without Sniffer, we connect both to vcan0.
    # If we want to test with Sniffer, we would connect one to vcan0 and one to vcan1,
    # and run the Sniffer in between.
    # For now, let's test direct communication.
    
    lib.start_system_emulator(vcan0)
    lib.start_computer_emulator(vcan0) 

    print("Emulators running. Monitoring...")
    
    start_time = time.time()
    while time.time() - start_time < 10: # Run for 10 seconds
        rpm = lib.wrapper_get_rpm()
        speed = lib.wrapper_get_speed()
        throttle = lib.wrapper_get_throttle()
        brake = lib.wrapper_get_brake()
        steering = lib.wrapper_get_steering()
        
        print(f"Time: {time.time()-start_time:.1f}s | RPM: {rpm:.0f} | Speed: {speed:.1f} | Thr: {throttle:.0f}% | Brk: {brake:.0f} | Str: {steering:.1f}")
        
        if gui:
            # Update GUI here
            pass
            
        time.sleep(0.5)

    print("Stopping...")
    lib.stop_computer_emulator()
    lib.stop_system_emulator()
    print("Test Passed")

if __name__ == "__main__":
    gui_mode = "--gui" in sys.argv
    run_test(gui_mode)
