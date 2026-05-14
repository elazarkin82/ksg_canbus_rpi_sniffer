import tkinter as tk
from tkinter import ttk

class SnifferStatusWindow(tk.Toplevel):
    def __init__(self, master, parent_app):
        # master is the tkinter root/window, parent_app is the MainApp instance
        super().__init__(master)
        self.title("Sniffer Status")
        self.geometry("600x400")
        
        self.parent_app = parent_app
        
        # Ensure it's not destroyed on close, just hidden or managed by parent
        self.protocol("WM_DELETE_WINDOW", self.on_close)
        
        self._setup_ui()

    def _setup_ui(self):
        self.text_area = tk.Text(self, wrap=tk.WORD, font=("Courier", 10))
        self.text_area.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Optional: Add a scrollbar
        scrollbar = ttk.Scrollbar(self.text_area, orient=tk.VERTICAL, command=self.text_area.yview)
        self.text_area.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Read-only
        self.text_area.config(state=tk.DISABLED)

    def update_status(self, status_text):
        if not self.winfo_exists():
            return
            
        self.text_area.config(state=tk.NORMAL)
        self.text_area.delete("1.0", tk.END)
        self.text_area.insert(tk.END, status_text)
        self.text_area.config(state=tk.DISABLED)

    def on_close(self):
        # Notify parent_app and destroy
        if hasattr(self.parent_app, 'on_status_window_close'):
            self.parent_app.on_status_window_close()
        self.destroy()
