import json
import os

class ProfileManager:
    def __init__(self, filename="profile.json"):
        self.filename = filename
        self.data = {
            "decoders": {}, # Key: CAN ID (hex string), Value: List of signals
            "message_configs": {}, # NEW: Key: CAN ID (hex string), Value: Protocol config
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
                # Merge keys to prevent errors if a key is missing in an old profile
                # Or replace entirely? Usually loading a profile implies replacing current config.
                # Let's replace but keep structure safe.
                self.data = {
                    "decoders": loaded_data.get("decoders", {}),
                    "message_configs": loaded_data.get("message_configs", {}),
                    "mappings": loaded_data.get("mappings", {})
                }
                # Update filename to the loaded one so save() writes back to it?
                # Or keep original filename? Usually "Load" implies switching context.
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
        # For PID-based decoding, we can create a unique key
        key = f"{hex_id}_{pid}" if pid is not None else hex_id
        return self.data["decoders"].get(key, [])

    def set_signals(self, can_id, signals, pid=None):
        hex_id = hex(can_id)
        key = f"{hex_id}_{pid}" if pid is not None else hex_id
        self.data["decoders"][key] = signals
        # self.save() # Removed automatic save

    def get_message_config(self, can_id):
        hex_id = hex(can_id)
        # Return a default config if not found
        return self.data["message_configs"].get(hex_id, {
            "protocol": "Manual",
            "has_pid": False,
            "pid_index": 0
        })

    def set_message_config(self, can_id, config):
        hex_id = hex(can_id)
        self.data["message_configs"][hex_id] = config
        # self.save() # Removed automatic save
