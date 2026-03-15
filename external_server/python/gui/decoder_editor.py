import tkinter as tk
from tkinter import ttk
import time

class DecoderEditorDialog(tk.Toplevel):
    # Presets for quick loading (including internal CAN and OBD2)
    PRESETS = {
        "(x[0]*256 + x[1])/4.0 (used for example for Engine RPM)": {"name": "Engine RPM", "start_byte": 0, "length": 2, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "(x[0]*256 + x[1])/4.0"},
        "x[0] (used for example for Vehicle Speed)": {"name": "Vehicle Speed", "start_byte": 0, "length": 1, "type": "uint8", "scale": 1.0, "offset": 0.0, "formula": "x[0]"},
        "(x[0]*256 + x[1])/4.0 (used for example for OBD2 Engine RPM)": {"name": "Engine RPM", "start_byte": 3, "length": 2, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "(x[0]*256 + x[1])/4.0"},
        "x[0] (used for example for OBD2 Vehicle Speed)": {"name": "Vehicle Speed", "start_byte": 3, "length": 1, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "x[0]"},
        "x[0] - 40 (used for example for OBD2 Coolant Temp)": {"name": "Coolant Temp", "start_byte": 3, "length": 1, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "x[0] - 40"},
        "x[0] * 100.0 / 255.0 (used for example for OBD2 Throttle Pos)": {"name": "Throttle Pos", "start_byte": 3, "length": 1, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "x[0] * 100.0 / 255.0"},
        "x[0] * 100.0 / 255.0 (used for example for OBD2 Engine Load)": {"name": "Engine Load", "start_byte": 3, "length": 1, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "x[0] * 100.0 / 255.0"},
        "x[0] * 100.0 / 255.0 (used for example for OBD2 Fuel Level)": {"name": "Fuel Level", "start_byte": 3, "length": 1, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "x[0] * 100.0 / 255.0"},
        "(x[0]*256 + x[1])/100.0 (used for example for OBD2 MAF Air Flow)": {"name": "MAF Air Flow", "start_byte": 3, "length": 2, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "(x[0]*256 + x[1])/100.0"},
        "x[0]/2.0 - 64.0 (used for example for OBD2 Timing Advance)": {"name": "Timing Advance", "start_byte": 3, "length": 1, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "x[0]/2.0 - 64.0"},
        "x[0] (used for example for OBD2 Intake MAP)": {"name": "Intake MAP", "start_byte": 3, "length": 1, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "x[0]"},
        "x[0] * 3 (used for example for OBD2 Fuel Pressure)": {"name": "Fuel Pressure", "start_byte": 3, "length": 1, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "x[0] * 3"},
        "x[0]*256+x[1] (used for example for OBD2 Run Time)": {"name": "Run Time", "start_byte": 3, "length": 2, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "x[0]*256+x[1]"},
        "x[0] * 100.0 / 255.0 (used for example for Generic 1 Byte Percent)": {"name": "Percent Value", "start_byte": 0, "length": 1, "type": "formula", "scale": 1.0, "offset": 0.0, "formula": "x[0] * 100.0 / 255.0"},
    }

    def __init__(self, parent, can_id, current_signals, on_save_callback, pid=None, recorder=None, pid_index=0):
        super().__init__(parent)
        
        self.can_id = can_id
        self.pid = pid
        self.pid_index = pid_index
        self.signals = current_signals if current_signals else []
        self.on_save = on_save_callback
        self.recorder = recorder
        
        # Extracted messages for this specific ID
        self.history_messages = []
        # Unique PIDs found for this ID
        self.found_pids = set()
        
        if self.recorder:
            self._extract_history()

        title_str = f"Edit Decoder for {hex(can_id)}"
        if pid is not None:
            title_str += f" (PID: {hex(pid)})"
        self.title(title_str)
        
        # Make the window much larger to act as a workspace
        self.geometry("1000x700")
        
        self._setup_ui()
        self._refresh_signal_list()
        self._refresh_history_preview()

    def _extract_history(self):
        """Extract all messages from recorder that match the current ID"""
        all_msgs = self.recorder.get_messages()
        hex_id = hex(self.can_id)
        
        for msg in all_msgs:
            if msg['can_id'] == hex_id:
                data = bytes.fromhex(msg['data'])
                self.history_messages.append(msg)
                
                if self.pid is not None and len(data) > self.pid_index:
                    self.found_pids.add(data[self.pid_index])

    def _setup_ui(self):
        # Create PanedWindow to separate Control (Top) and Live View (Bottom)
        paned = ttk.PanedWindow(self, orient=tk.VERTICAL)
        paned.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # --- Top Pane: Control & Editing ---
        top_pane = ttk.Frame(paned)
        paned.add(top_pane, weight=0) # Don't expand vertically
        
        # Top Controls Frame (PID Selector & Presets)
        controls_frame = ttk.Frame(top_pane)
        controls_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # PID Selector (only if has_pid)
        if self.pid is not None:
            ttk.Label(controls_frame, text="Filter by PID:").pack(side=tk.LEFT)
            self.pid_filter_var = tk.StringVar(value=hex(self.pid))
            
            pid_values = ["All"] + [hex(p) for p in sorted(list(self.found_pids))]
            if hex(self.pid) not in pid_values:
                pid_values.append(hex(self.pid))
                
            pid_cb = ttk.Combobox(controls_frame, textvariable=self.pid_filter_var, values=pid_values, width=10, state="readonly")
            pid_cb.pack(side=tk.LEFT, padx=5)
            pid_cb.bind("<<ComboboxSelected>>", lambda e: self._refresh_history_preview())
            
            ttk.Separator(controls_frame, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=10)

        # Presets Loader
        ttk.Label(controls_frame, text="Load Template:").pack(side=tk.LEFT)
        self.preset_var = tk.StringVar()
        preset_cb = ttk.Combobox(controls_frame, textvariable=self.preset_var, values=list(self.PRESETS.keys()), width=70)
        preset_cb.pack(side=tk.LEFT, padx=5)
        ttk.Button(controls_frame, text="Load", command=self._load_preset).pack(side=tk.LEFT)

        # Signal List
        self.tree_signals = ttk.Treeview(top_pane, columns=("Name", "Start", "Len", "Type", "Scale", "Formula"), show="headings", height=4)
        self.tree_signals.heading("Name", text="Name")
        self.tree_signals.heading("Start", text="Start Byte")
        self.tree_signals.heading("Len", text="Length")
        self.tree_signals.heading("Type", text="Type")
        self.tree_signals.heading("Scale", text="Scale")
        self.tree_signals.heading("Formula", text="Formula")
        
        self.tree_signals.column("Name", width=120)
        self.tree_signals.column("Start", width=60)
        self.tree_signals.column("Len", width=50)
        self.tree_signals.column("Type", width=70)
        self.tree_signals.column("Scale", width=50)
        self.tree_signals.column("Formula", width=200)

        self.tree_signals.pack(fill=tk.X, padx=5, pady=5)
        self.tree_signals.bind("<<TreeviewSelect>>", self._on_select)

        # Editor Frame
        edit_frame = ttk.LabelFrame(top_pane, text="Signal Editor", padding="5")
        edit_frame.pack(fill=tk.X, padx=5, pady=5)

        # Editor Fields with bindings for live update
        def on_edit(*args):
            self.after(200, self._preview_current_edit)

        ttk.Label(edit_frame, text="Name:").grid(row=0, column=0, sticky="e")
        self.name_var = tk.StringVar()
        self.name_var.trace_add("write", on_edit)
        ttk.Entry(edit_frame, textvariable=self.name_var).grid(row=0, column=1, padx=5, pady=2, sticky="w")

        ttk.Label(edit_frame, text="Start Byte:").grid(row=0, column=2, sticky="e")
        self.start_var = tk.StringVar(value="0") # Use StringVar for continuous editing
        self.start_var.trace_add("write", on_edit)
        ttk.Entry(edit_frame, textvariable=self.start_var, width=5).grid(row=0, column=3, padx=5, pady=2, sticky="w")

        ttk.Label(edit_frame, text="Length:").grid(row=0, column=4, sticky="e")
        self.len_var = tk.StringVar(value="1")
        self.len_var.trace_add("write", on_edit)
        ttk.Entry(edit_frame, textvariable=self.len_var, width=5).grid(row=0, column=5, padx=5, pady=2, sticky="w")

        ttk.Label(edit_frame, text="Type:").grid(row=1, column=0, sticky="e")
        self.type_var = tk.StringVar(value="uint8")
        type_cb = ttk.Combobox(edit_frame, textvariable=self.type_var, values=["uint8", "uint16_be", "uint16_le", "percent", "ascii", "formula"])
        type_cb.grid(row=1, column=1, padx=5, pady=2, sticky="w")
        type_cb.bind("<<ComboboxSelected>>", on_edit)

        ttk.Label(edit_frame, text="Formula:").grid(row=1, column=2, sticky="e")
        self.formula_var = tk.StringVar()
        self.formula_var.trace_add("write", on_edit)
        ttk.Entry(edit_frame, textvariable=self.formula_var, width=50).grid(row=1, column=3, columnspan=3, padx=5, pady=2, sticky="w")

        # Buttons
        btn_frame = ttk.Frame(top_pane)
        btn_frame.pack(fill=tk.X, padx=5, pady=5)
        ttk.Button(btn_frame, text="Add/Update Signal to List", command=self._add_signal).pack(side=tk.LEFT)
        ttk.Button(btn_frame, text="Remove Selected", command=self._remove_signal).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Save & Close", command=self._save).pack(side=tk.RIGHT)


        # --- Bottom Pane: Live Data View ---
        bottom_pane = ttk.LabelFrame(paned, text="Live History Preview", padding="5")
        paned.add(bottom_pane, weight=1) # Expand vertically
        
        columns = ("Time", "Dir", "Data", "Decoded")
        self.tree_history = ttk.Treeview(bottom_pane, columns=columns, show="headings")
        self.tree_history.heading("Time", text="Time")
        self.tree_history.heading("Dir", text="Dir")
        self.tree_history.heading("Data", text="Raw Data")
        self.tree_history.heading("Decoded", text="Live Decoded Result")
        
        self.tree_history.column("Time", width=100)
        self.tree_history.column("Dir", width=60)
        self.tree_history.column("Data", width=200)
        self.tree_history.column("Decoded", width=400)
        
        # Add Scrollbar
        scrollbar = ttk.Scrollbar(bottom_pane, orient=tk.VERTICAL, command=self.tree_history.yview)
        self.tree_history.configure(yscrollcommand=scrollbar.set)
        
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.tree_history.pack(fill=tk.BOTH, expand=True)

    def _preview_current_edit(self):
        """Updates the history table based on the CURRENT fields in the editor, without saving"""
        try:
            start_byte = int(self.start_var.get() or 0)
            length = int(self.len_var.get() or 1)
        except ValueError:
            return # Invalid input while typing
            
        test_signal = {
            "name": self.name_var.get() or "Preview",
            "start_byte": start_byte,
            "length": length,
            "type": self.type_var.get(),
            "scale": 1.0,
            "offset": 0.0,
            "formula": self.formula_var.get()
        }
        
        self._refresh_history_preview(preview_signal=test_signal)

    def _refresh_history_preview(self, preview_signal=None):
        from logic.decoder import Decoder
        
        # Don't refresh if no history
        if not hasattr(self, 'tree_history'): return
        
        for item in self.tree_history.get_children():
            self.tree_history.delete(item)
            
        # Determine which signals to use for decoding
        signals_to_use = list(self.signals)
        if preview_signal:
            # For simplicity, we just clear and use ONLY the preview signal 
            # to make it very clear what is being edited
            signals_to_use = [preview_signal]
            
        filter_pid = None
        if hasattr(self, 'pid_filter_var') and self.pid_filter_var.get() != "All":
            filter_pid = int(self.pid_filter_var.get(), 16)
            
        for msg in self.history_messages:
            data = bytes.fromhex(msg['data'])
            
            # Apply PID filter
            if self.pid is not None and filter_pid is not None:
                if len(data) <= self.pid_index or data[self.pid_index] != filter_pid:
                    continue

            decoded_str = ""
            if signals_to_use:
                parts = []
                for sig in signals_to_use:
                    val = Decoder.decode(data, sig["start_byte"]*8, sig["length"]*8, sig["type"], 
                                       1.0, 0.0, sig.get("formula"))
                    parts.append(f"{sig['name']}: {val}")
                decoded_str = ", ".join(parts)
            else:
                decoded_str = "No decoder defined"
                
            # Convert timestamp
            time_str = time.strftime('%H:%M:%S', time.localtime(msg['timestamp'])) + f".{int(msg['timestamp']%1*1000):03d}"
                
            self.tree_history.insert("", tk.END, values=(
                time_str,
                msg['direction'],
                msg['data'],
                decoded_str
            ))
            
        # Scroll to bottom to see latest
        if self.tree_history.get_children():
            self.tree_history.yview_moveto(1)

    def _load_preset(self):
        preset_name = self.preset_var.get()
        if preset_name in self.PRESETS:
            preset = self.PRESETS[preset_name]
            self.name_var.set(preset["name"])
            self.start_var.set(str(preset["start_byte"]))
            self.len_var.set(str(preset["length"]))
            self.type_var.set(preset["type"])
            self.formula_var.set(preset["formula"])
            # The trace will automatically trigger _preview_current_edit

    def _on_select(self, event):
        selected = self.tree_signals.selection()
        if selected:
            idx = self.tree_signals.index(selected[0])
            sig = self.signals[idx]
            self.name_var.set(sig.get("name", ""))
            self.start_var.set(str(sig.get("start_byte", 0)))
            self.len_var.set(str(sig.get("length", 1)))
            self.type_var.set(sig.get("type", "uint8"))
            self.formula_var.set(sig.get("formula", "") or "")

    def _refresh_signal_list(self):
        for item in self.tree_signals.get_children():
            self.tree_signals.delete(item)
        
        for sig in self.signals:
            form = sig.get("formula", "")
            if not form: form = ""
            self.tree_signals.insert("", tk.END, values=(
                sig.get("name","N/A"), 
                sig.get("start_byte",0), 
                sig.get("length",0), 
                sig.get("type","N/A"), 
                sig.get("scale",1.0),
                form
            ))

    def _add_signal(self):
        try:
            start_byte = int(self.start_var.get() or 0)
            length = int(self.len_var.get() or 1)
        except ValueError:
            return

        new_sig = {
            "name": self.name_var.get(),
            "start_byte": start_byte,
            "length": length,
            "type": self.type_var.get(),
            "scale": 1.0,
            "offset": 0.0,
            "formula": self.formula_var.get() if self.type_var.get() == "formula" else None
        }
        
        selected = self.tree_signals.selection()
        if selected:
            idx = self.tree_signals.index(selected[0])
            self.signals[idx] = new_sig
        else:
            self.signals.append(new_sig)
            
        self._refresh_signal_list()
        self._refresh_history_preview() # Reset preview to show all saved signals
        
        for item in self.tree_signals.selection():
            self.tree_signals.selection_remove(item)

    def _remove_signal(self):
        selected = self.tree_signals.selection()
        if selected:
            idx = self.tree_signals.index(selected[0])
            del self.signals[idx]
            self._refresh_signal_list()
            self._refresh_history_preview()

    def _save(self):
        self.on_save(self.can_id, self.signals)
        self.destroy()
