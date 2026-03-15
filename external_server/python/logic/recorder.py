import csv
import time

class Recorder:
    def __init__(self, max_messages=100000):
        # We don't need 'self.recording' flag anymore since it's controlled by UI
        # But we keep it if needed for compatibility, although we won't strictly rely on it 
        # to filter, we'll just add messages when the UI tells us to.
        self.messages = []
        self.max_messages = max_messages
        self.limit_reached = False

    def clear(self):
        self.messages = []
        self.limit_reached = False

    def add_message(self, timestamp, direction, can_id, data):
        if len(self.messages) >= self.max_messages:
            self.limit_reached = True
            return False # Indicate failure due to limit
            
        self.messages.append({
            "timestamp": timestamp,
            "direction": direction,
            "can_id": hex(can_id),
            "data": data.hex() if isinstance(data, bytes) else str(data)
        })
        return True

    def get_messages(self):
        return self.messages

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
