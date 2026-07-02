import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import serial
import serial.tools.list_ports
import struct
import json
import os
import math
import re

def get_param_config(module_type, param_name, param_idx):
    p_lower = param_name.lower()
    m_type = module_type.split("<")[0].strip() # Clean MatrixRouter<3,2>
    
    if m_type == "Biquad":
        if param_idx == 0:
            return {"type": "cb", "values": ["0: LowPass", "1: HighPass", "2: BandPass", "3: Peaking EQ", "4: LowShelf", "5: HighShelf"]}
        elif param_idx == 1:
            return {"type": "knob", "min": 20.0, "max": 20000.0, "step": 1.0, "log": True, "unit": "Hz"}
        elif param_idx == 2:
            return {"type": "knob", "min": -24.0, "max": 24.0, "step": 0.1, "log": False, "unit": "dB"}
        elif param_idx == 3:
            return {"type": "knob", "min": 0.05, "max": 20.0, "step": 0.05, "log": False, "unit": "Q"}
        elif param_idx == 100:
            return {"type": "bypass"}
            
    elif m_type == "Dynamics":
        if param_idx == 0:
            return {"type": "cb", "values": ["0: Compressor", "1: Limiter", "2: Expander", "3: Gate"]}
        elif param_idx == 1:
            return {"type": "knob", "min": -80.0, "max": 0.0, "step": 0.1, "log": False, "unit": "dB"}
        elif param_idx == 2:
            return {"type": "knob", "min": 1.0, "max": 20.0, "step": 0.1, "log": False, "unit": "Ratio"}
        elif param_idx == 3:
            return {"type": "knob", "min": 0.1, "max": 1000.0, "step": 0.1, "log": False, "unit": "ms"}
        elif param_idx == 4:
            return {"type": "knob", "min": 0.0, "max": 1000.0, "step": 1.0, "log": False, "unit": "ms"}
        elif param_idx == 5:
            return {"type": "knob", "min": 1.0, "max": 5000.0, "step": 1.0, "log": False, "unit": "ms"}
        elif param_idx == 6:
            return {"type": "knob", "min": -24.0, "max": 24.0, "step": 0.1, "log": False, "unit": "dB"}
        elif param_idx == 7:
            return {"type": "cb", "values": ["0: Bypass", "1: HighPass", "2: LowPass", "3: BandPass"]}
        elif param_idx == 8:
            return {"type": "knob", "min": 20.0, "max": 20000.0, "step": 1.0, "log": True, "unit": "Hz"}
        elif param_idx == 100:
            return {"type": "bypass"}
            
    elif m_type == "Mixer" or m_type == "Splitter":
        if param_idx >= 100:
            return {"type": "mute"}
        else:
            return {"type": "knob", "min": -80.0, "max": 12.0, "step": 0.1, "log": False, "unit": "dB"}
            
    elif m_type == "Gain":
        if param_idx == 0:
            return {"type": "knob", "min": -80.0, "max": 12.0, "step": 0.1, "log": False, "unit": "dB"}
        elif param_idx == 1:
            return {"type": "mute"}
        elif param_idx == 2:
            return {"type": "invert"}
            
    elif m_type == "FIR":
        if param_idx == 3:
            return {"type": "knob", "min": 16.0, "max": 512.0, "step": 16.0, "log": False, "unit": "Taps"}
        elif param_idx == 4:
            return {"type": "knob", "min": -24.0, "max": 24.0, "step": 0.1, "log": False, "unit": "dB"}
        elif param_idx == 100:
            return {"type": "bypass"}

    elif m_type == "Meter":
        if param_idx == 0:
            return {"type": "meter", "min": -80.0, "max": 6.0, "unit": "dBFS"}
        elif param_idx == 1:
            return {"type": "knob", "min": 0.5, "max": 0.999, "step": 0.001, "log": False, "unit": "decay"}

    # Fallbacks
    if "bypass" in p_lower:
        return {"type": "bypass"}
    elif "mute" in p_lower:
        return {"type": "mute"}
    elif "invert" in p_lower:
        return {"type": "invert"}
    elif "freq" in p_lower:
        return {"type": "knob", "min": 20.0, "max": 20000.0, "step": 1.0, "log": True, "unit": "Hz"}
    elif "gain" in p_lower or "db" in p_lower:
        return {"type": "knob", "min": -40.0, "max": 20.0, "step": 0.1, "log": False, "unit": "dB"}
    elif "ratio" in p_lower:
        return {"type": "knob", "min": 1.0, "max": 20.0, "step": 0.1, "log": False, "unit": "Ratio"}
    elif "attack" in p_lower or "release" in p_lower or "hold" in p_lower:
        return {"type": "knob", "min": 0.0, "max": 1000.0, "step": 1.0, "log": False, "unit": "ms"}
    elif "q-factor" in p_lower or "q" in p_lower:
        return {"type": "knob", "min": 0.1, "max": 10.0, "step": 0.05, "log": False, "unit": "Q"}
    elif "taps" in p_lower:
        return {"type": "knob", "min": 16.0, "max": 512.0, "step": 16.0, "log": False, "unit": "Taps"}
    elif "(lin)" in p_lower or "route" in p_lower:
        return {"type": "knob", "min": 0.0, "max": 1.5, "step": 0.01, "log": False, "unit": "Lin"}
        
    return {"type": "knob", "min": 0.0, "max": 100.0, "step": 1.0, "log": False, "unit": ""}

