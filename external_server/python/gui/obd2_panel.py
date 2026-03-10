import tkinter as tk
from tkinter import ttk
from logic.obd2_manager import OBD2Manager

class OBD2Panel(ttk.Frame):
    def __init__(self, parent, client):
        super().__init__(parent)
        self.client = client
        self.manager = OBD2Manager(client, self.on_pid_update)
        self.pid_vars = {} # PID -> BooleanVar
        self.pid_values = {} # PID -> Tree Item ID
        
        self._setup_ui()

    def _setup_ui(self):
        # Control Frame
        control_frame = ttk.Frame(self)
        control_frame.pack(fill=tk.X, padx=5, pady=5)
        
        self.btn_start = ttk.Button(control_frame, text="Start Polling", command=self.toggle_polling)
        self.btn_start.pack(side=tk.LEFT)
        
        ttk.Label(control_frame, text="Interval (s):").pack(side=tk.LEFT, padx=(10, 2))
        self.interval_var = tk.DoubleVar(value=0.1)
        ttk.Entry(control_frame, textvariable=self.interval_var, width=5).pack(side=tk.LEFT)
        
        # Table
        columns = ("PID", "Name", "Value", "Unit")
        self.tree = ttk.Treeview(self, columns=columns, show="headings")
        
        self.tree.heading("PID", text="PID")
        self.tree.heading("Name", text="Name")
        self.tree.heading("Value", text="Value")
        self.tree.heading("Unit", text="Unit")
        
        self.tree.column("PID", width=50)
        self.tree.column("Name", width=150)
        self.tree.column("Value", width=100)
        self.tree.column("Unit", width=50)
        
        self.tree.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Populate Table
        for pid, info in sorted(OBD2Manager.PIDS.items()):
            item_id = self.tree.insert("", tk.END, values=(hex(pid), info["name"], "-", info["unit"]))
            self.pid_values[pid] = item_id
            
        # Selection Frame (Checkboxes)
        select_frame = ttk.LabelFrame(self, text="Select PIDs to Poll")
        select_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Create a grid of checkboxes
        row = 0
        col = 0
        for pid, info in sorted(OBD2Manager.PIDS.items()):
            var = tk.BooleanVar(value=False)
            self.pid_vars[pid] = var
            cb = ttk.Checkbutton(select_frame, text=f"{info['name']} ({hex(pid)})", variable=var, command=self.update_active_pids)
            cb.grid(row=row, column=col, sticky="w", padx=5, pady=2)
            
            col += 1
            if col > 3:
                col = 0
                row += 1

    def toggle_polling(self):
        if self.manager.running:
            self.manager.stop_polling()
            self.btn_start.config(text="Start Polling")
        else:
            try:
                interval = float(self.interval_var.get())
                self.manager.interval = interval
            except:
                pass
            self.update_active_pids()
            self.manager.start_polling()
            self.btn_start.config(text="Stop Polling")

    def update_active_pids(self):
        active = [pid for pid, var in self.pid_vars.items() if var.get()]
        self.manager.set_active_pids(active)

    def on_pid_update(self, pid, value_str):
        # This is called from a thread, so we need to schedule UI update
        self.after(0, self._update_tree_item, pid, value_str)

    def _update_tree_item(self, pid, value_str):
        if pid in self.pid_values:
            item_id = self.pid_values[pid]
            current_vals = self.tree.item(item_id, "values")
            # Update Value column (index 2)
            new_vals = (current_vals[0], current_vals[1], value_str, current_vals[3])
            self.tree.item(item_id, values=new_vals)

    def on_message(self, can_id, data):
        # Forward message to manager for processing
        self.manager.process_message(can_id, data)
