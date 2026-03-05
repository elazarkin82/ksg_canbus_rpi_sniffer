import csv
import time

class Recorder:
    def __init__(self):
        self.recording = False
        self.messages = []

    def start(self):
        self.recording = True
        self.messages = []

    def stop(self):
        self.recording = False

    def add_message(self, timestamp, direction, can_id, data):
        if self.recording:
            self.messages.append({
                "timestamp": timestamp,
                "direction": direction,
                "can_id": hex(can_id),
                "data": data.hex() if isinstance(data, bytes) else str(data)
            })

    def save_to_file(self, filename):
        if not self.messages:
            return False
        
        try:
            with open(filename, 'w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=["timestamp", "direction", "can_id", "data"])
                writer.writeheader()
                writer.writerows(self.messages)
            return True
        except Exception as e:
            print(f"Failed to save recording: {e}")
            return False
