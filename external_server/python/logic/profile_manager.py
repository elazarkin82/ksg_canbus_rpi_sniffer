import json
import os

class ProfileManager:
    def __init__(self, filename="profile.json"):
        self.filename = filename
        self.data = {
            "decoders": {}, # Key: CAN ID (hex string), Value: List of signals
            "mappings": {}
        }
        self.load()

    def load(self):
        if os.path.exists(self.filename):
            try:
                with open(self.filename, 'r') as f:
                    self.data = json.load(f)
            except Exception as e:
                print(f"Failed to load profile: {e}")

    def save(self):
        try:
            with open(self.filename, 'w') as f:
                json.dump(self.data, f, indent=4)
        except Exception as e:
            print(f"Failed to save profile: {e}")

    def get_signals(self, can_id):
        # can_id should be int
        hex_id = hex(can_id)
        return self.data["decoders"].get(hex_id, [])

    def set_signals(self, can_id, signals):
        hex_id = hex(can_id)
        self.data["decoders"][hex_id] = signals
        self.save()
