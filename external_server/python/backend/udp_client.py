import ctypes
import os
import sys
from .protocol import *

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
            os.path.abspath('./libsniffer_client.so') # Current dir
        ]
        
        for path in paths:
            if os.path.exists(path):
                return ctypes.CDLL(path)
        
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

        self.lib.client_read_message.argtypes = [ctypes.c_void_p, ctypes.POINTER(ExternalCanfdMessage), ctypes.c_int]
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
        msg = ExternalCanfdMessage()
        if self.lib.client_read_message(self.handle, ctypes.byref(msg), timeout_ms) > 0:
            return msg
        return None
