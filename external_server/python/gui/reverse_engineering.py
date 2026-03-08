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
        self.logging_active = False # Controlled by MainApp
        
        self._setup_ui()
        # Context menu is created dynamically now

    def _setup_ui(self):
        # Toolbar
        toolbar = ttk.Frame(self)
        toolbar.pack(fill=tk.X, padx=5, pady=5)
        
        self.btn_record = ttk.Button(toolbar, text="Record", command=self.toggle_record)
        self.btn_record.pack(side=tk.LEFT)
        
        # Removed Pause button
        
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

    def set_logging_state(self, active):
        self.logging_active = active

    def _create_dynamic_menu(self, data_len):
        menu = tk.Menu(self, tearoff=0)
        
        # --- OBD-II Presets ---
        obd_menu = tk.Menu(menu, tearoff=0)
        
        # Helper to add preset if length matches
        def add_obd(name, formula, req_len):
            if data_len >= req_len:
                obd_menu.add_command(label=name, command=lambda: self.apply_preset("formula", 0, req_len, formula))

        add_obd("Engine RPM", "(d[0]*256 + d[1])/4.0", 2)
        add_obd("Vehicle Speed", "d[0]", 1)
        add_obd("Coolant Temp", "d[0] - 40", 1)
        add_obd("Throttle Position", "d[0] * 100.0 / 255.0", 1)
        add_obd("Engine Load", "d[0] * 100.0 / 255.0", 1)
        add_obd("Fuel Level", "d[0] * 100.0 / 255.0", 1)
        add_obd("MAF Air Flow", "(d[0]*256 + d[1])/100.0", 2)
        add_obd("Timing Advance", "d[0]/2.0 - 64.0", 1)
        add_obd("Intake MAP", "d[0]", 1)
        add_obd("Fuel Pressure", "d[0] * 3", 1)
        add_obd("Run Time", "f'{d[0]*256+d[1]//3600:02}:{(d[0]*256+d[1]%3600)//60:02}:{d[0]*256+d[1]%60:02}'", 2) # Complex formula

        menu.add_cascade(label="OBD-II Presets", menu=obd_menu)
        menu.add_separator()

        # --- Generic Types ---
        
        # Uint8
        u8_menu = tk.Menu(menu, tearoff=0)
        for i in range(data_len):
            u8_menu.add_command(label=f"Byte {i}", command=lambda x=i: self.apply_preset("uint8", x, 1))
        menu.add_cascade(label="Uint8", menu=u8_menu)

        # Uint16 BE
        if data_len >= 2:
            u16_menu = tk.Menu(menu, tearoff=0)
            for i in range(data_len - 1):
                u16_menu.add_command(label=f"Bytes {i}-{i+1}", command=lambda x=i: self.apply_preset("uint16_be", x, 2))
            menu.add_cascade(label="Uint16 (BE)", menu=u16_menu)

        # Percent
        pct_menu = tk.Menu(menu, tearoff=0)
        for i in range(data_len):
            pct_menu.add_command(label=f"Byte {i}", command=lambda x=i: self.apply_preset("percent", x, 1))
        menu.add_cascade(label="Percent", menu=pct_menu)

        menu.add_separator()
        menu.add_command(label="ASCII String", command=lambda: self.apply_preset("ascii", 0, data_len))
        
        menu.add_separator()
        menu.add_command(label="Custom Formula...", command=self.ask_formula)
        menu.add_separator()
        menu.add_command(label="Clear Decoder", command=self.clear_decoder)
        
        return menu

    def show_context_menu(self, event):
        if self.logging_active:
            return # Only allow editing when logging is stopped

        item_id = self.tree.identify_row(event.y)
        if item_id:
            self.tree.selection_set(item_id)
            vals = self.tree.item(item_id, "values")
            self.selected_can_id = int(vals[0], 16)
            
            # Get data length for dynamic menu
            data_str = vals[4]
            try:
                data_bytes = bytes.fromhex(data_str)
                menu = self._create_dynamic_menu(len(data_bytes))
                menu.tk_popup(event.x_root, event.y_root) # Changed from post to tk_popup
            except:
                pass

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

    def clear_table(self):
        for item in self.tree.get_children():
            self.tree.delete(item)
        self.messages_map.clear()

    def on_message(self, timestamp, direction, can_id, data):
        # Record
        self.recorder.add_message(timestamp, direction, can_id, data)
        
        # No need to check paused/logging_active here, as main.py controls the flow
        # But if we want to be safe:
        # if not self.logging_active: return

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
