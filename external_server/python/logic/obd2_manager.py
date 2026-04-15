import threading
import time
import struct

class OBD2Manager:
    # Supported PIDs and their decoding logic
    PIDS = {
        0x04: {"name": "Engine Load", "unit": "%", "len": 1, "formula": lambda d: d[0] * 100.0 / 255.0},
        0x05: {"name": "Coolant Temp", "unit": "°C", "len": 1, "formula": lambda d: d[0] - 40},
        0x0A: {"name": "Fuel Pressure", "unit": "kPa", "len": 1, "formula": lambda d: d[0] * 3},
        0x0B: {"name": "Intake MAP", "unit": "kPa", "len": 1, "formula": lambda d: d[0]},
        0x0C: {"name": "Engine RPM", "unit": "rpm", "len": 2, "formula": lambda d: (d[0] * 256 + d[1]) / 4.0},
        0x0D: {"name": "Vehicle Speed", "unit": "km/h", "len": 1, "formula": lambda d: d[0]},
        0x0E: {"name": "Timing Advance", "unit": "°", "len": 1, "formula": lambda d: d[0] / 2.0 - 64.0},
        0x10: {"name": "MAF Air Flow", "unit": "g/s", "len": 2, "formula": lambda d: (d[0] * 256 + d[1]) / 100.0},
        0x11: {"name": "Throttle Pos", "unit": "%", "len": 1, "formula": lambda d: d[0] * 100.0 / 255.0},
        0x1F: {"name": "Run Time", "unit": "sec", "len": 2, "formula": lambda d: d[0] * 256 + d[1]},
        0x2F: {"name": "Fuel Level", "unit": "%", "len": 1, "formula": lambda d: d[0] * 100.0 / 255.0},
    }

    def __init__(self, client, on_update_callback):
        self.client = client
        self.on_update = on_update_callback # Callback(pid, value_str)
        self.running = False
        self.active_pids = set()
        self.thread = None
        self.interval = 0.1 # 100ms default

    def start_polling(self):
        if self.running: return
        self.running = True
        self.thread = threading.Thread(target=self._poll_loop)
        self.thread.daemon = True
        self.thread.start()

    def stop_polling(self):
        self.running = False
        if self.thread:
            self.thread.join(timeout=1.0)

    def set_active_pids(self, pids):
        self.active_pids = set(pids)

    def _poll_loop(self):
        while self.running:
            if not self.active_pids:
                time.sleep(0.5)
                continue

            for pid in list(self.active_pids):
                if not self.running: break
                self._send_request(pid)
                time.sleep(self.interval)

    def _send_request(self, pid):
        # ID 0x7DF (Broadcast), Data: [0x02, 0x01, PID, 0, 0, 0, 0, 0]
        data = [0x02, 0x01, pid, 0x00, 0x00, 0x00, 0x00, 0x00]
        try:
            # Target 2 = Car Computer Bus (where ECU is)
            self.client.inject_frame(2, 0x7DF, bytes(data))
        except Exception as e:
            print(f"Error sending OBD2 request: {e}")

    def process_message(self, can_id, data):
        # Check for response (0x7E8 - 0x7EF)
        if 0x7E8 <= can_id <= 0x7EF:
            # Check for Service 01 Response (0x41)
            # Data format: [PCI, Mode, PID, A, B, C, D]
            if len(data) >= 3 and data[1] == 0x41:
                pid = data[2]
                if pid in self.PIDS:
                    try:
                        # Extract data based on length
                        value_data = data[3:]
                        formula = self.PIDS[pid]["formula"]
                        val = formula(value_data)
                        
                        # Format
                        if isinstance(val, float):
                            val_str = f"{val:.2f}"
                        else:
                            val_str = str(val)
                            
                        self.on_update(pid, val_str)
                    except Exception as e:
                        print(f"Error decoding PID {hex(pid)}: {e}")
