import tkinter as tk
from tkinter import ttk, filedialog, simpledialog
import time
from logic.recorder import Recorder
from gui.decoder_editor import DecoderEditorDialog
from logic.decoder import Decoder

class ReverseEngineeringPanel(ttk.Frame):
    
    # OBD2 Presets (PID -> {name, formula, length})
    OBD2_PRESETS = {
        0x04: {"name": "Engine Load", "formula": "d[0] * 100.0 / 255.0", "length": 1},
        0x05: {"name": "Coolant Temp", "formula": "d[0] - 40", "length": 1},
        0x0A: {"name": "Fuel Pressure", "formula": "d[0] * 3", "length": 1},
        0x0B: {"name": "Intake MAP", "formula": "d[0]", "length": 1},
        0x0C: {"name": "Engine RPM", "formula": "(d[0]*256 + d[1])/4.0", "length": 2},
        0x0D: {"name": "Vehicle Speed", "formula": "d[0]", "length": 1},
        0x0E: {"name": "Timing Advance", "formula": "d[0]/2.0 - 64.0", "length": 1},
        0x10: {"name": "MAF Air Flow", "formula": "(d[0]*256 + d[1])/100.0", "length": 2},
        0x11: {"name": "Throttle Position", "formula": "d[0] * 100.0 / 255.0", "length": 1},
        0x1F: {"name": "Run Time", "formula": "f'{d[0]*256+d[1]//3600:02}:{(d[0]*256+d[1]%3600)//60:02}:{d[0]*256+d[1]%60:02}'", "length": 2},
        0x2F: {"name": "Fuel Level", "formula": "d[0] * 100.0 / 255.0", "length": 1},
    }

    def __init__(self, parent, profile_manager):
        super().__init__(parent)
        self.profile_manager = profile_manager
        self.recorder = Recorder()
        self.messages_map = {} # Key: (hex_id, pid) -> Item ID in tree
        self.display_buffer = [] # Buffer for re-grouping logic
        self.logging_active = False # Controlled by MainApp
        self.selected_can_id = None
        self.selected_pid = None
        
        self._setup_ui()

    def _setup_ui(self):
        # Toolbar
        toolbar = ttk.Frame(self)
        toolbar.pack(fill=tk.X, padx=5, pady=5)
        
        self.btn_record = ttk.Button(toolbar, text="Record", command=self.toggle_record)
        self.btn_record.pack(side=tk.LEFT)
        
        ttk.Button(toolbar, text="Clear", command=self.clear_table).pack(side=tk.LEFT, padx=5)
        
        # Load & Save Buttons
        ttk.Button(toolbar, text="Load Profile", command=self.load_profile).pack(side=tk.LEFT, padx=5)
        ttk.Button(toolbar, text="Save Profile", command=self.save_profile).pack(side=tk.LEFT, padx=5)

        # Table
        columns = ("ID", "Protocol", "PID", "Dir", "Count", "Interval", "Data", "Decoded")
        self.tree = ttk.Treeview(self, columns=columns, show="headings")
        
        self.tree.heading("ID", text="ID")
        self.tree.heading("Protocol", text="Protocol")
        self.tree.heading("PID", text="PID")
        self.tree.heading("Dir", text="Dir")
        self.tree.heading("Count", text="Count")
        self.tree.heading("Interval", text="Interval (ms)")
        self.tree.heading("Data", text="Data")
        self.tree.heading("Decoded", text="Decoded")
        
        self.tree.column("ID", width=60)
        self.tree.column("Protocol", width=80)
        self.tree.column("PID", width=50)
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

    def load_profile(self):
        filename = filedialog.askopenfilename(filetypes=[("JSON Files", "*.json")])
        if filename:
            if self.profile_manager.load_from_file(filename):
                self.refresh_table()

    def save_profile(self):
        filename = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON Files", "*.json")])
        if filename:
            # Update filename in manager and save
            self.profile_manager.filename = filename
            self.profile_manager.save()

    def _create_dynamic_menu(self, can_id, data_len):
        menu = tk.Menu(self, tearoff=0)
        
        # --- Protocol Configuration ---
        proto_menu = tk.Menu(menu, tearoff=0)
        
        # Get current config
        config = self.profile_manager.get_message_config(can_id)
        current_proto = config.get("protocol", "Manual")
        has_pid = config.get("has_pid", False)
        pid_index = config.get("pid_index", 0)

        # Protocol Selection
        protocols = ["Manual", "OBD2", "UDS", "ISO-TP", "J1939"]
        for p in protocols:
            proto_menu.add_radiobutton(label=p, variable=tk.StringVar(value=current_proto), value=p,
                                     command=lambda x=p: self.set_protocol(can_id, x))
        
        proto_menu.add_separator()
        
        # PID Configuration
        proto_menu.add_checkbutton(label="Has PID", variable=tk.BooleanVar(value=has_pid),
                                 command=lambda: self.toggle_has_pid(can_id))
        
        if has_pid:
            proto_menu.add_command(label=f"Set PID Index (Current: {pid_index})", 
                                 command=lambda: self.ask_pid_index(can_id))

        menu.add_cascade(label="Protocol Settings", menu=proto_menu)
        menu.add_separator()

        # --- OBD-II Presets ---
        obd_menu = tk.Menu(menu, tearoff=0)
        
        def add_obd(name, formula, req_len):
            if data_len >= req_len:
                obd_menu.add_command(label=name, command=lambda: self.apply_preset("formula", 0, req_len, formula))

        # Use the class constant for presets
        for pid, info in sorted(self.OBD2_PRESETS.items()):
             add_obd(f"{info['name']} ({hex(pid)})", info['formula'], info['length'])

        menu.add_cascade(label="OBD-II Presets", menu=obd_menu)
        
        # --- Generic Types (Grouped) ---
        generic_menu = tk.Menu(menu, tearoff=0)
        
        # Uint8
        u8_menu = tk.Menu(generic_menu, tearoff=0)
        for i in range(data_len):
            u8_menu.add_command(label=f"Byte {i}", command=lambda x=i: self.apply_preset("uint8", x, 1))
        generic_menu.add_cascade(label="Uint8", menu=u8_menu)

        # Uint16 BE
        if data_len >= 2:
            u16_menu = tk.Menu(generic_menu, tearoff=0)
            for i in range(data_len - 1):
                u16_menu.add_command(label=f"Bytes {i}-{i+1}", command=lambda x=i: self.apply_preset("uint16_be", x, 2))
            generic_menu.add_cascade(label="Uint16 (BE)", menu=u16_menu)

        # Percent
        pct_menu = tk.Menu(generic_menu, tearoff=0)
        for i in range(data_len):
            pct_menu.add_command(label=f"Byte {i}", command=lambda x=i: self.apply_preset("percent", x, 1))
        generic_menu.add_cascade(label="Percent", menu=pct_menu)

        generic_menu.add_separator()
        generic_menu.add_command(label="ASCII String", command=lambda: self.apply_preset("ascii", 0, data_len))
        
        menu.add_cascade(label="Generic Types", menu=generic_menu)
        
        menu.add_separator()
        menu.add_command(label="Custom Formula...", command=self.ask_formula)
        menu.add_separator()
        menu.add_command(label="Clear Decoder", command=self.clear_decoder)
        
        return menu

    def show_context_menu(self, event):
        if self.logging_active:
            return 

        item_id = self.tree.identify_row(event.y)
        if item_id:
            self.tree.selection_set(item_id)
            vals = self.tree.item(item_id, "values")
            self.selected_can_id = int(vals[0], 16)
            
            # Extract PID if present
            pid_str = vals[2]
            self.selected_pid = int(pid_str, 16) if pid_str != "-" else None
            
            data_str = vals[6]
            try:
                data_bytes = bytes.fromhex(data_str)
                menu = self._create_dynamic_menu(self.selected_can_id, len(data_bytes))
                menu.tk_popup(event.x_root, event.y_root)
            except:
                pass

    # --- Protocol Configuration Methods ---

    def set_protocol(self, can_id, protocol):
        config = self.profile_manager.get_message_config(can_id)
        config["protocol"] = protocol
        
        # Apply protocol defaults for PID
        defaults = self.profile_manager.get_protocol_defaults(protocol)
        config.update(defaults)
        
        self.profile_manager.set_message_config(can_id, config)
        self.refresh_table() # Re-render table to reflect changes

    def toggle_has_pid(self, can_id):
        config = self.profile_manager.get_message_config(can_id)
        config["has_pid"] = not config.get("has_pid", False)
        self.profile_manager.set_message_config(can_id, config)
        self.refresh_table()

    def ask_pid_index(self, can_id):
        config = self.profile_manager.get_message_config(can_id)
        current_idx = config.get("pid_index", 0)
        new_idx = simpledialog.askinteger("PID Index", "Enter byte index for PID (0-7):", 
                                        initialvalue=current_idx, minvalue=0, maxvalue=7)
        if new_idx is not None:
            config["pid_index"] = new_idx
            self.profile_manager.set_message_config(can_id, config)
            self.refresh_table()

    # --- Decoder Methods ---

    def apply_preset(self, type_name, start_byte, length, formula=None):
        if self.selected_can_id is None: return
        
        signal = {
            "name": "Value",
            "start_byte": start_byte,
            "length": length,
            "type": type_name,
            "scale": 1.0,
            "offset": 0.0,
            "formula": formula
        }
        
        # Use PID if available
        self.profile_manager.set_signals(self.selected_can_id, [signal], self.selected_pid)
        self.refresh_row(self.selected_can_id, self.selected_pid)

    def ask_formula(self):
        if self.selected_can_id is None: return
        
        formula = simpledialog.askstring("Custom Formula", "Enter formula (e.g. d[0]*5 + d[1]):")
        if formula:
            self.apply_preset("formula", 0, 0, formula)

    def clear_decoder(self):
        if self.selected_can_id is None: return
        self.profile_manager.set_signals(self.selected_can_id, [], self.selected_pid)
        self.refresh_row(self.selected_can_id, self.selected_pid)

    def refresh_row(self, can_id, pid=None):
        hex_id = hex(can_id)
        key = (hex_id, pid)
        
        if key in self.messages_map:
            item_id = self.messages_map[key]
            current_vals = self.tree.item(item_id, "values")
            data_str = current_vals[6]
            try:
                data = bytes.fromhex(data_str)
                decoded_str = self._decode_data(can_id, data, pid)
                
                new_vals = list(current_vals)
                new_vals[7] = decoded_str
                self.tree.item(item_id, values=new_vals)
            except Exception as e:
                print(f"Error refreshing row: {e}")

    def _decode_data(self, can_id, data, pid=None):
        decoded_str = ""
        signals = self.profile_manager.get_signals(can_id, pid)
        if signals:
            parts = []
            for sig in signals:
                formula = sig.get("formula")
                val = Decoder.decode(data, sig["start_byte"]*8, sig["length"]*8, sig["type"], 
                                   sig.get("scale", 1.0), sig.get("offset", 0.0), formula)
                parts.append(f"{sig['name']}: {val}")
            decoded_str = ", ".join(parts)
        return decoded_str

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
        self.display_buffer.clear() 

    def refresh_table(self):
        # Clear UI only
        for item in self.tree.get_children():
            self.tree.delete(item)
        self.messages_map.clear()
        
        # Re-populate UI from buffer
        for msg in self.display_buffer:
            self._update_ui_row(msg['timestamp'], msg['direction'], msg['can_id'], msg['data'])

    def on_message(self, timestamp, direction, can_id, data):
        # 1. Add to recorder (for file saving)
        self.recorder.add_message(timestamp, direction, can_id, data)
        
        # 2. Add to local buffer (for re-grouping)
        self.display_buffer.append({
            'timestamp': timestamp,
            'direction': direction,
            'can_id': can_id,
            'data': data
        })
        
        # 3. Update UI
        self._update_ui_row(timestamp, direction, can_id, data)

    def _update_ui_row(self, timestamp, direction, can_id, data):
        hex_id = hex(can_id)
        data_str = data.hex()
        
        # Get Config (now with auto-detection for new IDs)
        config = self.profile_manager.get_message_config(can_id)
        protocol = config.get("protocol", "Manual")
        has_pid = config.get("has_pid", False)
        pid_index = config.get("pid_index", 0)
        
        pid_val = None
        pid_str = "-"
        
        if has_pid and len(data) > pid_index:
            pid_val = data[pid_index]
            pid_str = hex(pid_val)

        # --- Auto-Apply Decoder for OBD2 ---
        if protocol == "OBD2" and pid_val is not None:
            existing_signals = self.profile_manager.get_signals(can_id, pid_val)
            if not existing_signals and pid_val in self.OBD2_PRESETS:
                preset = self.OBD2_PRESETS[pid_val]
                # Standard OBD2 response: [Len, Mode, PID, A, B, C, D]
                # Data starts after PID. If pid_index is 2, data starts at 3.
                start_byte = pid_index + 1
                
                signal = {
                    "name": preset["name"],
                    "start_byte": start_byte,
                    "length": preset["length"],
                    "type": "formula",
                    "scale": 1.0,
                    "offset": 0.0,
                    "formula": preset["formula"]
                }
                self.profile_manager.set_signals(can_id, [signal], pid_val)

        # Decode
        decoded_str = self._decode_data(can_id, data, pid_val)

        # Grouping Key
        key = (hex_id, pid_val)

        if key in self.messages_map:
            item_id = self.messages_map[key]
            current_vals = self.tree.item(item_id, "values")
            count = int(current_vals[4]) + 1
            # Update row
            self.tree.item(item_id, values=(hex_id, protocol, pid_str, direction, count, "-", data_str, decoded_str))
        else:
            # Insert sorted
            self._insert_sorted(hex_id, protocol, pid_str, direction, data_str, decoded_str, key)

    def _insert_sorted(self, hex_id, protocol, pid_str, direction, data_str, decoded_str, key):
        # Convert hex_id to int for comparison
        new_id_val = int(hex_id, 16)
        new_pid_val = int(pid_str, 16) if pid_str != "-" else -1
        
        # Find insertion point
        children = self.tree.get_children()
        insert_idx = tk.END # Default to end
        
        for i, child in enumerate(children):
            vals = self.tree.item(child, "values")
            curr_id_val = int(vals[0], 16)
            
            if curr_id_val > new_id_val:
                insert_idx = i
                break
            elif curr_id_val == new_id_val:
                # If IDs match, check PID
                curr_pid_str = vals[2]
                curr_pid_val = int(curr_pid_str, 16) if curr_pid_str != "-" else -1
                
                if curr_pid_val > new_pid_val:
                    insert_idx = i
                    break
        
        # Insert at found index
        if insert_idx == tk.END:
            item_id = self.tree.insert("", tk.END, values=(hex_id, protocol, pid_str, direction, 1, "-", data_str, decoded_str))
        else:
            item_id = self.tree.insert("", insert_idx, values=(hex_id, protocol, pid_str, direction, 1, "-", data_str, decoded_str))
            
        self.messages_map[key] = item_id

    def on_double_click(self, event):
        item_id = self.tree.identify_row(event.y)
        if not item_id: return
        
        vals = self.tree.item(item_id, "values")
        hex_id = vals[0]
        can_id = int(hex_id, 16)
        
        pid_str = vals[2]
        pid = int(pid_str, 16) if pid_str != "-" else None
        
        current_signals = self.profile_manager.get_signals(can_id, pid)
        
        def save_callback(cid, sigs):
            self.profile_manager.set_signals(cid, sigs, pid)
            self.refresh_row(cid, pid)

        DecoderEditorDialog(self, can_id, current_signals, save_callback, pid=pid)
