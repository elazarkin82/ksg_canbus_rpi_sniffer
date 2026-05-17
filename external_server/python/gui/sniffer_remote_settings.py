import tkinter as tk
from tkinter import ttk, messagebox

class SnifferRemoteSettings(tk.Toplevel):
    def __init__(self, parent, client):
        super().__init__(parent)
        self.title("Sniffer Remote Settings")
        self.geometry("500x600")
        self.client = client
        self.entries = {}
        self.string_vars = {}
        self.original_values = {}
        self.is_dirty = False

        self._setup_ui()
        self.update_button_states()

    def _setup_ui(self):
        # Top buttons
        top_frame = ttk.Frame(self, padding="10")
        top_frame.pack(fill=tk.X)

        self.btn_get = ttk.Button(top_frame, text="Get Configuration", command=self.get_config)
        self.btn_get.pack(side=tk.LEFT, padx=5)

        # Scrollable area for params
        self.container = ttk.Frame(self, padding="10")
        self.container.pack(fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(self.container)
        self.scrollbar = ttk.Scrollbar(self.container, orient="vertical", command=self.canvas.yview)
        self.scrollable_frame = ttk.Frame(self.canvas)

        self.scrollable_frame.bind(
            "<Configure>",
            lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all"))
        )

        self.canvas.create_window((0, 0), window=self.scrollable_frame, anchor="nw")
        self.canvas.configure(yscrollcommand=self.scrollbar.set)

        self.canvas.pack(side="left", fill="both", expand=True)
        self.scrollbar.pack(side="right", fill="y")

        # Bottom buttons
        bottom_frame = ttk.Frame(self, padding="10")
        bottom_frame.pack(fill=tk.X)

        self.btn_set = ttk.Button(bottom_frame, text="Apply & Restart Sniffer", command=self.set_config, state=tk.DISABLED)
        self.btn_set.pack(side=tk.RIGHT, padx=5)

    def update_button_states(self):
        connected = self.client and self.client.is_connected()
        self.btn_get.config(state=tk.NORMAL if connected else tk.DISABLED)
        
        if not connected:
            self.btn_set.config(state=tk.DISABLED)
        else:
            self.btn_set.config(state=tk.NORMAL if self.is_dirty else tk.DISABLED)

    def get_config(self):
        if self.client:
            self.client.request_params()
            # We don't clear entries yet, wait for response

    def on_params_received(self, params_str):
        # Clear existing
        for widget in self.scrollable_frame.winfo_children():
            widget.destroy()
        
        self.entries = {}
        self.string_vars = {}
        self.original_values = {}
        self.is_dirty = False

        lines = params_str.strip().split('\n')
        row = 0
        for line in lines:
            if '=' not in line:
                continue
            key, val = line.split('=', 1)
            key = key.strip()
            val = val.strip()

            lbl = ttk.Label(self.scrollable_frame, text=key + ":")
            lbl.grid(row=row, column=0, sticky=tk.W, padx=5, pady=2)

            # Create and store StringVar to prevent garbage collection
            var = tk.StringVar(value=val)
            var.trace_add("write", lambda *args, k=key: self.on_change(k))
            self.string_vars[key] = var

            entry = ttk.Entry(self.scrollable_frame, width=40, textvariable=var)
            entry.grid(row=row, column=1, sticky=tk.EW, padx=5, pady=2)
            
            self.entries[key] = entry
            self.original_values[key] = val
            row += 1
        
        self.scrollable_frame.columnconfigure(1, weight=1)
        self.update_button_states()

    def on_change(self, key):
        self.is_dirty = False
        for k, entry in self.entries.items():
            if entry.get() != self.original_values[k]:
                self.is_dirty = True
                break
        self.update_button_states()

    def set_config(self):
        if not self.client or not self.is_dirty:
            return
        
        confirm = messagebox.askyesno("Confirm", "Applying settings will restart the Sniffer service. Continue?")
        if not confirm:
            return

        params_list = []
        for key, entry in self.entries.items():
            params_list.append(f"{key}={entry.get()}")
        
        params_str = "\n".join(params_list) + "\n"
        self.client.send_params(params_str)
        self.is_dirty = False
        self.update_button_states()
        messagebox.showinfo("Success", "Settings sent. Sniffer is restarting.")
        self.destroy()
