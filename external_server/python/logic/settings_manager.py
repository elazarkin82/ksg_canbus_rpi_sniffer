import json
import os

class SettingsManager:
    DEFAULT_SETTINGS = {
        "sniffer_ip": "127.0.0.1",
        "sniffer_port": 9095,
        "local_port": 9096,
        "keep_alive_ms": 500
    }

    def __init__(self, filename="settings.json"):
        # Determine path relative to this file or main script
        # Let's assume it's in the same dir as main.py or current working dir
        self.filepath = os.path.abspath(filename)
        self.settings = self.DEFAULT_SETTINGS.copy()
        self.load()

    def load(self):
        if os.path.exists(self.filepath):
            try:
                with open(self.filepath, 'r') as f:
                    data = json.load(f)
                    # Update settings with loaded data, preserving defaults for missing keys
                    self.settings.update(data)
            except Exception as e:
                print(f"Failed to load settings: {e}")
        else:
            self.save() # Create default file

    def save(self):
        try:
            with open(self.filepath, 'w') as f:
                json.dump(self.settings, f, indent=4)
        except Exception as e:
            print(f"Failed to save settings: {e}")
