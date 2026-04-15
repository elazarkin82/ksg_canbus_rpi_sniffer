import tkinter as tk
from tkinter import ttk
import math

class CockpitPanel(ttk.Frame):
    def __init__(self, parent, profile_manager):
        super().__init__(parent)
        self.profile_manager = profile_manager
        
        # State
        self.mode = tk.StringVar(value="PASSIVE")
        self.steering_angle = 0.0 # -100 to 100 for drawing
        self.throttle_percent = 0.0 # 0 to 100
        self.brake_percent = 0.0 # 0 to 100
        
        # Active Mode Input Flags
        self.keys = {
            "left": False,
            "right": False,
            "up": False,
            "down": False
        }
        
        self._setup_ui()
        
        # Start physics loop
        self.after(50, self._update_physics)

    def _setup_ui(self):
        # 1. Control Bar
        ctrl_frame = ttk.Frame(self, padding="10")
        ctrl_frame.pack(fill=tk.X)
        
        ttk.Label(ctrl_frame, text="Cockpit Mode:", font=('Arial', 12, 'bold')).pack(side=tk.LEFT, padx=10)
        
        rb_passive = ttk.Radiobutton(ctrl_frame, text="Passive View (Listen to CAN)", variable=self.mode, value="PASSIVE", command=self._on_mode_change)
        rb_passive.pack(side=tk.LEFT, padx=10)
        
        rb_active = ttk.Radiobutton(ctrl_frame, text="Active Control (Keyboard/Joystick)", variable=self.mode, value="ACTIVE", command=self._on_mode_change)
        rb_active.pack(side=tk.LEFT, padx=10)
        
        # Mapping Info
        self.lbl_mapping = ttk.Label(ctrl_frame, text="Configure mappings in Reverse Engineering tab.", foreground="gray")
        self.lbl_mapping.pack(side=tk.RIGHT, padx=10)

        # 2. Main Dashboard Area
        dash_frame = ttk.Frame(self)
        dash_frame.pack(fill=tk.BOTH, expand=True, pady=20)
        
        # Left Pedal (Brake)
        brake_frame = ttk.Frame(dash_frame)
        brake_frame.pack(side=tk.LEFT, expand=True)
        ttk.Label(brake_frame, text="Brake", font=('Arial', 14)).pack()
        self.cv_brake = tk.Canvas(brake_frame, width=60, height=300, bg='white', relief=tk.SUNKEN, borderwidth=2)
        self.cv_brake.pack(pady=10)
        self.lbl_brake_val = ttk.Label(brake_frame, text="0%", font=('Arial', 16, 'bold'))
        self.lbl_brake_val.pack()

        # Center (Steering Wheel)
        steer_frame = ttk.Frame(dash_frame)
        steer_frame.pack(side=tk.LEFT, expand=True)
        ttk.Label(steer_frame, text="Steering", font=('Arial', 14)).pack()
        self.cv_steer = tk.Canvas(steer_frame, width=400, height=400)
        self.cv_steer.pack(pady=10)
        self.lbl_steer_val = ttk.Label(steer_frame, text="0°", font=('Arial', 24, 'bold'))
        self.lbl_steer_val.pack()

        # Right Pedal (Throttle)
        throttle_frame = ttk.Frame(dash_frame)
        throttle_frame.pack(side=tk.LEFT, expand=True)
        ttk.Label(throttle_frame, text="Throttle", font=('Arial', 14)).pack()
        self.cv_throttle = tk.Canvas(throttle_frame, width=60, height=300, bg='white', relief=tk.SUNKEN, borderwidth=2)
        self.cv_throttle.pack(pady=10)
        self.lbl_throttle_val = ttk.Label(throttle_frame, text="0%", font=('Arial', 16, 'bold'))
        self.lbl_throttle_val.pack()

        # Initial Draw
        self._draw_all()

    def _on_mode_change(self):
        # Reset values safely
        self.steering_angle = 0.0
        self.throttle_percent = 0.0
        self.brake_percent = 0.0
        self.keys = {k: False for k in self.keys}
        
        top = self.winfo_toplevel()
        
        if self.mode.get() == "ACTIVE":
            # Bind keys
            top.bind("<KeyPress-Left>", lambda e: self._on_key(e, "left", True))
            top.bind("<KeyRelease-Left>", lambda e: self._on_key(e, "left", False))
            top.bind("<KeyPress-Right>", lambda e: self._on_key(e, "right", True))
            top.bind("<KeyRelease-Right>", lambda e: self._on_key(e, "right", False))
            top.bind("<KeyPress-Up>", lambda e: self._on_key(e, "up", True))
            top.bind("<KeyRelease-Up>", lambda e: self._on_key(e, "up", False))
            top.bind("<KeyPress-Down>", lambda e: self._on_key(e, "down", True))
            top.bind("<KeyRelease-Down>", lambda e: self._on_key(e, "down", False))
            self.lbl_mapping.config(text="ACTIVE MODE: Use Arrow Keys to drive", foreground="blue")
        else:
            # Unbind keys
            top.unbind("<KeyPress-Left>")
            top.unbind("<KeyRelease-Left>")
            top.unbind("<KeyPress-Right>")
            top.unbind("<KeyRelease-Right>")
            top.unbind("<KeyPress-Up>")
            top.unbind("<KeyRelease-Up>")
            top.unbind("<KeyPress-Down>")
            top.unbind("<KeyRelease-Down>")
            self.lbl_mapping.config(text="PASSIVE MODE: Listening to CAN Bus", foreground="gray")
            
        self._draw_all()

    def _on_key(self, event, key, state):
        self.keys[key] = state

    def _update_physics(self):
        # Run every 50ms
        if self.mode.get() == "ACTIVE":
            changed = False
            
            # Steering Logic
            if self.keys["left"]:
                self.steering_angle -= 5.0
                if self.steering_angle < -100: self.steering_angle = -100
                changed = True
            elif self.keys["right"]:
                self.steering_angle += 5.0
                if self.steering_angle > 100: self.steering_angle = 100
                changed = True
            else:
                # Decay to center
                if self.steering_angle > 0:
                    self.steering_angle -= 5.0
                    if self.steering_angle < 0: self.steering_angle = 0
                    changed = True
                elif self.steering_angle < 0:
                    self.steering_angle += 5.0
                    if self.steering_angle > 0: self.steering_angle = 0
                    changed = True

            # Throttle Logic
            if self.keys["up"]:
                self.throttle_percent += 5.0
                if self.throttle_percent > 100: self.throttle_percent = 100
                changed = True
            else:
                if self.throttle_percent > 0:
                    self.throttle_percent -= 10.0 # Faster decay for pedals
                    if self.throttle_percent < 0: self.throttle_percent = 0
                    changed = True

            # Brake Logic
            if self.keys["down"]:
                self.brake_percent += 10.0 # Brakes hit hard
                if self.brake_percent > 100: self.brake_percent = 100
                changed = True
            else:
                if self.brake_percent > 0:
                    self.brake_percent -= 10.0
                    if self.brake_percent < 0: self.brake_percent = 0
                    changed = True

            if changed:
                self._draw_all()
                # FUTURE: Here we will calculate raw values and inject via OBD2Manager/Client

        # Reschedule loop
        self.after(50, self._update_physics)

    # --- Setters (Abstraction Layer) ---
    
    def set_steering(self, normalized_percent):
        """normalized_percent: -100 (left) to +100 (right)"""
        self.steering_angle = max(-100, min(100, normalized_percent))
        self._draw_steering()

    def set_throttle(self, percent):
        """Percent 0 to 100"""
        self.throttle_percent = max(0, min(100, percent))
        self._draw_pedal(self.cv_throttle, self.throttle_percent, 'green', self.lbl_throttle_val)

    def set_brake(self, percent):
        """Percent 0 to 100"""
        self.brake_percent = max(0, min(100, percent))
        self._draw_pedal(self.cv_brake, self.brake_percent, 'red', self.lbl_brake_val)

    # --- Drawing Methods ---

    def _draw_all(self):
        self._draw_steering()
        self._draw_pedal(self.cv_throttle, self.throttle_percent, 'green', self.lbl_throttle_val)
        self._draw_pedal(self.cv_brake, self.brake_percent, 'red', self.lbl_brake_val)

    def _draw_steering(self):
        c = self.cv_steer
        c.delete("all")
        
        # Wheel parameters
        cx, cy = 200, 200
        r_outer = 150
        r_inner = 130
        
        # Outer thick ring
        c.create_oval(cx - r_outer, cy - r_outer, cx + r_outer, cy + r_outer, width=15, outline="#333")
        
        # Map normalized -100..100 to visual angle (-180 to 180 degrees)
        visual_angle = (self.steering_angle / 100.0) * 180.0
        
        # Angle in radians
        # 0 degrees is straight up (so we subtract 90 deg / PI/2)
        rad = math.radians(visual_angle - 90)
        
        # Draw center hub
        c.create_oval(cx - 30, cy - 30, cx + 30, cy + 30, fill="#555")
        
        # Draw main indicator spoke (top)
        x2 = cx + r_inner * math.cos(rad)
        y2 = cy + r_inner * math.sin(rad)
        c.create_line(cx, cy, x2, y2, width=10, fill="red") # Top indicator is red
        
        # Draw side spokes (Y shape)
        rad_left = math.radians(visual_angle + 150)
        x_l = cx + r_inner * math.cos(rad_left)
        y_l = cy + r_inner * math.sin(rad_left)
        c.create_line(cx, cy, x_l, y_l, width=20, fill="#666")
        
        rad_right = math.radians(visual_angle + 30)
        x_r = cx + r_inner * math.cos(rad_right)
        y_r = cy + r_inner * math.sin(rad_right)
        c.create_line(cx, cy, x_r, y_r, width=20, fill="#666")

        # Update label with the normalized percentage for clarity
        self.lbl_steer_val.config(text=f"{int(self.steering_angle)}%")

    def _draw_pedal(self, canvas, percent, color, label):
        canvas.delete("all")
        w = 60
        h = 300
        
        # Calculate fill height
        fill_h = (percent / 100.0) * h
        
        # Draw filled rect from bottom
        canvas.create_rectangle(0, h - fill_h, w, h, fill=color, outline="")
        
        # Draw grid lines for effect
        for i in range(10):
            y = i * (h / 10)
            canvas.create_line(0, y, w, y, fill="#ccc")
            
        label.config(text=f"{int(percent)}%")

    # --- Incoming Data Handler ---
    
    def on_message(self, can_id, data):
        """Called by main loop when a new CAN message arrives"""
        if self.mode.get() != "PASSIVE":
            return
            
        # Optimization: only process if we have mappings defined
        mappings = self.profile_manager.data.get("controls_mapping", {})
        if not mappings:
            return
            
        from logic.decoder import Decoder
            
        # Check if this ID matches any of our controls
        for control_name, mapping in mappings.items():
            if mapping.get("can_id") == can_id:
                # Check PID match if required
                mapped_pid = mapping.get("pid")
                
                config = self.profile_manager.get_message_config(can_id)
                has_pid = config.get("has_pid", False)
                pid_index = config.get("pid_index", 0)
                
                current_pid = None
                if has_pid and len(data) > pid_index:
                    current_pid = data[pid_index]
                    
                if mapped_pid is not None and current_pid != mapped_pid:
                    continue # Not the right PID
                    
                # We found a match! Decode it.
                sig_idx = mapping.get("signal_index", 0)
                signals = self.profile_manager.get_signals(can_id, current_pid)
                if signals and sig_idx < len(signals):
                    sig = signals[sig_idx]
                    
                    # Ensure start_byte is adjusted for OBD2 payload
                    start_b = sig["start_byte"]
                    if config.get("protocol") == "OBD2":
                         # The decoder expects the start_bit relative to the whole frame
                         # but our formulas use x[0]. The Decoder class slices from start_byte.
                         pass 
                    
                    val = Decoder.decode(data, start_b*8, sig["length"]*8, sig["type"], 
                                       1.0, 0.0, sig.get("formula"))
                    
                    if isinstance(val, (int, float)):
                        # Normalize physical value to GUI percentage
                        min_v = mapping.get("min_val", 0.0)
                        max_v = mapping.get("max_val", 100.0)
                        
                        range_total = max_v - min_v
                        if range_total == 0: range_total = 1.0
                        
                        # Calculate percentage (0.0 to 1.0)
                        percent = (val - min_v) / range_total
                        
                        if control_name == "steering":
                            # map [0.0, 1.0] to [-100, 100]
                            normalized = (percent * 200.0) - 100.0
                            self.set_steering(normalized)
                            
                        elif control_name == "throttle":
                            normalized = percent * 100.0
                            self.set_throttle(normalized)
                            
                        elif control_name == "brake":
                            normalized = percent * 100.0
                            self.set_brake(normalized)
