import tkinter as tk
from tkinter import ttk

class SettingsDialog(tk.Toplevel):
    def __init__(self, parent, settings_manager):
        super().__init__(parent)
        self.title("Settings")
        self.geometry("300x250")
        self.settings_manager = settings_manager
        self.entries = {}

        self._setup_ui()

    def _setup_ui(self):
        frame = ttk.Frame(self, padding="10")
        frame.pack(fill=tk.BOTH, expand=True)

        row = 0
        for key, value in self.settings_manager.settings.items():
            label = ttk.Label(frame, text=key.replace("_", " ").title() + ":")
            label.grid(row=row, column=0, sticky=tk.W, pady=5)
            
            entry = ttk.Entry(frame)
            entry.insert(0, str(value))
            entry.grid(row=row, column=1, sticky=tk.EW, pady=5)
            
            self.entries[key] = entry
            row += 1

        frame.columnconfigure(1, weight=1)

        btn_frame = ttk.Frame(frame)
        btn_frame.grid(row=row, column=0, columnspan=2, pady=10)

        save_btn = ttk.Button(btn_frame, text="Save", command=self.save)
        save_btn.pack(side=tk.LEFT, padx=5)

        cancel_btn = ttk.Button(btn_frame, text="Cancel", command=self.destroy)
        cancel_btn.pack(side=tk.LEFT, padx=5)

    def save(self):
        for key, entry in self.entries.items():
            val = entry.get()
            # Try to convert to int if original was int
            if isinstance(self.settings_manager.DEFAULT_SETTINGS.get(key), int):
                try:
                    val = int(val)
                except ValueError:
                    pass # Keep as string if conversion fails (or handle error)
            
            self.settings_manager.settings[key] = val
        
        self.settings_manager.save()
        self.destroy()
