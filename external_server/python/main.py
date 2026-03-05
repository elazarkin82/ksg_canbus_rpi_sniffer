#!/usr/bin/env python3

import tkinter as tk
from tkinter import ttk
import threading
import time
from backend.udp_client import UdpClient
from logic.decoder import Decoder
from logic.settings_manager import SettingsManager
from gui.settings_dialog import SettingsDialog

class MainApp:
    def __init__(self, root):
        self.root = root
        self.root.title("External Server - CAN Sniffer Control")
        self.root.geometry("800x600")
        
        self.settings_manager = SettingsManager()
        self.client = None
        self.running = False

        self._setup_ui()

    def _setup_ui(self):
        # Menu Bar
        menubar = tk.Menu(self.root)
        file_menu = tk.Menu(menubar, tearoff=0)
        file_menu.add_command(label="Settings", command=self.open_settings)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.root.quit)
        menubar.add_cascade(label="File", menu=file_menu)
        self.root.config(menu=menubar)

        # Top Bar
        top_frame = ttk.Frame(self.root, padding="5")
        top_frame.pack(fill=tk.X)
        
        self.btn_connect = ttk.Button(top_frame, text="Connect", command=self.toggle_connection)
        self.btn_connect.pack(side=tk.LEFT)
        
        self.lbl_status = ttk.Label(top_frame, text="Disconnected", foreground="red")
        self.lbl_status.pack(side=tk.LEFT, padx=10)

        self.btn_log = ttk.Button(top_frame, text="Start Logging", command=self.toggle_logging, state=tk.DISABLED)
        self.btn_log.pack(side=tk.LEFT)

        # Notebook (Tabs)
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.tab_traffic = ttk.Frame(self.notebook)
        self.tab_rules = ttk.Frame(self.notebook)
        self.tab_cockpit = ttk.Frame(self.notebook)
        
        self.notebook.add(self.tab_traffic, text="Traffic")
        self.notebook.add(self.tab_rules, text="Rules")
        self.notebook.add(self.tab_cockpit, text="Cockpit")
        
        self._setup_traffic_tab()

    def _setup_traffic_tab(self):
        # Treeview for messages
        columns = ("Time", "Dir", "ID", "Len", "Data", "Decoded")
        self.tree = ttk.Treeview(self.tab_traffic, columns=columns, show="headings")
        for col in columns:
            self.tree.heading(col, text=col)
            self.tree.column(col, width=80)
        self.tree.column("Data", width=200)
        self.tree.pack(fill=tk.BOTH, expand=True)

    def open_settings(self):
        SettingsDialog(self.root, self.settings_manager)

    def toggle_connection(self):
        if not self.client:
            try:
                s = self.settings_manager.settings
                self.client = UdpClient(
                    ip=s["sniffer_ip"],
                    remote_port=int(s["sniffer_port"]),
                    local_port=int(s["local_port"]),
                    keep_alive_ms=int(s["keep_alive_ms"])
                )
                
                if self.client.start():
                    self.lbl_status.config(text="Connected", foreground="green")
                    self.btn_connect.config(text="Disconnect")
                    self.btn_log.config(state=tk.NORMAL)
                    self.running = True
                    threading.Thread(target=self.rx_loop, daemon=True).start()
                else:
                    self.lbl_status.config(text="Failed to start", foreground="red")
            except Exception as e:
                print(f"Error: {e}")
                self.lbl_status.config(text=f"Error: {e}", foreground="red")
        else:
            self.running = False
            self.client.close()
            self.client = None
            self.lbl_status.config(text="Disconnected", foreground="red")
            self.btn_connect.config(text="Connect")
            self.btn_log.config(state=tk.DISABLED)

    def toggle_logging(self):
        if self.client:
            # Toggle logic (need state tracking)
            self.client.set_logging(True)

    def rx_loop(self):
        while self.running and self.client:
            msg = self.client.read_message()
            if msg:
                can_id = msg.frame.can_id
                length = msg.frame.len
                data = list(msg.frame.data[:length])
                magic = msg.magic_key.decode('utf-8', errors='ignore')
                
                # Update UI (thread safe way needed, but for demo direct call)
                # In real app use queue or after()
                self.tree.insert("", 0, values=(time.time(), magic, hex(can_id), length, str(data), ""))

if __name__ == "__main__":
    root = tk.Tk()
    app = MainApp(root)
    root.mainloop()
