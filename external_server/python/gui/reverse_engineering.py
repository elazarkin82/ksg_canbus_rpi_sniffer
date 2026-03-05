import tkinter as tk
from tkinter import ttk, filedialog
import time
from logic.recorder import Recorder
from gui.decoder_editor import DecoderEditorDialog
from logic.decoder import Decoder

class ReverseEngineeringPanel(ttk.Frame):
    def __init__(self, parent, profile_manager):
        super().__init__(parent)
        self.profile_manager = profile_manager
        self.recorder = Recorder()
        self.messages_map = {} # ID -> Item ID in tree
        self.paused = False
        
        self._setup_ui()

    def _setup_ui(self):
        # Toolbar
        toolbar = ttk.Frame(self)
        toolbar.pack(fill=tk.X, padx=5, pady=5)
        
        self.btn_record = ttk.Button(toolbar, text="Record", command=self.toggle_record)
        self.btn_record.pack(side=tk.LEFT)
        
        self.btn_pause = ttk.Button(toolbar, text="Pause View", command=self.toggle_pause)
        self.btn_pause.pack(side=tk.LEFT, padx=5)
        
        ttk.Button(toolbar, text="Clear", command=self.clear_table).pack(side=tk.LEFT, padx=5)

        # Table
        columns = ("ID", "Dir", "Count", "Interval", "Data", "Decoded")
        self.tree = ttk.Treeview(self, columns=columns, show="headings")
        
        self.tree.heading("ID", text="ID")
        self.tree.heading("Dir", text="Dir")
        self.tree.heading("Count", text="Count")
        self.tree.heading("Interval", text="Interval (ms)")
        self.tree.heading("Data", text="Data")
        self.tree.heading("Decoded", text="Decoded")
        
        self.tree.column("ID", width=60)
        self.tree.column("Dir", width=50)
        self.tree.column("Count", width=50)
        self.tree.column("Interval", width=80)
        self.tree.column("Data", width=200)
        self.tree.column("Decoded", width=200)
        
        self.tree.pack(fill=tk.BOTH, expand=True)
        
        # Double click to edit decoder
        self.tree.bind("<Double-1>", self.on_double_click)

    def toggle_record(self):
        if not self.recorder.recording:
            self.recorder.start()
            self.btn_record.config(text="Stop & Save")
        else:
            self.recorder.stop()
            self.btn_record.config(text="Record")
            filename = filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV Files", "*.csv")])
            if filename:
                self.recorder.save_to_file(filename)

    def toggle_pause(self):
        self.paused = not self.paused
        self.btn_pause.config(text="Resume View" if self.paused else "Pause View")

    def clear_table(self):
        for item in self.tree.get_children():
            self.tree.delete(item)
        self.messages_map.clear()

    def on_message(self, timestamp, direction, can_id, data):
        # Record
        self.recorder.add_message(timestamp, direction, can_id, data)
        
        if self.paused:
            return

        hex_id = hex(can_id)
        data_str = data.hex()
        
        # Decode
        decoded_str = ""
        signals = self.profile_manager.get_signals(can_id)
        if signals:
            parts = []
            for sig in signals:
                val = Decoder.decode(data, sig["start_byte"]*8, sig["length"]*8, sig["type"], sig["scale"])
                parts.append(f"{sig['name']}: {val}")
            decoded_str = ", ".join(parts)

        # Update Table (Grouping Logic)
        if hex_id in self.messages_map:
            item_id = self.messages_map[hex_id]
            current_vals = self.tree.item(item_id, "values")
            count = int(current_vals[2]) + 1
            # Interval calc could be improved by storing last timestamp
            
            self.tree.item(item_id, values=(hex_id, direction, count, "-", data_str, decoded_str))
        else:
            item_id = self.tree.insert("", tk.END, values=(hex_id, direction, 1, "-", data_str, decoded_str))
            self.messages_map[hex_id] = item_id

    def on_double_click(self, event):
        item_id = self.tree.identify_row(event.y)
        if not item_id: return
        
        vals = self.tree.item(item_id, "values")
        hex_id = vals[0]
        can_id = int(hex_id, 16)
        
        current_signals = self.profile_manager.get_signals(can_id)
        DecoderEditorDialog(self, can_id, current_signals, self.profile_manager.set_signals)
