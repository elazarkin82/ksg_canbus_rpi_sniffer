#!/usr/bin/env python3

import tkinter as tk
from tkinter import ttk
import threading
import time
import backend.udp_client # Import module to access global DEBUG_MODE
from backend.udp_client import UdpClient
from logic.settings_manager import SettingsManager
from logic.profile_manager import ProfileManager
from gui.settings_dialog import SettingsDialog
from gui.reverse_engineering import ReverseEngineeringPanel

# Command IDs (Must match C++ header)
CMD_CAN_MSG_FROM_SYSTEM = 0x2001
CMD_CAN_MSG_FROM_COMPUTER = 0x2002

class MainApp:
    def __init__(self, root):
        self.root = root
        self.root.title("External Server - CAN Sniffer Control")
        self.root.geometry("1000x700")
        
        self.settings_manager = SettingsManager()
        self.profile_manager = ProfileManager()
        self.client = None
        self.running = False
        self.logging_active = False
        
        self.debug_var = tk.BooleanVar(value=False)

        self._setup_ui()

    def _setup_ui(self):
        # Menu Bar
        menubar = tk.Menu(self.root)
        
        file_menu = tk.Menu(menubar, tearoff=0)
        file_menu.add_command(label="Settings", command=self.open_settings)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.root.quit)
        menubar.add_cascade(label="File", menu=file_menu)
        
        view_menu = tk.Menu(menubar, tearoff=0)
        view_menu.add_checkbutton(label="Debug Mode", onvalue=True, offvalue=False, variable=self.debug_var, command=self.toggle_debug)
        menubar.add_cascade(label="View", menu=view_menu)
        
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
        
        self.rev_eng_panel = ReverseEngineeringPanel(self.notebook, self.profile_manager)
        self.tab_rules = ttk.Frame(self.notebook)
        self.tab_cockpit = ttk.Frame(self.notebook)
        
        self.notebook.add(self.rev_eng_panel, text="Reverse Engineering")
        self.notebook.add(self.tab_rules, text="Rules Manager")
        self.notebook.add(self.tab_cockpit, text="Cockpit")

    def open_settings(self):
        SettingsDialog(self.root, self.settings_manager)
        
    def toggle_debug(self):
        backend.udp_client.DEBUG_MODE = self.debug_var.get()
        print(f"Debug Mode set to: {backend.udp_client.DEBUG_MODE}")

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
                    self.btn_log.config(state=tk.NORMAL, text="Start Logging")
                    self.logging_active = False
                    self.running = True
                    threading.Thread(target=self.rx_loop, daemon=True).start()
                else:
                    self.lbl_status.config(text="Failed to start", foreground="red")
            except Exception as e:
                print(f"Error: {e}")
                self.lbl_status.config(text=f"Error: {e}", foreground="red")
        else:
            self.running = False
            self.logging_active = False
            self.client.close()
            self.client = None
            self.lbl_status.config(text="Disconnected", foreground="red")
            self.btn_connect.config(text="Connect")
            self.btn_log.config(state=tk.DISABLED, text="Start Logging")

    def toggle_logging(self):
        if self.client:
            self.logging_active = not self.logging_active
            self.client.set_logging(self.logging_active)
            
            if self.logging_active:
                self.btn_log.config(text="Stop Logging")
            else:
                self.btn_log.config(text="Start Logging")

    def rx_loop(self):
        while self.running and self.client:
            msg = self.client.read_message()
            if msg:
                # msg is a simple object with command, data, can_id, dlc, frame_data
                
                direction = "Unknown"
                if msg.command == CMD_CAN_MSG_FROM_SYSTEM:
                    direction = "SYS->ECU"
                elif msg.command == CMD_CAN_MSG_FROM_COMPUTER:
                    direction = "ECU->SYS"
                
                # Update UI via Panel - MUST be done in Main Thread
                if hasattr(msg, 'can_id'):
                    # Use root.after to schedule the update on the main thread
                    self.root.after(0, lambda t=time.time(), d=direction, i=msg.can_id, f=msg.frame_data: 
                                    self.rev_eng_panel.on_message(t, d, i, f))

if __name__ == "__main__":
    root = tk.Tk()
    app = MainApp(root)
    root.mainloop()