class Knob(tk.Canvas):
    def __init__(self, parent, min_val=0.0, max_val=100.0, command=None, log_scale=False, *args, **kwargs):
        kwargs['width'] = kwargs.get('width', 60)
        kwargs['height'] = kwargs.get('height', 60)
        kwargs['bg'] = kwargs.get('bg', '#f0f0f0')
        kwargs['highlightthickness'] = 0
        super().__init__(parent, *args, **kwargs)
        self.min_val = min_val
        self.max_val = max_val
        self.value = min_val
        self.command = command
        self.log_scale = log_scale
        self.radius = min(kwargs['width'], kwargs['height']) / 2 - 5
        self.cx = kwargs['width'] / 2
        self.cy = kwargs['height'] / 2
        self.bind("<B1-Motion>", self.on_drag)
        self.bind("<Button-1>", self.on_click)
        self.bind("<MouseWheel>", self.on_scroll)
        self.draw()
        
    def set(self, value):
        self.value = max(self.min_val, min(self.max_val, float(value)))
        self.draw()
        
    def draw(self):
        self.delete("all")
        self.create_arc(5, 5, self.winfo_reqwidth()-5, self.winfo_reqheight()-5, start=-45, extent=270, style=tk.ARC, outline="#cccccc", width=4)
        
        if self.log_scale and self.value > 0 and self.min_val > 0:
            log_min, log_max = math.log10(self.min_val), math.log10(self.max_val)
            pct = (math.log10(self.value) - log_min) / (log_max - log_min) if log_max > log_min else 0
        else:
            pct = (self.value - self.min_val) / (self.max_val - self.min_val) if self.max_val > self.min_val else 0
            
        angle = 225 - (pct * 270)
        self.create_arc(5, 5, self.winfo_reqwidth()-5, self.winfo_reqheight()-5, start=angle, extent=225-angle, style=tk.ARC, outline="#007ACC", width=4)
        rad = math.radians(angle)
        ex = self.cx + self.radius * math.cos(rad)
        ey = self.cy - self.radius * math.sin(rad)
        self.create_line(self.cx, self.cy, ex, ey, fill="#007ACC", width=3)
        
        # Format text
        if self.value >= 1000:
            txt = f"{self.value/1000:.1f}k"
        else:
            txt = f"{self.value:.1f}"
        self.create_text(self.cx, self.cy + 15, text=txt, font=("Arial", 8))
        
    def _update_from_angle(self, event):
        dx = event.x - self.cx
        dy = self.cy - event.y 
        angle = math.degrees(math.atan2(dy, dx))
        if angle < -90: angle += 360
        if angle < -45: angle = -45
        if angle > 225: angle = 225
        pct = (225 - angle) / 270.0
        
        if self.log_scale and self.min_val > 0:
            log_min, log_max = math.log10(self.min_val), math.log10(self.max_val)
            log_val = log_min + pct * (log_max - log_min)
            val = 10 ** log_val
        else:
            val = self.min_val + pct * (self.max_val - self.min_val)
            
        self.set(val)
        if self.command: self.command(self.value)
            
    def on_click(self, event): self._update_from_angle(event)
    def on_drag(self, event): self._update_from_angle(event)
    def on_scroll(self, event):
        if self.log_scale and self.value > 0 and self.min_val > 0:
            log_min, log_max = math.log10(self.min_val), math.log10(self.max_val)
            step = (log_max - log_min) / 50.0
            log_val = math.log10(self.value)
            log_val = log_val + step if event.delta > 0 else log_val - step
            new_val = 10 ** log_val
        else:
            step = (self.max_val - self.min_val) / 50.0
            new_val = self.value + step if event.delta > 0 else self.value - step
            
        self.set(new_val)
        if self.command: self.command(self.value)

