import tkinter as tk
from tkinter import ttk, filedialog, simpledialog, messagebox
import time
import csv
from logic.recorder import Recorder
from gui.decoder_editor import DecoderEditorDialog
from logic.decoder import Decoder

class ReverseEngineeringPanel(ttk.Frame):
    
    # OBD2 Presets (PID -> {name, formula, length, reverse_formula})
    OBD2_PRESETS = {
        0x04: {"name": "Engine Load", "formula": "x[0] * 100.0 / 255.0", "length": 1, "reverse_formula": "int(v * 255.0 / 100.0)"},
        0x05: {"name": "Coolant Temp", "formula": "x[0] - 40", "length": 1, "reverse_formula": "int(v + 40)"},
        0x0A: {"name": "Fuel Pressure", "formula": "x[0] * 3", "length": 1, "reverse_formula": "int(v / 3.0)"},
        0x0B: {"name": "Intake MAP", "formula": "x[0]", "length": 1, "reverse_formula": "int(v)"},
        0x0C: {"name": "Engine RPM", "formula": "(x[0]*256 + x[1])/4.0", "length": 2, "reverse_formula": "int(v * 4.0)"},
        0x0D: {"name": "Vehicle Speed", "formula": "x[0]", "length": 1, "reverse_formula": "int(v)"},
        0x0E: {"name": "Timing Advance", "formula": "x[0]/2.0 - 64.0", "length": 1, "reverse_formula": "int((v + 64.0) * 2.0)"},
        0x10: {"name": "MAF Air Flow", "formula": "(x[0]*256 + x[1])/100.0", "length": 2, "reverse_formula": "int(v * 100.0)"},
        0x11: {"name": "Throttle Position", "formula": "x[0] * 100.0 / 255.0", "length": 1, "reverse_formula": "int(v * 255.0 / 100.0)"},
        0x1F: {"name": "Run Time", "formula": "f'{x[0]*256+x[1]//3600:02}:{(x[0]*256+x[1]%3600)//60:02}:{x[0]*256+x[1]%60:02}'", "length": 2, "reverse_formula": "int(v)"},
        0x2F: {"name": "Fuel Level", "formula": "x[0] * 100.0 / 255.0", "length": 1, "reverse_formula": "int(v * 255.0 / 100.0)"},
    }

    # Common Internal CAN Presets (Name -> {formula, length, reverse_formula})
    INTERNAL_PRESETS = {
        "Engine RPM (2 Bytes, Div 4)": {"name": "Engine RPM", "formula": "(x[0]*256 + x[1])/4.0", "length": 2, "reverse_formula": "int(v * 4.0)"},
        "Vehicle Speed (1 Byte)": {"name": "Vehicle Speed", "formula": "x[0]", "length": 1, "reverse_formula": "int(v)"},
        "Percent (1 Byte)": {"name": "Percent Value", "formula": "x[0] * 100.0 / 255.0", "length": 1, "reverse_formula": "int(v * 255.0 / 100.0)"},
        "Steering Angle (2 Bytes, Div 10)": {"name": "Steering Angle", "formula": "int.from_bytes(bytes([x[0], x[1]]), byteorder='big', signed=True) / 10.0", "length": 2, "reverse_formula": "int(v * 10.0)"}
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
        
        # Replaced Record button with Save Record
        self.btn_save_record = ttk.Button(toolbar, text="Save Record", command=self.save_recording, state=tk.DISABLED)
        self.btn_save_record.pack(side=tk.LEFT)
        
        # Load Recording Button
        self.btn_load_record = ttk.Button(toolbar, text="Load Recording", command=self.load_recording)
        self.btn_load_record.pack(side=tk.LEFT, padx=5)
        
        self.btn_clear = ttk.Button(toolbar, text="Clear", command=self.clear_table)
        self.btn_clear.pack(side=tk.LEFT, padx=5)
        
        # Load & Save Profile Buttons
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
        if active:
            self.btn_save_record.config(state=tk.DISABLED)
            self.btn_load_record.config(state=tk.DISABLED)
            self.btn_clear.config(state=tk.DISABLED)
        else:
            self.btn_load_record.config(state=tk.NORMAL)
            self.btn_clear.config(state=tk.NORMAL)
            if len(self.recorder.get_messages()) > 0:
                self.btn_save_record.config(state=tk.NORMAL)
            else:
                self.btn_save_record.config(state=tk.DISABLED)

    def save_recording(self):
        if self.logging_active:
            messagebox.showwarning("Warning", "Please stop logging before saving.")
            return
            
        messages = self.recorder.get_messages()
        if not messages:
            messagebox.showinfo("Info", "No data to save.")
            return
            
        filename = filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV Files", "*.csv")])
        if filename:
            if self.recorder.save_to_file(filename):
                messagebox.showinfo("Success", f"Successfully saved {len(messages)} messages.")
            else:
                messagebox.showerror("Error", "Failed to save recording.")

    def load_recording(self):
        filename = filedialog.askopenfilename(filetypes=[("CSV Files", "*.csv")])
        if not filename:
            return
            
        if self.logging_active:
            messagebox.showwarning("Warning", "Please stop logging before loading a recording.")
            return
            
        # Confirm clear
        if self.display_buffer:
            if not messagebox.askyesno("Confirm", "Loading a recording will clear the current display. Continue?"):
                return
                
        self.clear_table()
        
        try:
            with open(filename, 'r', newline='') as f:
                reader = csv.DictReader(f)
                count = 0
                for row in reader:
                    # row format: timestamp, direction, can_id, data
                    timestamp = float(row['timestamp'])
                    direction = row['direction']
                    can_id = int(row['can_id'], 16)
                    # Convert hex string data back to bytes
                    data = bytes.fromhex(row['data'])
                    
                    # Add to local buffer and update UI
                    self.display_buffer.append({
                        'timestamp': timestamp,
                        'direction': direction,
                        'can_id': can_id,
                        'data': data
                    })
                    # Add to recorder so it can be saved again if needed
                    self.recorder.add_message(timestamp, direction, can_id, data)
                    self._update_ui_row(timestamp, direction, can_id, data)
                    count += 1
                    
            print(f"Loaded {count} messages from {filename}")
            
            # Enable save button
            if count > 0:
                self.btn_save_record.config(state=tk.NORMAL)
                
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load recording:\n{e}")

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

        # --- Internal CAN Presets ---
        internal_menu = tk.Menu(menu, tearoff=0)
        for key, info in self.INTERNAL_PRESETS.items():
            if data_len >= info['length']:
                label_text = f"{info['formula']} (used for example for {info['name']})"
                internal_menu.add_command(label=label_text, command=lambda n=info['name'], f=info['formula'], l=info['length']: self.apply_preset("formula", 0, l, f, name=n))
        menu.add_cascade(label="Common Presets", menu=internal_menu)

        # --- OBD-II Presets ---
        obd_menu = tk.Menu(menu, tearoff=0)
        
        def add_obd(label_text, formula, req_len, name):
            if data_len >= req_len:
                obd_menu.add_command(label=label_text, command=lambda: self.apply_preset("formula", 0, req_len, formula, name=name))

        # Use the class constant for presets
        for pid, info in sorted(self.OBD2_PRESETS.items()):
             label = f"{info['formula']} (used for example for {info['name']})"
             add_obd(label, info['formula'], info['length'], info['name'])

        menu.add_cascade(label="OBD-II Presets", menu=obd_menu)
        
        # --- Generic Types (Grouped) ---
        generic_menu = tk.Menu(menu, tearoff=0)
        
        # Uint8
        u8_menu = tk.Menu(generic_menu, tearoff=0)
        for i in range(data_len):
            u8_menu.add_command(label=f"Byte {i}", command=lambda idx=i: self.apply_preset("uint8", idx, 1))
        generic_menu.add_cascade(label="Uint8", menu=u8_menu)

        # Uint16 BE
        if data_len >= 2:
            u16_menu = tk.Menu(generic_menu, tearoff=0)
            for i in range(data_len - 1):
                u16_menu.add_command(label=f"Bytes {i}-{i+1}", command=lambda idx=i: self.apply_preset("uint16_be", idx, 2))
            generic_menu.add_cascade(label="Uint16 (BE)", menu=u16_menu)

        # Percent
        pct_menu = tk.Menu(generic_menu, tearoff=0)
        for i in range(data_len):
            pct_menu.add_command(label=f"Byte {i}", command=lambda idx=i: self.apply_preset("percent", idx, 1))
        generic_menu.add_cascade(label="Percent", menu=pct_menu)

        generic_menu.add_separator()
        generic_menu.add_command(label="ASCII String", command=lambda: self.apply_preset("ascii", 0, data_len))
        
        menu.add_cascade(label="Generic Types", menu=generic_menu)
        
        menu.add_separator()
        menu.add_command(label="Custom Formula...", command=self.ask_formula)
        menu.add_separator()
        menu.add_command(label="Clear Decoder", command=self.clear_decoder)

        # --- MAP TO CONTROLS ---
        menu.add_separator()
        map_menu = tk.Menu(menu, tearoff=0)
        map_menu.add_command(label="Map to Steering", command=lambda: self.map_control("steering"))
        map_menu.add_command(label="Map to Throttle", command=lambda: self.map_control("throttle"))
        map_menu.add_command(label="Map to Brake", command=lambda: self.map_control("brake"))
        menu.add_cascade(label="Map to Control Input...", menu=map_menu)
        
        return menu

    def map_control(self, control_name):
        if self.selected_can_id is None:
            return

        # Check if there are any signals defined for this ID/PID
        signals = self.profile_manager.get_signals(self.selected_can_id, self.selected_pid)
        if not signals:
            messagebox.showwarning("Warning", "Please define a decoder (signal) for this message before mapping it to a control.")
            return

        # Ask for signal index if there are multiple
        sig_index = 0
        if len(signals) > 1:
            sig_index = simpledialog.askinteger("Select Signal", 
                                                f"There are {len(signals)} signals defined.\nEnter index (0-{len(signals)-1}):", 
                                                initialvalue=0, minvalue=0, maxvalue=len(signals)-1)
            if sig_index is None:
                return
                
        sig = signals[sig_index]

        # Ask for raw boundaries for normalization
        min_val = simpledialog.askfloat("Min Value", f"Enter the raw decoded value corresponding to minimal {control_name} input (e.g., full left / no pedal):", initialvalue=0.0)
        if min_val is None: return
        
        max_val = simpledialog.askfloat("Max Value", f"Enter the raw decoded value corresponding to maximal {control_name} input (e.g., full right / full pedal):", initialvalue=100.0)
        if max_val is None: return

        # Try to find reverse formula from presets
        rev_formula = "int(v)"
        form = sig.get("formula", "")
        
        # Check OBD2
        for pid, info in self.OBD2_PRESETS.items():
            if info["formula"] == form:
                rev_formula = info.get("reverse_formula", "int(v)")
                break
                
        # Check Internal
        for name, info in self.INTERNAL_PRESETS.items():
            if info["formula"] == form:
                rev_formula = info.get("reverse_formula", "int(v)")
                break

        rev_formula = simpledialog.askstring("Reverse Formula", "Enter the Reverse Formula (Encode) using 'v' as input:", initialvalue=rev_formula)
        if rev_formula is None: return

        self.profile_manager.set_control_mapping(
            control_name=control_name,
            can_id=self.selected_can_id,
            pid=self.selected_pid,
            signal_index=sig_index,
            min_val=min_val,
            max_val=max_val,
            reverse_formula=rev_formula,
            start_byte=sig.get("start_byte", 0),
            length=sig.get("length", 1),
            sig_type=sig.get("type", "uint8")
        )
        self.profile_manager.save()
        messagebox.showinfo("Success", f"Successfully mapped {hex(self.selected_can_id)} to {control_name}.")

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

    def apply_preset(self, type_name, start_byte, length, formula=None, name="Value"):
        if self.selected_can_id is None: return
        
        signal = {
            "name": name,
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
        
        formula = simpledialog.askstring("Custom Formula", "Enter formula (e.g. x[0]*5 + x[1]):")
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

    def clear_table(self):
        for item in self.tree.get_children():
            self.tree.delete(item)
        self.messages_map.clear()
        self.display_buffer.clear()
        self.recorder.clear()
        self.btn_save_record.config(state=tk.DISABLED)

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
        # It handles the max limit internally
        if not self.recorder.add_message(timestamp, direction, can_id, data):
            # Limit reached, UI handler in main.py will detect this and stop logging
            return
        
        # Enable save button if not logging (though we usually are when this is called)
        # However, to be safe, state updates are handled by set_logging_state
        
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

        # Pass the current pid_index if available
        config = self.profile_manager.get_message_config(can_id)
        pid_index = config.get("pid_index", 0)

        DecoderEditorDialog(self, can_id, current_signals, save_callback, pid=pid, recorder=self.recorder, pid_index=pid_index, profile_manager=self.profile_manager)
