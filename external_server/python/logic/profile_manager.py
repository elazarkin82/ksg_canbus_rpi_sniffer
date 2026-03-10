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
            "mappings": {}
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
                    "mappings": loaded_data.get("mappings", {})
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
