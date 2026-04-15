import json
import os

class ProfileManager:
    # Known IDs database
    KNOWN_IDS = {
        0x7DF: {"protocol": "OBD2", "has_pid": True, "pid_index": 2, "name": "OBD2 Request"},
        0x7E0: {"protocol": "OBD2", "has_pid": True, "pid_index": 2, "name": "Engine ECU Request"},
        0x7E8: {"protocol": "OBD2", "has_pid": True, "pid_index": 2, "name": "Engine ECU Response"},
        0x7E9: {"protocol": "OBD2", "has_pid": True, "pid_index": 2, "name": "Transmission ECU Response"},
    }

    # Default PID config per protocol
    PROTOCOL_DEFAULTS = {
        "OBD2": {"has_pid": True, "pid_index": 2},
        "UDS": {"has_pid": True, "pid_index": 2}, # Often 2 or 3 depending on addressing
        "ISO-TP": {"has_pid": False, "pid_index": 0}, # Usually handles fragmentation
        "J1939": {"has_pid": False, "pid_index": 0},
        "Manual": {"has_pid": False, "pid_index": 0}
    }

    def __init__(self, filename="profile.json"):
        self.filename = filename
        self.data = {
            "decoders": {}, # Key: CAN ID (hex string) or ID_PID, Value: List of signals
            "message_configs": {}, # Key: CAN ID (hex string), Value: Protocol config
            "controls_mapping": {} # Key: control name (steering, throttle, brake), Value: mapping config
        }
        self.load()

    def load(self):
        if os.path.exists(self.filename):
            self.load_from_file(self.filename)

    def load_from_file(self, filepath):
        try:
            with open(filepath, 'r') as f:
                loaded_data = json.load(f)
                self.data = {
                    "decoders": loaded_data.get("decoders", {}),
                    "message_configs": loaded_data.get("message_configs", {}),
                    "controls_mapping": loaded_data.get("controls_mapping", {})
                }
                self.filename = filepath
                print(f"Profile loaded from {filepath}")
                return True
        except Exception as e:
            print(f"Failed to load profile from {filepath}: {e}")
            return False

    def save(self):
        try:
            with open(self.filename, 'w') as f:
                json.dump(self.data, f, indent=4)
            print("Profile saved successfully.") 
        except Exception as e:
            print(f"Failed to save profile: {e}")

    def get_signals(self, can_id, pid=None):
        hex_id = hex(can_id)
        key = f"{hex_id}_{pid}" if pid is not None else hex_id
        return self.data["decoders"].get(key, [])

    def set_signals(self, can_id, signals, pid=None):
        hex_id = hex(can_id)
        key = f"{hex_id}_{pid}" if pid is not None else hex_id
        self.data["decoders"][key] = signals

    def get_message_config(self, can_id):
        hex_id = hex(can_id)
        # Check if config exists
        if hex_id in self.data["message_configs"]:
            return self.data["message_configs"][hex_id]
        
        # If not, check known IDs
        if can_id in self.KNOWN_IDS:
            return self.KNOWN_IDS[can_id].copy()
            
        # Default
        return {
            "protocol": "Manual",
            "has_pid": False,
            "pid_index": 0
        }

    def set_message_config(self, can_id, config):
        hex_id = hex(can_id)
        self.data["message_configs"][hex_id] = config

    def get_protocol_defaults(self, protocol):
        return self.PROTOCOL_DEFAULTS.get(protocol, {"has_pid": False, "pid_index": 0})

    def set_control_mapping(self, control_name, can_id, pid, signal_index, min_val, max_val, reverse_formula, start_byte, length, sig_type):
        """
        Stores the complete mapping required for both UI normalization and future active injection.
        """
        self.data["controls_mapping"][control_name] = {
            "can_id": can_id,
            "pid": pid,
            "signal_index": signal_index,
            "min_val": min_val,
            "max_val": max_val,
            "reverse_formula": reverse_formula,
            "start_byte": start_byte,
            "length": length,
            "type": sig_type,
            "base_payload": None # Will be updated dynamically in passive mode
        }

    def get_control_mapping(self, control_name):
        return self.data["controls_mapping"].get(control_name)

    def update_control_base_payload(self, control_name, payload_hex):
        """Called during passive mode to maintain a snapshot of the last valid frame"""
        mapping = self.data["controls_mapping"].get(control_name)
        if mapping:
            mapping["base_payload"] = payload_hex
