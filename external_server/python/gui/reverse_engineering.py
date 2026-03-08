import tkinter as tk
from tkinter import ttk, filedialog, simpledialog
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
        self._create_context_menu()

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
        
        # Bindings
        self.tree.bind("<Double-1>", self.on_double_click)
        
        # Right-click context menu (Linux/Win: Button-3, Mac: Button-2)
        if self.tk.call('tk', 'windowingsystem') == 'aqua':
            self.tree.bind("<Button-2>", self.show_context_menu)
        else:
            self.tree.bind("<Button-3>", self.show_context_menu)

    def _create_context_menu(self):
        self.context_menu = tk.Menu(self, tearoff=0)
        self.selected_can_id = None
        
        # Integer Submenu
        int_menu = tk.Menu(self.context_menu, tearoff=0)
        int_menu.add_command(label="Uint8 (Byte 0)", command=lambda: self.apply_preset("uint8", 0, 1))
        int_menu.add_command(label="Int8 (Byte 0)", command=lambda: self.apply_preset("int8", 0, 1))
        int_menu.add_separator()
        int_menu.add_command(label="Uint16 BE (Bytes 0-1)", command=lambda: self.apply_preset("uint16_be", 0, 2))
        int_menu.add_command(label="Uint16 LE (Bytes 0-1)", command=lambda: self.apply_preset("uint16_le", 0, 2))
        int_menu.add_command(label="Int16 BE (Bytes 0-1)", command=lambda: self.apply_preset("int16_be", 0, 2))
        int_menu.add_command(label="Int16 LE (Bytes 0-1)", command=lambda: self.apply_preset("int16_le", 0, 2))
        
        self.context_menu.add_cascade(label="Integer", menu=int_menu)
        
        # Float Submenu
        float_menu = tk.Menu(self.context_menu, tearoff=0)
        float_menu.add_command(label="Float (Bytes 0-3)", command=lambda: self.apply_preset("float", 0, 4))
        self.context_menu.add_cascade(label="Float", menu=float_menu)
        
        # Other Types
        self.context_menu.add_command(label="Percent (Byte 0)", command=lambda: self.apply_preset("percent", 0, 1))
        self.context_menu.add_command(label="ASCII String", command=lambda: self.apply_preset("ascii", 0, 8))
        
        self.context_menu.add_separator()
        self.context_menu.add_command(label="Custom Formula...", command=self.ask_formula)
        self.context_menu.add_separator()
        self.context_menu.add_command(label="Clear Decoder", command=self.clear_decoder)

    def show_context_menu(self, event):
        if not self.paused:
            return # Only allow editing when paused to avoid UI jumps

        item_id = self.tree.identify_row(event.y)
        if item_id:
            self.tree.selection_set(item_id)
            vals = self.tree.item(item_id, "values")
            self.selected_can_id = int(vals[0], 16)
            self.context_menu.post(event.x_root, event.y_root)

    def apply_preset(self, type_name, start_byte, length, formula=None):
        if self.selected_can_id is None: return
        
        # Create a single signal definition
        signal = {
            "name": "Value",
            "start_byte": start_byte,
            "length": length,
            "type": type_name,
            "scale": 1.0,
            "offset": 0.0,
            "formula": formula
        }
        
        self.profile_manager.set_signals(self.selected_can_id, [signal])
        self.refresh_row(self.selected_can_id)

    def ask_formula(self):
        if self.selected_can_id is None: return
        
        formula = simpledialog.askstring("Custom Formula", "Enter formula (e.g. d[0]*5 + d[1]):")
        if formula:
            self.apply_preset("formula", 0, 0, formula)

    def clear_decoder(self):
        if self.selected_can_id is None: return
        self.profile_manager.set_signals(self.selected_can_id, [])
        self.refresh_row(self.selected_can_id)

    def refresh_row(self, can_id):
        # Find item in tree
        hex_id = hex(can_id)
        if hex_id in self.messages_map:
            item_id = self.messages_map[hex_id]
            current_vals = self.tree.item(item_id, "values")
            data_str = current_vals[4]
            try:
                data = bytes.fromhex(data_str)
                
                # Re-decode
                decoded_str = ""
                signals = self.profile_manager.get_signals(can_id)
                if signals:
                    parts = []
                    for sig in signals:
                        # Pass formula if exists
                        formula = sig.get("formula")
                        val = Decoder.decode(data, sig["start_byte"]*8, sig["length"]*8, sig["type"], 
                                           sig["scale"], sig["offset"], formula)
                        parts.append(f"{sig['name']}: {val}")
                    decoded_str = ", ".join(parts)
                
                # Update tree
                new_vals = list(current_vals)
                new_vals[5] = decoded_str
                self.tree.item(item_id, values=new_vals)
                
            except Exception as e:
                print(f"Error refreshing row: {e}")

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
                formula = sig.get("formula")
                val = Decoder.decode(data, sig["start_byte"]*8, sig["length"]*8, sig["type"], 
                                   sig["scale"], sig["offset"], formula)
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