class RadStudio:
    def __init__(self, root):
        self.root = root
        self.root.title("RadStudio Lite (Serial DSP Controller)")
        self.root.geometry("800x850")
        
        # Konfigurasi Font Tabel (Treeview)
        style = ttk.Style()
        style.configure("Treeview", font=("Arial", 10), rowheight=25)
        style.configure("Treeview.Heading", font=("Arial", 10, "bold"))
        
        self.serial_port = None
        self.module_map = {}
        self.module_list_keys = []
        self.load_module_config()
        
        # UI Elements
        self.create_widgets()
        
    def load_module_config(self):
        # Hapus file json statis. Sekarang kita murni plug & play!
        self.module_map = {}
        self.routing_edges = []
        self.schema_fetched = False
            
    def create_widgets(self):
        # Frame Connection
        frame_conn = tk.LabelFrame(self.root, text="Connection", padx=10, pady=5)
        frame_conn.pack(fill="x", padx=10, pady=5)
        
        tk.Label(frame_conn, text="Port:").grid(row=0, column=0, sticky="w")
        self.cb_port = ttk.Combobox(frame_conn, values=[port.device for port in serial.tools.list_ports.comports()], width=15)
        self.cb_port.grid(row=0, column=1, padx=5)
        if self.cb_port['values']:
            self.cb_port.current(0)
            
        tk.Label(frame_conn, text="Baud:").grid(row=0, column=2, sticky="w", padx=(10, 0))
        self.cb_baud = ttk.Combobox(frame_conn, values=["9600", "115200", "921600"], width=10)
        self.cb_baud.grid(row=0, column=3, padx=5)
        self.cb_baud.current(1)
        
        self.btn_connect = tk.Button(frame_conn, text="Connect", command=self.toggle_connection)
        self.btn_connect.grid(row=0, column=4, padx=5)
        
        # Frame DAG Visualization
        frame_dag = tk.LabelFrame(self.root, text="DSP Routing Graph", padx=5, pady=5)
        frame_dag.pack(fill="x", padx=10, pady=5)
        self.btn_view_graph = tk.Button(frame_dag, text="👁️ View Routing Graph", command=self.show_routing_window, state="disabled", bg="#E1F0FF")
        self.btn_view_graph.pack(fill="x", pady=5)
        
        # Frame DSP Parameters
        frame_dsp = tk.LabelFrame(self.root, text="DSP Parameters", padx=10, pady=10)
        frame_dsp.pack(fill="x", padx=10, pady=5)
        
        tk.Label(frame_dsp, text="Module Target:").grid(row=0, column=0, sticky="w", pady=5)
        
        # self.module_map = {"1": {"name": "eqBand1", "type": "Biquad", "params": [...]}}
        self.module_list_keys = list(self.module_map.keys())
        display_values = []
        for k in self.module_list_keys:
            m = self.module_map[k]
            # m can be dict (new schema) or string (old schema)
            if isinstance(m, dict):
                display_values.append(f"ID {k} - {m.get('name', 'Unknown')} ({m.get('type', '')})")
            else:
                display_values.append(f"ID {k} - {m}")
        
        self.cb_module = ttk.Combobox(frame_dsp, values=display_values, width=35)
        self.cb_module.grid(row=0, column=1, sticky="w")
        self.cb_module.bind("<<ComboboxSelected>>", self.on_module_changed)
        
        tk.Label(frame_dsp, text="Parameter:").grid(row=1, column=0, sticky="w", pady=5)
        self.cb_param = ttk.Combobox(frame_dsp, values=[], width=25)
        self.cb_param.grid(row=1, column=1, sticky="w")
        
        if display_values:
            self.cb_module.current(0)
            self.on_module_changed(None)
        
        tk.Label(frame_dsp, text="Value (Float):").grid(row=2, column=0, sticky="w", pady=5)
        self.ent_value = tk.Entry(frame_dsp, width=15)
        self.ent_value.grid(row=2, column=1, sticky="w")
        self.ent_value.insert(0, "1000.0")
        
        self.btn_send = tk.Button(frame_dsp, text="SEND COMMAND", command=self.send_command, state="disabled", bg="lightblue")
        self.btn_send.grid(row=3, column=0, columnspan=2, pady=15, sticky="ew")
        
        # --- PACK BOTTOM FRAMES FIRST ---
        # Frame Status
        self.lbl_status = tk.Label(self.root, text="Status: Disconnected", fg="red")
        self.lbl_status.pack(side="bottom", anchor="w", padx=10, pady=5)

        # Frame System Telemetry
        frame_sys = tk.LabelFrame(self.root, text="System Health (ESP32)", padx=10, pady=10)
        frame_sys.pack(side="bottom", fill="x", padx=10, pady=5)
        
        tk.Label(frame_sys, text="DSP Load (Core 0):").grid(row=0, column=0, sticky="w")
        self.pb_dsp0 = ttk.Progressbar(frame_sys, orient="horizontal", length=200, mode="determinate")
        self.pb_dsp0.grid(row=0, column=1, padx=10, pady=2)
        self.lbl_dsp0 = tk.Label(frame_sys, text="0.0 %")
        self.lbl_dsp0.grid(row=0, column=2, sticky="w")
        
        tk.Label(frame_sys, text="DSP Load (Core 1):").grid(row=1, column=0, sticky="w")
        self.pb_dsp1 = ttk.Progressbar(frame_sys, orient="horizontal", length=200, mode="determinate")
        self.pb_dsp1.grid(row=1, column=1, padx=10, pady=2)
        self.lbl_dsp1 = tk.Label(frame_sys, text="0.0 %")
        self.lbl_dsp1.grid(row=1, column=2, sticky="w")
        
        tk.Label(frame_sys, text="RAM Usage:").grid(row=2, column=0, sticky="w")
        self.pb_ram = ttk.Progressbar(frame_sys, orient="horizontal", length=200, mode="determinate")
        self.pb_ram.grid(row=2, column=1, padx=10, pady=2)
        self.lbl_ram = tk.Label(frame_sys, text="0 KB / 0 KB")
        self.lbl_ram.grid(row=2, column=2, sticky="w")

        # --- PACK EXPANDING FRAME LAST ---
        # Frame Telemetry (Sync Table)
        frame_telemetry = tk.LabelFrame(self.root, text="Telemetry (Live Data)", padx=10, pady=10)
        frame_telemetry.pack(fill="both", expand=True, padx=10, pady=5)
        
        self.btn_sync = tk.Button(frame_telemetry, text="SYNC FROM DSP", command=self.start_sync, state="disabled", bg="lightgreen")
        self.btn_sync.pack(fill="x", pady=(0, 10))
        
        columns = ("ID", "Module Name", "Parameter", "Value")
        self.tree = ttk.Treeview(frame_telemetry, columns=columns, show="headings", height=8)
        self.tree.heading("ID", text="ID")
        self.tree.heading("Module Name", text="Module Name")
        self.tree.heading("Parameter", text="Parameter")
        self.tree.heading("Value", text="Value")
        
        self.tree.column("ID", width=40, anchor="center")
        self.tree.column("Module Name", width=120)
        self.tree.column("Parameter", width=150)
        self.tree.column("Value", width=80, anchor="e")
        
        # Tambahkan Scrollbar
        scrollbar = ttk.Scrollbar(frame_telemetry, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)
        
        self.tree.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        
        # Start periodic telemetry polling
        self.root.after(2000, self.poll_system_telemetry)
        
    def toggle_connection(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            self.btn_connect.config(text="Connect")
            self.btn_send.config(state="disabled")
            self.btn_sync.config(state="disabled")
            self.btn_view_graph.config(state="disabled")
            self.lbl_status.config(text="Status: Disconnected", fg="red")
            self.module_map = {}
            self.routing_edges = []
            self.schema_fetched = False
            self.refresh_module_ui()
            if hasattr(self, 'graph_window') and self.graph_window.winfo_exists():
                self.graph_window.destroy()
        else:
            try:
                self.serial_port = serial.Serial(self.cb_port.get(), int(self.cb_baud.get()), timeout=1)
                self.btn_connect.config(text="Disconnect")
                self.lbl_status.config(text="Status: Connected. Fetching schema...", fg="orange")
                self.root.after(500, self.fetch_schema)
            except Exception as e:
                messagebox.showerror("Connection Error", str(e))
                
    def fetch_schema(self):
        if not self.serial_port or not self.serial_port.is_open: return
        self.serial_port.write(b'{"id":254,"req":0}\n')
        self.root.after(100, self.read_schema)
        
    def read_schema(self):
        if not self.serial_port or not self.serial_port.is_open: return
        try:
            self.serial_port.timeout = 0.5
            response = self.serial_port.readline().decode('ascii').strip()
            if response.startswith("{") and "modules" in response:
                data = json.loads(response)
                self.module_map = data.get("modules", {})
                self.routing_edges = data.get("routing", [])
                self.schema_fetched = True
                
                self.refresh_module_ui()
                
                self.btn_send.config(state="normal")
                self.btn_sync.config(state="normal")
                self.btn_view_graph.config(state="normal")
                self.lbl_status.config(text="Status: Connected & Schema Loaded!", fg="green")
            else:
                self.lbl_status.config(text="Status: Connected (No Schema found)", fg="blue")
        except Exception as e:
            self.lbl_status.config(text="Status: Connected (Failed to parse schema)", fg="red")

    def refresh_module_ui(self):
        self.module_list_keys = list(self.module_map.keys())
        display_values = []
        for k in self.module_list_keys:
            m = self.module_map[k]
            if isinstance(m, dict):
                display_values.append(f"ID {k} - {m.get('name', 'Unknown')} ({m.get('type', '')})")
            else:
                display_values.append(f"ID {k} - {m}")
        
        self.cb_module['values'] = display_values
        if display_values:
            self.cb_module.current(0)
            self.on_module_changed(None)
        else:
            self.cb_module.set('')
            self.cb_param['values'] = []
            self.cb_param.set('')

    def show_routing_window(self):
        if hasattr(self, 'graph_window') and self.graph_window.winfo_exists():
            self.graph_window.lift()
            return
            
        self.graph_window = tk.Toplevel(self.root)
        self.graph_window.title("DSP Routing Graph")
        self.graph_window.geometry("800x400")
        
        # Add scrollbars
        hbar = ttk.Scrollbar(self.graph_window, orient=tk.HORIZONTAL)
        hbar.pack(side=tk.BOTTOM, fill=tk.X)
        vbar = ttk.Scrollbar(self.graph_window, orient=tk.VERTICAL)
        vbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.graph_canvas = tk.Canvas(self.graph_window, bg="#ffffff", xscrollcommand=hbar.set, yscrollcommand=vbar.set)
        self.graph_canvas.pack(fill="both", expand=True)
        
        hbar.config(command=self.graph_canvas.xview)
        vbar.config(command=self.graph_canvas.yview)
        
        # Bind resize event to redraw
        self.graph_canvas.bind("<Configure>", lambda e: self.draw_dag())
        
        # Initial draw
        self.root.after(100, self.draw_dag)

    def draw_dag(self):
        if not hasattr(self, 'graph_canvas') or not self.graph_canvas.winfo_exists(): return
        self.graph_canvas.delete("all")
        if not self.routing_edges: return
        
        # 1. Topological Sort
        nodes = set()
        in_degree = {}
        adj = {}
        
        for u, v in self.routing_edges:
            nodes.add(u)
            nodes.add(v)
            adj.setdefault(u, []).append(v)
            adj.setdefault(v, [])
            in_degree.setdefault(u, 0)
            in_degree[v] = in_degree.get(v, 0) + 1
            
        levels = {}
        queue = [n for n in nodes if in_degree[n] == 0]
        for q in queue: levels[q] = 0
        
        while queue:
            curr = queue.pop(0)
            for neighbor in adj[curr]:
                in_degree[neighbor] -= 1
                levels[neighbor] = max(levels.get(neighbor, 0), levels[curr] + 1)
                if in_degree[neighbor] == 0:
                    queue.append(neighbor)
                    
        # 2. Assign coordinates
        import tkinter.font as tkfont
        fnt = tkfont.Font(family="Arial", size=9, weight="bold")
        
        # Dynamic box width depending on the text
        node_w = {n: max(60, fnt.measure(n) + 20) for n in nodes}
        box_h = 35
        
        max_level = max(levels.values()) if levels else 0
        
        level_nodes = {}
        for n, lvl in levels.items():
            level_nodes.setdefault(lvl, []).append(n)
            
        level_width = {}
        for lvl in range(max_level + 1):
            if lvl in level_nodes:
                level_width[lvl] = max(node_w[n] for n in level_nodes[lvl])
            else:
                level_width[lvl] = 80
                
        # Total available width
        width = self.graph_canvas.winfo_width()
        if width <= 1: width = 800
        
        margin_x = 20
        margin_y = 20
        current_x = margin_x
        current_row = 0
        
        level_row = {}
        level_x = {}
        row_levels = {}
        row_levels[current_row] = []
        
        for lvl in range(max_level + 1):
            if lvl not in level_nodes: continue
            col_w = level_width[lvl]
            # Wrap to next row if column exceeds window width
            if current_x + col_w + margin_x > width and row_levels[current_row]:
                current_row += 1
                current_x = margin_x
                row_levels[current_row] = []
                
            level_row[lvl] = current_row
            level_x[lvl] = current_x + col_w / 2
            row_levels[current_row].append(lvl)
            current_x += col_w + 40  # 40px spacing between level columns
            
        row_heights = {}
        for row_idx, lvls in row_levels.items():
            max_nodes = max(len(level_nodes[lvl]) for lvl in lvls)
            row_heights[row_idx] = max(150, max_nodes * 60 + 40)
            
        row_y_start = {}
        current_y = margin_y
        for row_idx in sorted(row_levels.keys()):
            row_y_start[row_idx] = current_y
            current_y += row_heights[row_idx]
            
        # Update canvas scroll region
        max_needed_w = max(width, current_x + 40)
        self.graph_canvas.config(scrollregion=(0, 0, max_needed_w, current_y + 40))
        
        coords = {}
        for row_idx, lvls in row_levels.items():
            y_start = row_y_start[row_idx]
            row_h = row_heights[row_idx]
            for lvl in lvls:
                cx = level_x[lvl]
                nlist = level_nodes[lvl]
                total_nodes = len(nlist)
                y_spacing = row_h / (total_nodes + 1)
                for i, n in enumerate(nlist):
                    cy = y_start + (i + 1) * y_spacing
                    coords[n] = (cx, cy)
                    
        # 3. Draw Edges with vertical pin offsets and smooth Bezier S-curves
        incoming_map = {}
        outgoing_map = {}
        for u, v in self.routing_edges:
            incoming_map.setdefault(v, []).append(u)
            outgoing_map.setdefault(u, []).append(v)
            
        for u, v in self.routing_edges:
            x1, y1 = coords[u]
            x2, y2 = coords[v]
            
            # Target offset on the left edge of destination node v (inputs)
            u_list = sorted(incoming_map[v], key=lambda node: coords[node][1])
            idx_t = u_list.index(u)
            K_t = len(u_list)
            offset_t = -10 + (idx_t * 20 / (K_t - 1)) if K_t > 1 else 0
            
            # Source offset on the right edge of source node u (outputs)
            v_list = sorted(outgoing_map[u], key=lambda node: coords[node][1])
            idx_s = v_list.index(v)
            K_s = len(v_list)
            offset_s = -10 + (idx_s * 20 / (K_s - 1)) if K_s > 1 else 0
            
            xs = x1 + node_w[u]/2
            ys = y1 + offset_s
            xt = x2 - node_w[v]/2
            yt = y2 + offset_t
            
            # Dynamic path drawing depending on layout flow direction
            if xt > xs + 40:
                # Normal left-to-right Bezier line
                x_mid = xs + (xt - xs) / 2
                self.graph_canvas.create_line(xs, ys, x_mid, ys, x_mid, yt, xt, yt, arrow=tk.LAST, fill="#007ACC", width=2, smooth=True)
            else:
                # Wrap-around line path: exits right, goes down/up, enters left
                y_mid = (ys + yt) / 2
                self.graph_canvas.create_line(xs, ys, xs + 20, ys, xs + 20, y_mid, xt - 20, y_mid, xt - 20, yt, xt, yt, arrow=tk.LAST, fill="#007ACC", width=2, smooth=True)
                
        # 4. Draw Nodes
        for n, (cx, cy) in coords.items():
            w = node_w[n]
            box = self.graph_canvas.create_rectangle(cx-w/2, cy-box_h/2, cx+w/2, cy+box_h/2, fill="#E1F0FF", outline="#007ACC", width=2, tags=("node", f"name_{n}"))
            txt = self.graph_canvas.create_text(cx, cy, text=n, font=("Arial", 9, "bold"), fill="#333333", tags=("node", f"name_{n}"))
            
            # Bind Double Click Event to the box and text
            self.graph_canvas.tag_bind(box, "<Double-Button-1>", lambda e, node_name=n: self.open_node_popup(node_name))
            self.graph_canvas.tag_bind(txt, "<Double-Button-1>", lambda e, node_name=n: self.open_node_popup(node_name))
            
    def open_node_popup(self, node_name):
        # Cari ID dari node_name di module_map
        target_id = None
        target_mod = None
        for k, v in self.module_map.items():
            if isinstance(v, dict) and v.get("name") == node_name:
                target_id = k
                target_mod = v
                break
                
        if not target_id:
            # It means it's not an effect, but an abstract node like I2S0_In or Split
            return
            
        popup = tk.Toplevel(self.graph_window)
        popup.title(f"Parameter: {node_name}")
        popup.geometry("450x400")
        popup.attributes("-topmost", True)
        
        tk.Label(popup, text=f"{node_name} ({target_mod.get('type')})", font=("Arial", 12, "bold")).pack(pady=10)
        
        frame_params = tk.Frame(popup)
        frame_params.pack(fill="both", expand=True, padx=10)
        
        # Sync Current Values
        current_vals = {}
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.timeout = 0.1
            for p_name in target_mod.get("params", []):
                try:
                    real_id = int(p_name.split(":")[0].strip())
                except:
                    continue
                self.serial_port.write(f'{{"id":{target_id},"req":{real_id}}}\n'.encode('ascii'))
                try:
                    for _ in range(5):
                        resp = self.serial_port.readline().decode('ascii').strip()
                        if resp.startswith("{") and "ack" in resp:
                            data = json.loads(resp)
                            if data.get("id") == int(target_id) and data.get("p") == real_id:
                                current_vals[real_id] = data.get("v", 0.0)
                                break
                except:
                    pass
        
        entries = []
        ui_elements = {}
        # For Live Update so it doesn't flood the serial, we create a send function
        def send_live(p_idx, val):
            if self.serial_port and self.serial_port.is_open:
                cmd = f'{{"id":{target_id},"p":{p_idx},"v":{val}}}\n'
                self.serial_port.write(cmd.encode('ascii'))
                
        def update_dynamics_ui():
            if target_mod.get("type") != "Dynamics": return
            # Get Dynamics Type value (index 0)
            dyn_type = 0
            if 0 in ui_elements and 'cb' in ui_elements[0]:
                dyn_type = ui_elements[0]['cb'].current()
                
            # If Limiter (1), Ratio becomes Ceiling
            if 2 in ui_elements:
                ui_elements[2]['label'].config(text="2: Ceiling" if dyn_type == 1 else "2: Ratio")
                
            # SC Filter Type & Freq (Index 7 & 8)
            state_cb = "disabled" if dyn_type == 1 else "readonly"
            state_ent = "disabled" if dyn_type == 1 else "normal"
            if 7 in ui_elements:
                ui_elements[7]['cb'].config(state=state_cb)
            if 8 in ui_elements:
                ui_elements[8]['ent'].config(state=state_ent)
                
        is_matrix = target_mod.get("type", "").startswith("MatrixRouter")
        if is_matrix:
            import re
            m = re.search(r'<(\d+)\s*,\s*(\d+)>', target_mod.get("type", ""))
            num_in, num_out = (int(m.group(1)), int(m.group(2))) if m else (3, 2)
            tk.Label(frame_params, text="Matrix Router Routing (Linear Gain)", font=("Arial", 10, "bold")).grid(row=0, column=0, columnspan=num_out+1, pady=10)
            src_names = [f"IN {i}" for i in range(num_in)]
            dst_names = [f"OUT {j}" for j in range(num_out)]
            for p_str in target_mod.get("params", []):
                try:
                    p_id = int(p_str.split(":")[0])
                    p_name = p_str.split(":")[1].replace("(Lin)", "").strip()
                    parts = p_name.split("->")
                    if len(parts) == 2:
                        in_idx, out_idx = p_id // num_out, p_id % num_out
                        if in_idx < num_in: src_names[in_idx] = parts[0].strip()
                        if out_idx < num_out: dst_names[out_idx] = parts[1].strip()
                except: pass
            for j in range(num_out):
                tk.Label(frame_params, text=dst_names[j], font=("Arial", 9, "bold")).grid(row=1, column=j+1, padx=5)
            for i in range(num_in):
                tk.Label(frame_params, text=src_names[i], font=("Arial", 9, "bold")).grid(row=i+2, column=0, padx=5, sticky="e")
                for j in range(num_out):
                    param_idx = i * num_out + j
                    ent = tk.Entry(frame_params, width=6)
                    ent.grid(row=i+2, column=j+1, padx=5, pady=5)
                    if param_idx in current_vals: ent.insert(0, str(current_vals[param_idx]))
                    else: ent.insert(0, "0.0")
                    def make_handler(idx, e):
                        return lambda event: send_live(idx, float(e.get()))
                    ent.bind("<Return>", make_handler(param_idx, ent))
                    if param_idx not in ui_elements: ui_elements[param_idx] = {}
                    ui_elements[param_idx]["ent"] = ent
                    entries.append((param_idx, ent))
        else:
            for i, param_name in enumerate(target_mod.get("params", [])):
                real_p_idx = i
                try:
                    if ":" in param_name:
                        real_p_idx = int(param_name.split(":")[0].strip())
                except: pass
            
                lbl = tk.Label(frame_params, text=param_name, anchor="w", width=15)
                lbl.grid(row=i, column=0, sticky="w", pady=5)
                ui_elements[real_p_idx] = {'label': lbl}
                
                config = get_param_config(target_mod.get("type", ""), param_name, real_p_idx)
                p_type = config.get("type", "knob")
            
                # Handle Checkbox Types (bypass, mute, invert)
                if p_type in ["bypass", "mute", "invert"]:
                    var_val = tk.IntVar(value=0)
                    if real_p_idx in current_vals:
                        var_val.set(int(current_vals[real_p_idx]))
                    
                    def checkbox_changed(p_idx=real_p_idx, var=var_val):
                        send_live(p_idx, float(var.get()))
                    
                    btn_text = "BYPASS" if p_type == "bypass" else ("MUTE" if p_type == "mute" else "INVERT")
                    btn = tk.Checkbutton(frame_params, text=btn_text, variable=var_val, command=checkbox_changed)
                    btn.grid(row=i, column=1, padx=10, sticky="w")
                    ui_elements[real_p_idx]['btn'] = btn
                    ui_elements[real_p_idx]['var'] = var_val
                
                    # Mock entry for Set All functionality
                    mock_ent = tk.Entry(popup)
                    mock_ent.insert(0, str(var_val.get()))
                    entries.append((real_p_idx, mock_ent))
                    continue
            
                if p_type == "cb":
                    cb_values = config["values"]
                    cb = ttk.Combobox(frame_params, values=cb_values, state="readonly", width=15)
                    cb.grid(row=i, column=1, columnspan=2, sticky="w", padx=10, pady=5)
                
                    # Set initial value if available
                    if real_p_idx in current_vals:
                        idx = int(current_vals[real_p_idx])
                        if 0 <= idx < len(cb_values):
                            cb.current(idx)
                        
                    def type_changed(event, p_idx=real_p_idx, cb_ref=cb):
                        val = cb_ref.current()
                        send_live(p_idx, float(val))
                        if target_mod.get("type") == "Dynamics":
                            update_dynamics_ui()
                    
                    cb.bind("<<ComboboxSelected>>", type_changed)
                    ui_elements[real_p_idx]['cb'] = cb
                
                    # Mock entry
                    mock_ent = tk.Entry(popup)
                    mock_ent.insert(0, str(cb.current() if cb.current() >= 0 else 0))
                    entries.append((real_p_idx, mock_ent))
                    continue

                if p_type == "meter":
                    # Canvas VU Meter Premium
                    meter_canvas = tk.Canvas(frame_params, width=220, height=22, bg="#111111", highlightthickness=0)
                    meter_canvas.grid(row=i, column=1, columnspan=2, sticky="w", padx=10, pady=5)
                    
                    # Buat visual segment background
                    meter_canvas.create_rectangle(0, 0, 220, 22, fill="#1c1c1c")
                    meter_bar = meter_canvas.create_rectangle(0, 0, 0, 22, fill="#00FF00")
                    lbl_db = tk.Label(frame_params, text="-80.0 dB", font=("Arial", 9, "bold"), fg="#555555")
                    lbl_db.grid(row=i, column=3, padx=5, sticky="w")
                    
                    active_popup = [True]
                    
                    def poll_meter():
                        if not active_popup[0] or not self.serial_port or not self.serial_port.is_open:
                            return
                        
                        # Request parameter 0 (meter level dB)
                        cmd = f'{{"id":{target_id},"req":0}}\n'
                        try:
                            self.serial_port.write(cmd.encode('ascii'))
                            self.serial_port.timeout = 0.04 # Low timeout for responsiveness
                            resp = self.serial_port.readline().decode('ascii', errors='ignore').strip()
                            if resp.startswith("{") and "ack" in resp:
                                data = json.loads(resp)
                                if data.get("id") == int(target_id) and data.get("p") == 0:
                                    val_db = data.get("v", -80.0)
                                    
                                    # Update text label
                                    lbl_db.config(text=f"{val_db:.1f} dB", fg="#00FF00" if val_db < -12 else ("#FFAA00" if val_db < -3 else "#FF0000"))
                                    
                                    # Konversi -80 dB s.d +6 dB ke lebar bar 0 s.d 220px
                                    norm_val = (val_db + 80.0) / 86.0 # 0.0 s.d 1.0
                                    if norm_val < 0.0: norm_val = 0.0
                                    if norm_val > 1.0: norm_val = 1.0
                                    
                                    width = int(norm_val * 220)
                                    
                                    # Ubah warna bar dinamis berdasarkan tingkat kekerasan
                                    color = "#00FF00" # Hijau
                                    if val_db >= -12.0 and val_db < -3.0:
                                        color = "#FFAA00" # Orange/Kuning
                                    elif val_db >= -3.0:
                                        color = "#FF0000" # Merah (Peak)
                                        
                                    meter_canvas.coords(meter_bar, 0, 0, width, 22)
                                    meter_canvas.itemconfig(meter_bar, fill=color)
                        except Exception:
                            pass
                        
                        popup.after(120, poll_meter)
                        
                    # Hentikan timer ketika popup ditutup
                    def on_close():
                        active_popup[0] = False
                        popup.destroy()
                        
                    popup.protocol("WM_DELETE_WINDOW", on_close)
                    poll_meter()
                    
                    # Mock entry
                    mock_ent = tk.Entry(popup)
                    mock_ent.insert(0, "-80.0")
                    entries.append((real_p_idx, mock_ent))
                    continue
            
                # Numeric Knob Parameters
                min_v = config.get("min", 0.0)
                max_v = config.get("max", 100.0)
                log_sc = config.get("log", False)
                step_v = config.get("step", 1.0)
            
                # Create Entry & Knob
                ent = tk.Entry(frame_params, width=8)
                ent.grid(row=i, column=2, padx=10, pady=5)
            
                def knob_changed(val, e=ent, idx=real_p_idx, step=step_v):
                    # Round value to step
                    val = round(val / step) * step
                    e.delete(0, tk.END)
                    e.insert(0, f"{val:.2f}")
                    send_live(idx, val)
                
                knob = Knob(frame_params, min_val=min_v, max_val=max_v, log_scale=log_sc, command=knob_changed)
                knob.grid(row=i, column=1, padx=10)
            
                if real_p_idx in current_vals:
                    knob.set(current_vals[real_p_idx])
                    ent.delete(0, tk.END)
                    ent.insert(0, str(current_vals[real_p_idx]))
            
                ui_elements[real_p_idx]['ent'] = ent
                ui_elements[real_p_idx]['knob'] = knob
                entries.append((real_p_idx, ent))
            
            update_dynamics_ui()
            
            def send_from_popup():
                if not self.serial_port or not self.serial_port.is_open:
                    messagebox.showerror("Error", "Not connected!", parent=popup)
                    return
                for param_idx, ent in entries:
                    val_str = ent.get().strip()
                    if val_str:
                        try:
                            v = float(val_str)
                            cmd = f'{{"id":{target_id},"p":{param_idx},"v":{v}}}\n'
                            self.serial_port.write(cmd.encode('ascii'))
                        except ValueError:
                            pass
                messagebox.showinfo("Success", "Parameters Sent!", parent=popup)
            
            btn_frame = tk.Frame(popup)
            btn_frame.pack(fill="x", pady=10)
            tk.Button(btn_frame, text="Set All (Manual)", command=send_from_popup, bg="#A0E8AF", font=("Arial", 10, "bold")).pack(side="left", padx=10)
        
            if target_mod.get("type") == "FIR":
                def upload_ir():
                    if not self.serial_port or not self.serial_port.is_open:
                        messagebox.showerror("Error", "Not connected!", parent=popup)
                        return
                    filepath = filedialog.askopenfilename(title="Select IR File", filetypes=[("IR Files", "*.txt *.wav"), ("All Files", "*.*")], parent=popup)
                    if not filepath: return
                
                    taps = []
                    try:
                        if filepath.lower().endswith(".txt"):
                            with open(filepath, "r") as f:
                                lines = f.readlines()
                        
                            is_smaart = any("Magnitude (dB)" in line for line in lines[:15])
                        
                            if is_smaart:
                                import numpy as np
                                freqs, mags, phases = [], [], []
                                for line in lines:
                                    parts = line.strip().split('\t')
                                    if len(parts) >= 3:
                                        try:
                                            f_val = float(parts[0])
                                            m_val = float(parts[1])
                                            p_val = float(parts[2])
                                        
                                            freqs.append(f_val)
                                            mags.append(m_val)
                                            phases.append(p_val)
                                        except ValueError:
                                            pass
                                        
                                if len(freqs) > 10:
                                    taps_count = 512
                                    if 3 in ui_elements:
                                        try: taps_count = int(float(ui_elements[3]['ent'].get()))
                                        except: pass
                                    if taps_count < 64: taps_count = 64
                                    if taps_count > 512: taps_count = 512
                                
                                    N = 4096
                                    linear_freqs = np.linspace(0, 24000, N // 2 + 1)
                                    mags_interp = np.interp(linear_freqs, freqs, mags, left=mags[0], right=mags[-1])
                                    phases_interp = np.interp(linear_freqs, freqs, phases, left=phases[0], right=phases[-1])
                                
                                    A = 10 ** (mags_interp / 20.0)
                                    phi = phases_interp * np.pi / 180.0
                                    Z = A * np.exp(1j * phi)
                                
                                    ir = np.fft.irfft(Z)
                                
                                    peak_idx = np.argmax(np.abs(ir))
                                    shift = (taps_count // 2) - peak_idx
                                    ir_rolled = np.roll(ir, shift)
                                
                                    window = np.hanning(taps_count)
                                    final_taps = ir_rolled[:taps_count] * window
                                    taps = final_taps.tolist()
                            else:
                                for line in lines:
                                    try: taps.append(float(line.strip()))
                                    except: pass
                                
                        elif filepath.lower().endswith(".wav"):
                            import wave, struct
                            with wave.open(filepath, 'rb') as w:
                                n_frames = w.getnframes()
                                sample_width = w.getsampwidth()
                                num_channels = w.getnchannels()
                                frames = w.readframes(n_frames)
                            
                                for i in range(n_frames):
                                    offset = i * num_channels * sample_width
                                    if sample_width == 2: # 16-bit
                                        val = struct.unpack_from('<h', frames, offset)[0]
                                        taps.append(val / 32768.0)
                                    elif sample_width == 4: # 32-bit float
                                        val = struct.unpack_from('<f', frames, offset)[0]
                                        taps.append(val)
                    except Exception as e:
                        messagebox.showerror("Parse Error", f"Failed to parse file:\n{e}", parent=popup)
                        return
                    
                    if not taps:
                        messagebox.showerror("Error", "No valid coefficients found in file!", parent=popup)
                        return
                    
                    # For non-smaart files, we truncate to UI length
                    taps_count = 512
                    if 3 in ui_elements:
                        try: taps_count = int(float(ui_elements[3]['ent'].get()))
                        except: pass
                    if len(taps) > taps_count:
                        taps = taps[:taps_count]
                    
                    import time
                    try:
                        # 1. Set total length
                        self.serial_port.write(f'{{"id":{target_id},"p":3,"v":{len(taps)}}}\n'.encode('ascii'))
                        time.sleep(0.05)
                    
                        # 2. Upload taps sequentially
                        for i, val in enumerate(taps):
                            self.serial_port.write(f'{{"id":{target_id},"p":0,"v":{i}}}\n'.encode('ascii'))
                            self.serial_port.write(f'{{"id":{target_id},"p":1,"v":{val:.6f}}}\n'.encode('ascii'))
                            time.sleep(0.005) # Prevent serial overflow
                        
                        # 3. Commit swap
                        self.serial_port.write(f'{{"id":{target_id},"p":2,"v":1.0}}\n'.encode('ascii'))
                        messagebox.showinfo("Success", f"Uploaded {len(taps)} FIR Taps successfully!", parent=popup)
                    except Exception as e:
                        messagebox.showerror("Upload Error", f"Serial failed:\n{e}", parent=popup)

                tk.Button(btn_frame, text="Upload IR (TXT/WAV)", command=upload_ir, bg="#FFD700", font=("Arial", 10, "bold")).pack(side="right", padx=10)
                
    def on_module_changed(self, event):
        idx = self.cb_module.current()
        if idx < 0: return
        key = self.module_list_keys[idx]
        mod_info = self.module_map[key]
        
        if isinstance(mod_info, dict) and "params" in mod_info:
            params = mod_info["params"]
            self.cb_param['values'] = params
        else:
            # Fallback for old schema
            self.cb_param['values'] = ["0: Filter Type", "1: Frequency (Hz)", "2: Gain (dB)", "3: Q-Factor"]
            
        if self.cb_param['values']:
            self.cb_param.current(0)
            
    def send_command(self):
        if not self.serial_port or not self.serial_port.is_open:
            return
            
        try:
            idx = self.cb_module.current()
            if idx < 0: return
            module_id = int(self.module_list_keys[idx])
            
            param_str = self.cb_param.get()
            try:
                param_index = int(param_str.split(":")[0].strip())
            except Exception:
                param_index = self.cb_param.current()
            value = float(self.ent_value.get())
            
            # Format JSON: {"id":1,"p":2,"v":-6.5}
            cmd = f'{{"id":{module_id},"p":{param_index},"v":{value}}}\n'
            self.serial_port.write(cmd.encode('ascii'))
            
            # Read ACK dari ESP32
            self.serial_port.timeout = 1
            response = self.serial_port.readline().decode('ascii').strip()
            self.lbl_status.config(text=f"Sent JSON | RX: {response}", fg="blue")
            
        except ValueError:
            messagebox.showerror("Input Error", "Please ensure Module ID is an integer and Value is a float.")

    def start_sync(self):
        if not self.serial_port or not self.serial_port.is_open: return
        self.sync_queue = []
        for key in self.module_list_keys:
            mod_info = self.module_map[key]
            if isinstance(mod_info, dict) and "params" in mod_info:
                for param_str in mod_info["params"]:
                    try:
                        p_idx = int(param_str.split(":")[0].strip())
                    except Exception:
                        p_idx = 0
                    self.sync_queue.append((int(key), mod_info.get("name", "Unknown"), p_idx, param_str))
        
        # clear treeview
        for item in self.tree.get_children():
            self.tree.delete(item)
            
        self.btn_sync.config(state="disabled")
        self.lbl_status.config(text="Status: Syncing data...", fg="orange")
        self.process_sync_queue()

    def process_sync_queue(self):
        if not hasattr(self, 'sync_queue') or not self.sync_queue:
            self.btn_sync.config(state="normal")
            self.lbl_status.config(text="Status: Sync Complete", fg="green")
            return
            
        module_id, mod_name, p_idx, p_str = self.sync_queue.pop(0)
        
        # Send req
        cmd = f'{{"id":{module_id},"req":{p_idx}}}\n'
        self.serial_port.write(cmd.encode('ascii'))
        
        # Read response
        try:
            self.serial_port.timeout = 0.2
            response = self.serial_port.readline().decode('ascii').strip()
            if response.startswith("{") and "ack" in response:
                data = json.loads(response)
                val = data.get("v", 0.0)
                self.tree.insert("", "end", values=(module_id, mod_name, p_str, val))
        except Exception as e:
            pass
            
        # Schedule next iteration (prevents GUI freezing)
        self.root.after(30, self.process_sync_queue)

    def poll_system_telemetry(self):
        if self.serial_port and self.serial_port.is_open:
            try:
                # Minta data dari ID 255 (System)
                self.serial_port.write(b'{"id":255,"req":0}\n')
                self.serial_port.timeout = 0.1
                lines = self.serial_port.readlines()
                for line in lines:
                    response = line.decode('ascii', errors='ignore').strip()
                    if response.startswith("{") and "sys" in response:
                        data = json.loads(response)
                        dsp_load0 = data.get("c0", 0.0)
                        dsp_load1 = data.get("c1", 0.0)
                        ram_f = data.get("ramF", 0)
                        ram_t = data.get("ramT", 0)
                        ram_used = ram_t - ram_f
                        
                        self.pb_dsp0['value'] = min(dsp_load0, 100.0)
                        self.lbl_dsp0.config(text=f"{dsp_load0:.1f} %")
                        
                        self.pb_dsp1['value'] = min(dsp_load1, 100.0)
                        self.lbl_dsp1.config(text=f"{dsp_load1:.1f} %")
                        
                        if ram_t > 0:
                            ram_percent = (ram_used / ram_t) * 100.0
                            self.pb_ram['value'] = ram_percent
                            
                            # Konversi ke format KB/MB yang mudah dibaca
                            used_kb = ram_used / 1024.0
                            free_kb = ram_f / 1024.0
                            total_kb = ram_t / 1024.0
                            
                            text_ram = f"{ram_percent:.1f}% (Used: {used_kb:.1f} KB | Free: {free_kb:.1f} KB | Total: {total_kb:.1f} KB)"
                            self.lbl_ram.config(text=text_ram)
            except Exception:
                pass
                
        self.root.after(2000, self.poll_system_telemetry)

if __name__ == "__main__":
    root = tk.Tk()
    app = RadStudio(root)
    root.mainloop()
