import ctypes
import os
import sys
import struct # Added import
from .protocol import *

# Global debug flag
DEBUG_MODE = False

class UdpClient:
    def __init__(self, ip="127.0.0.1", remote_port=9095, local_port=9096, keep_alive_ms=500):
        self.lib = self._load_library()
        self._setup_argtypes()
        
        self.handle = self.lib.client_create(
            ip.encode('utf-8'), 
            remote_port, 
            local_port, 
            keep_alive_ms
        )
        
        if not self.handle:
            raise RuntimeError("Failed to create Sniffer Client")

    def _load_library(self):
        # Try relative to this file
        base_path = os.path.dirname(__file__)
        paths = [
            os.path.join(base_path, '../../build/libsniffer_client.so'), # If running from src
            os.path.join(base_path, '../../../build/external_server/libsniffer_client.so'), # Standard build
            os.path.abspath('./backend/libsniffer_client.so'), # Release structure
            os.path.abspath('./libsniffer_client.so') # Current dir
        ]
        
        for path in paths:
            if os.path.exists(path):
                return ctypes.CDLL(path)
        
        # Fallback for tests
        test_path = os.path.abspath("../build_tests/libsniffer_client.so")
        if os.path.exists(test_path):
            return ctypes.CDLL(test_path)
            
        raise FileNotFoundError("libsniffer_client.so not found")

    def _setup_argtypes(self):
        self.lib.client_create.argtypes = [ctypes.c_char_p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint32]
        self.lib.client_create.restype = ctypes.c_void_p

        self.lib.client_start.argtypes = [ctypes.c_void_p]
        self.lib.client_start.restype = ctypes.c_int

        self.lib.client_stop.argtypes = [ctypes.c_void_p]
        self.lib.client_destroy.argtypes = [ctypes.c_void_p]

        self.lib.client_send_log_enable.argtypes = [ctypes.c_void_p, ctypes.c_bool]
        
        self.lib.client_send_injection.argtypes = [ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8]

        self.lib.client_set_filters.argtypes = [ctypes.c_void_p, ctypes.POINTER(CanFilterRule), ctypes.c_size_t]

        # Updated API
        self.lib.client_read_message.argtypes = [
            ctypes.c_void_p, 
            ctypes.POINTER(ctypes.c_uint32), # command
            ctypes.POINTER(ctypes.c_uint8),  # data
            ctypes.POINTER(ctypes.c_uint32), # len
            ctypes.c_int                     # timeout
        ]
        self.lib.client_read_message.restype = ctypes.c_int

    def start(self):
        return self.lib.client_start(self.handle) == 0

    def stop(self):
        self.lib.client_stop(self.handle)

    def close(self):
        self.lib.client_destroy(self.handle)
        self.handle = None

    def set_logging(self, enable):
        self.lib.client_send_log_enable(self.handle, enable)

    def inject_frame(self, target, can_id, data):
        # target: 1=System, 2=Car
        data_arr = (ctypes.c_uint8 * len(data))(*data)
        self.lib.client_send_injection(self.handle, target, can_id, data_arr, len(data))

    def set_filters(self, rules):
        # rules: list of CanFilterRule objects
        if not rules:
            self.lib.client_set_filters(self.handle, None, 0)
            return

        rules_arr = (CanFilterRule * len(rules))(*rules)
        self.lib.client_set_filters(self.handle, rules_arr, len(rules))

    def read_message(self, timeout_ms=100):
        command = ctypes.c_uint32()
        length = ctypes.c_uint32()
        data = (ctypes.c_uint8 * 64)() # Max buffer
        
        res = self.lib.client_read_message(self.handle, ctypes.byref(command), data, ctypes.byref(length), timeout_ms)
        
        if res > 0:
            if DEBUG_MODE:
                print(f"[Python] read_message: res={res}, command=0x{command.value:X}, length={length.value}")
            
            # Return a dict or object with command and data
            # For compatibility with existing code, we might want to return an object with similar fields
            # But existing code expects ExternalCanfdMessage.
            # Let's return a simple object.
            
            class Message:
                pass
            
            msg = Message()
            msg.command = command.value
            msg.data = bytes(data[:length.value])
            
            # If it's a CAN frame, parse it for convenience
            if length.value >= 16: # sizeof(can_frame)
                # Parse can_frame manually
                # struct can_frame { canid_t can_id; uint8_t can_dlc; uint8_t __pad; uint8_t __res0; uint8_t __res1; uint8_t data[8]; };
                # canid_t is uint32
                
                # Note: This assumes the payload IS a can_frame.
                # If command is CMD_CANBUS_DATA, it might be V1 payload?
                # Sniffer sends raw can_frame in V1 payload.
                
                msg.can_id = struct.unpack("I", msg.data[0:4])[0]
                msg.dlc = msg.data[4]
                msg.frame_data = msg.data[8:8+msg.dlc]
            else:
                if DEBUG_MODE:
                    print(f"[Python] Warning: Message length {length.value} < 16, cannot parse as can_frame")
            
            return msg
        else:
            # print(f"[Python] read_message: res={res} (Timeout or Empty)")
            pass

        return None
