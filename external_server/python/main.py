#!/usr/bin/env python3

import tkinter as tk
from tkinter import ttk, messagebox
import threading
import time
import backend.udp_client # Import module to access global DEBUG_MODE
from backend.udp_client import UdpClient
from logic.settings_manager import SettingsManager
from logic.profile_manager import ProfileManager
from gui.settings_dialog import SettingsDialog
from gui.reverse_engineering import ReverseEngineeringPanel
from gui.obd2_panel import OBD2Panel # New import
from gui.cockpit_panel import CockpitPanel # New import
from gui.status_window import SnifferStatusWindow
from gui.sniffer_remote_settings import SnifferRemoteSettings

# Command IDs (Must match C++ header)
CMD_CAN_MSG_FROM_SYSTEM = 0x2001
CMD_CAN_MSG_FROM_COMPUTER = 0x2002
CMD_GET_PARAMS_RES = 0x2006

class MainApp:
    def __init__(self, root):
        self.root = root
        self.root.title("External Server - CAN Sniffer Control")
        
        # Start maximized
        try:
            self.root.state('zoomed')
        except tk.TclError:
            # Fallback for systems that don't support 'zoomed' (e.g. some Linux/Mac)
            try:
                self.root.attributes('-zoomed', True)
            except tk.TclError:
                self.root.geometry("1200x800") # Larger default fallback
        
        self.settings_manager = SettingsManager()
        
        # Ensure default setting for max messages exists
        if "max_log_messages" not in self.settings_manager.settings:
            self.settings_manager.settings["max_log_messages"] = 100000
            self.settings_manager.save()
            
        self.profile_manager = ProfileManager()
        self.client = None
        self.running = False
        self.logging_active = False
        self.ui_connected_state = False # Track UI state for logging
        self.last_status_print_time = 0 # To limit console spam
        self.status_window = None
        self.remote_settings_window = None
        
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

        sniffer_menu = tk.Menu(menubar, tearoff=0)
        sniffer_menu.add_command(label="Sniffer Status", command=self.show_sniffer_status)
        sniffer_menu.add_command(label="Remote Settings", command=self.show_remote_settings)
        menubar.add_cascade(label="Sniffer", menu=sniffer_menu)
        
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
        self.obd2_panel = None # Will be created on connect
        self.tab_rules = ttk.Frame(self.notebook)
        
        # Initialize Cockpit Panel
        self.cockpit_panel = CockpitPanel(self.notebook, self.profile_manager)
        
        self.notebook.add(self.rev_eng_panel, text="Reverse Engineering")
        # OBD2 tab will be added on connect
        self.notebook.add(self.tab_rules, text="Rules Manager")
        self.notebook.add(self.cockpit_panel, text="Cockpit")

    def open_settings(self):
        SettingsDialog(self.root, self.settings_manager)

    def show_sniffer_status(self):
        if self.status_window is None:
            self.status_window = SnifferStatusWindow(self.root, self)
        else:
            self.status_window.lift()

    def show_remote_settings(self):
        if self.remote_settings_window is None or not self.remote_settings_window.winfo_exists():
            self.remote_settings_window = SnifferRemoteSettings(self.root, self.client)
        else:
            self.remote_settings_window.lift()

    def on_status_window_close(self):
        self.status_window = None
        
    def toggle_debug(self):
        backend.udp_client.DEBUG_MODE = self.debug_var.get()
        print(f"Debug Mode set to: {backend.udp_client.DEBUG_MODE}")

    def toggle_connection(self):
        if not self.client:
            try:
                s = self.settings_manager.settings
                print(f"[Python] Attempting to connect to {s['sniffer_ip']}:{s['sniffer_port']}...")
                self.client = UdpClient(
                    ip=s["sniffer_ip"],
                    remote_port=int(s["sniffer_port"]),
                    local_port=int(s["local_port"]),
                    keep_alive_ms=int(s["keep_alive_ms"])
                )
                
                if self.client.start():
                    print("[Python] Client started. Waiting for data/keep-alive...")
                    self.lbl_status.config(text="Connecting...", foreground="orange")
                    self.btn_connect.config(text="Disconnect")
                    self.btn_log.config(state=tk.NORMAL, text="Start Logging")
                    self.logging_active = False
                    self.running = True
                    self.ui_connected_state = False
                    
                    # Create OBD2 panel now that we have a client
                    if not self.obd2_panel:
                        self.obd2_panel = OBD2Panel(self.notebook, self.client)
                        self.notebook.insert(1, self.obd2_panel, text="OBD2 Scanner")
                    
                    threading.Thread(target=self.rx_loop, daemon=True).start()
                else:
                    print("[Python] Failed to start UDP client.")
                    self.lbl_status.config(text="Failed to start", foreground="red")
            except Exception as e:
                print(f"[Python] Error during connection: {e}")
                self.lbl_status.config(text=f"Error: {e}", foreground="red")
        else:
            print("[Python] Disconnecting...")
            self.running = False
            self.logging_active = False
            if self.obd2_panel:
                self.obd2_panel.manager.stop_polling()
            self.client.close()
            self.client = None
            self.ui_connected_state = False
            self.lbl_status.config(text="Disconnected", foreground="red")
            self.btn_connect.config(text="Connect")
            self.btn_log.config(state=tk.DISABLED, text="Start Logging")

    def toggle_logging(self):
        if self.client:
            self.logging_active = not self.logging_active
            print(f"[Python] Setting logging to: {self.logging_active}")
            self.client.set_logging(self.logging_active)
            
            if self.logging_active:
                # Update max limit from settings before starting
                max_msgs = int(self.settings_manager.settings.get("max_log_messages", 100000))
                self.rev_eng_panel.recorder.max_messages = max_msgs
                self.rev_eng_panel.recorder.limit_reached = False
                
                self.btn_log.config(text="Stop Logging")
            else:
                self.btn_log.config(text="Start Logging")
                
            # Update UI state
            self.rev_eng_panel.set_logging_state(self.logging_active)

    def force_stop_logging_due_to_limit(self):
        if self.logging_active:
            print("[Python] Logging limit reached. Stopping automatically.")
            self.logging_active = False
            if self.client:
                self.client.set_logging(False)
            self.btn_log.config(text="Start Logging")
            self.rev_eng_panel.set_logging_state(False)
            
            limit = self.settings_manager.settings.get("max_log_messages", 100000)
            messagebox.showwarning(
                "Logging Stopped", 
                f"Logging automatically stopped because the memory limit of {limit} messages was reached.\n\n"
                "You can increase this limit in File -> Settings."
            )

    def rx_loop(self):
        last_conn_check = 0
        while self.running and self.client:
            # Periodically check connection status from library
            now = time.time()
            if now - last_conn_check > 0.5:
                is_connected = self.client.is_connected()
                if is_connected != self.ui_connected_state:
                    self.ui_connected_state = is_connected
                    status_text = "Connected" if is_connected else "Connecting..."
                    color = "green" if is_connected else "orange"
                    print(f"[Python] Connection status changed: {status_text}")
                    self.root.after(0, lambda t=status_text, c=color: self.lbl_status.config(text=t, foreground=c))
                
                # Update settings window button states if open
                if self.remote_settings_window and self.remote_settings_window.winfo_exists():
                    self.root.after(0, self.remote_settings_window.update_button_states)

                # Print sniffer status every 0.5 seconds
                if is_connected and (now - self.last_status_print_time > 0.5):
                    status_msg = self.client.get_sniffer_status()
                    if status_msg:
                        # Update status window if it exists
                        if self.status_window:
                            self.root.after(0, self.status_window.update_status, status_msg)
                    self.last_status_print_time = now
                
                last_conn_check = now

            msg = self.client.read_message(timeout_ms=100)
            if msg:
                if msg.command == CMD_GET_PARAMS_RES:
                    params_str = msg.data.decode('utf-8', errors='replace')
                    if self.remote_settings_window and self.remote_settings_window.winfo_exists():
                        self.root.after(0, self.remote_settings_window.on_params_received, params_str)
                    continue

                direction = "Unknown"
                if msg.command == CMD_CAN_MSG_FROM_SYSTEM:
                    direction = "SYS->ECU"
                elif msg.command == CMD_CAN_MSG_FROM_COMPUTER:
                    direction = "ECU->SYS"
                
                if hasattr(msg, 'can_id'):
                    # Update base payload for mapped controls (Passive mode snapshot)
                    hex_id = hex(msg.can_id)
                    mappings = self.profile_manager.data.get("controls_mapping", {})
                    for ctrl_name, mapping in mappings.items():
                        if mapping.get("can_id") == msg.can_id:
                            # Update the snapshot for future injection
                            self.profile_manager.update_control_base_payload(ctrl_name, msg.frame_data.hex())

                    # Schedule updates on the main thread
                    # msg.time_ms is the time from sniffer start
                    self.root.after(0, self.dispatch_message, time.time(), direction, msg.can_id, msg.frame_data, msg.time_ms)

    def dispatch_message(self, timestamp, direction, can_id, frame_data, time_ms=0.0):
        # Check if limit was reached during dispatch
        if self.logging_active and self.rev_eng_panel.recorder.limit_reached:
            self.force_stop_logging_due_to_limit()
            return

        # Dispatch to all interested panels
        self.rev_eng_panel.on_message(timestamp, direction, can_id, frame_data, time_ms)
        if self.obd2_panel:
            self.obd2_panel.on_message(can_id, frame_data)
        self.cockpit_panel.on_message(can_id, frame_data)

if __name__ == "__main__":
    root = tk.Tk()
    app = MainApp(root)
    root.mainloop()
