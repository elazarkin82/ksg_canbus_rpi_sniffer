import tkinter as tk
from tkinter import ttk

class DecoderEditorDialog(tk.Toplevel):
    def __init__(self, parent, can_id, current_signals, on_save_callback, pid=None):
        super().__init__(parent)
        
        title_str = f"Edit Decoder for {hex(can_id)}"
        if pid is not None:
            title_str += f" (PID: {hex(pid)})"
        self.title(title_str)
        
        self.geometry("600x400")
        
        self.can_id = can_id
        self.signals = current_signals if current_signals else []
        self.on_save = on_save_callback
        
        self._setup_ui()
        self._refresh_list()

    def _setup_ui(self):
        # List of signals
        self.tree = ttk.Treeview(self, columns=("Name", "Start", "Len", "Type", "Scale"), show="headings")
        self.tree.heading("Name", text="Name")
        self.tree.heading("Start", text="Start Byte")
        self.tree.heading("Len", text="Length")
        self.tree.heading("Type", text="Type")
        self.tree.heading("Scale", text="Scale")
        self.tree.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # Editor Frame
        edit_frame = ttk.LabelFrame(self, text="Signal Details", padding="10")
        edit_frame.pack(fill=tk.X, padx=10, pady=5)

        ttk.Label(edit_frame, text="Name:").grid(row=0, column=0)
        self.name_var = tk.StringVar()
        ttk.Entry(edit_frame, textvariable=self.name_var).grid(row=0, column=1)

        ttk.Label(edit_frame, text="Start Byte:").grid(row=0, column=2)
        self.start_var = tk.IntVar()
        ttk.Entry(edit_frame, textvariable=self.start_var, width=5).grid(row=0, column=3)

        ttk.Label(edit_frame, text="Length:").grid(row=0, column=4)
        self.len_var = tk.IntVar(value=1)
        ttk.Entry(edit_frame, textvariable=self.len_var, width=5).grid(row=0, column=5)

        ttk.Label(edit_frame, text="Type:").grid(row=1, column=0)
        self.type_var = tk.StringVar(value="uint8")
        type_cb = ttk.Combobox(edit_frame, textvariable=self.type_var, values=["uint8", "uint16_be", "uint16_le", "percent", "ascii", "formula"])
        type_cb.grid(row=1, column=1)

        ttk.Label(edit_frame, text="Scale:").grid(row=1, column=2)
        self.scale_var = tk.DoubleVar(value=1.0)
        ttk.Entry(edit_frame, textvariable=self.scale_var, width=5).grid(row=1, column=3)
        
        ttk.Label(edit_frame, text="Offset:").grid(row=1, column=4)
        self.offset_var = tk.DoubleVar(value=0.0)
        ttk.Entry(edit_frame, textvariable=self.offset_var, width=5).grid(row=1, column=5)


        btn_frame = ttk.Frame(self)
        btn_frame.pack(fill=tk.X, padx=10, pady=10)

        ttk.Button(btn_frame, text="Add/Update", command=self._add_signal).pack(side=tk.LEFT)
        ttk.Button(btn_frame, text="Remove Selected", command=self._remove_signal).pack(side=tk.LEFT, padx=5)
        
        ttk.Button(btn_frame, text="Save & Close", command=self._save).pack(side=tk.RIGHT)

    def _refresh_list(self):
        for item in self.tree.get_children():
            self.tree.delete(item)
        
        for sig in self.signals:
            self.tree.insert("", tk.END, values=(sig.get("name","N/A"), sig.get("start_byte",0), sig.get("length",0), sig.get("type","N/A"), sig.get("scale",1.0)))

    def _add_signal(self):
        # Simple add (no validation for overlap yet)
        new_sig = {
            "name": self.name_var.get(),
            "start_byte": self.start_var.get(),
            "length": self.len_var.get(),
            "type": self.type_var.get(),
            "scale": self.scale_var.get(),
            "offset": self.offset_var.get(),
            "formula": None # Not editing formula here for now
        }
        self.signals.append(new_sig)
        self._refresh_list()

    def _remove_signal(self):
        selected = self.tree.selection()
        if selected:
            idx = self.tree.index(selected[0])
            del self.signals[idx]
            self._refresh_list()

    def _save(self):
        self.on_save(self.can_id, self.signals)
        self.destroy()
