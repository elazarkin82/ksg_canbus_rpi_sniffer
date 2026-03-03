import json
import os

class ProfileManager:
    def __init__(self, filename="profile.json"):
        self.filename = filename
        self.data = {
            "decoders": {},
            "mappings": {}
        }

    def load(self):
        if os.path.exists(self.filename):
            with open(self.filename, 'r') as f:
                self.data = json.load(f)

    def save(self):
        with open(self.filename, 'w') as f:
            json.dump(self.data, f, indent=4)

    def get_decoder(self, can_id):
        return self.data["decoders"].get(str(can_id))

    def set_decoder(self, can_id, config):
        self.data["decoders"][str(can_id)] = config
