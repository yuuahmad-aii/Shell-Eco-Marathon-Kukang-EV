import sys
import struct
import serial
import serial.tools.list_ports
import csv
from datetime import datetime
import serial
import serial.tools.list_ports
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QPushButton, QComboBox, QLineEdit, 
                             QLabel, QPlainTextEdit, QSpinBox, QDoubleSpinBox, 
                             QCheckBox, QGroupBox, QTabWidget, QShortcut, QMessageBox)
from PyQt5.QtGui import QKeySequence
from PyQt5.QtCore import QThread, pyqtSignal, QTimer, Qt
import pyqtgraph as pg

class SerialReaderThread(QThread):
    # pos, vel, vq, target, ia, ib, ic, mode
    new_data_signal = pyqtSignal(float, float, float, float, float, float, float, int)
    text_data_signal = pyqtSignal(str)
    
    def __init__(self):
        super().__init__()
        self.serial_port = None
        self.is_running = False
        
    def connect_serial(self, port, baudrate=115200):
        try:
            self.serial_port = serial.Serial(port, baudrate, timeout=0.1)
            self.is_running = True
            self.start()
            return True
        except Exception as e:
            print(f"Error connecting: {e}")
            return False
            
    def disconnect_serial(self):
        self.is_running = False
        self.wait() # Wait for thread to finish
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            
    def send_command(self, cmd_str):
        if self.serial_port and self.serial_port.is_open:
            cmd = cmd_str.strip() + "\r\n"
            self.serial_port.write(cmd.encode('utf-8'))
            
    def run(self):
        sync_state = 0
        payload = bytearray()
        
        while self.is_running and self.serial_port and self.serial_port.is_open:
            try:
                waiting = self.serial_port.in_waiting
                if waiting > 0:
                    data = self.serial_port.read(waiting)
                    for b in data:
                        # State machine to find 0xAA 0xBB sync and read 21 bytes
                        if sync_state == 0:
                            if b == 0xAA:
                                sync_state = 1
                            else:
                                if b < 128 and b != 0x00:
                                    try:
                                        char = chr(b)
                                        self.text_data_signal.emit(char)
                                    except:
                                        pass
                        elif sync_state == 1:
                            if b == 0xBB:
                                sync_state = 2
                                payload.clear()
                                payload.append(0xAA)
                                payload.append(0xBB)
                            elif b == 0xAA:
                                sync_state = 1
                            else:
                                sync_state = 0
                        elif sync_state == 2:
                            payload.append(b)
                            if len(payload) == 33:
                                if payload[32] == 0x55: # Footer
                                    # Verify CRC
                                    crc = 0
                                    for i in range(2, 31):
                                        crc ^= payload[i]
                                        
                                    if crc == payload[31]:
                                        # Unpack Little Endian: <fffffffB (7 floats + 1 unsigned byte)
                                        pos, vel, vq, target, ia, ib, ic, mode = struct.unpack('<fffffffB', payload[2:31])
                                        self.new_data_signal.emit(pos, vel, vq, target, ia, ib, ic, mode)
                                sync_state = 0
                else:
                    self.msleep(1)
            except Exception as e:
                print(f"Serial Error: {e}")
                self.is_running = False

class FOCGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        
        self.setWindowTitle("FOC Real-time Tuner")
        self.resize(1200, 900)
        
        # Data Arrays
        self.max_points = 200
        self.pos_data = []
        self.vel_data = []
        self.vq_data = []
        self.target_data = []
        self.ia_data = []
        self.ib_data = []
        self.ic_data = []
        self.active_mode = -1
        
        # Logger Variables
        self.is_logging = False
        self.log_file = None
        self.csv_writer = None
        
        self.setup_ui()
        
        self.serial_thread = SerialReaderThread()
        self.serial_thread.new_data_signal.connect(self.on_new_binary_data)
        self.serial_thread.text_data_signal.connect(self.on_new_text_data)
        
        self.text_buffer = ""
        
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.update_plots)
        self.update_timer.start(50) # 20 FPS GUI update
        
        self.new_data_available = False
        
    def setup_ui(self):
        self.tabs = QTabWidget()
        self.setCentralWidget(self.tabs)
        
        # --- TAB 1: Dashboard ---
        dashboard_widget = QWidget()
        main_layout = QVBoxLayout(dashboard_widget)
        
        # Top Bar: Connection & Config
        top_layout = QHBoxLayout()
        
        conn_box = QGroupBox("Connection")
        conn_inner = QHBoxLayout()
        self.port_combo = QComboBox()
        self.refresh_ports()
        conn_inner.addWidget(QLabel("COM Port:"))
        conn_inner.addWidget(self.port_combo)
        
        self.refresh_btn = QPushButton("Refresh")
        self.refresh_btn.clicked.connect(self.refresh_ports)
        conn_inner.addWidget(self.refresh_btn)
        
        self.connect_btn = QPushButton("Connect")
        self.connect_btn.clicked.connect(self.toggle_connection)
        conn_inner.addWidget(self.connect_btn)
        
        self.log_btn = QPushButton("Start Logging (Ctrl+L)")
        self.log_btn.clicked.connect(self.toggle_logging)
        conn_inner.addWidget(self.log_btn)
        
        conn_box.setLayout(conn_inner)
        top_layout.addWidget(conn_box)
        
        conf_box = QGroupBox("Graph Configuration")
        conf_inner = QHBoxLayout()
        
        conf_inner.addWidget(QLabel("X History (Points):"))
        self.x_pts_spin = QSpinBox()
        self.x_pts_spin.setRange(50, 5000)
        self.x_pts_spin.setValue(200)
        self.x_pts_spin.setSingleStep(50)
        self.x_pts_spin.valueChanged.connect(self.update_config)
        conf_inner.addWidget(self.x_pts_spin)
        
        self.auto_y_chk = QCheckBox("Auto Y-Axis")
        self.auto_y_chk.setChecked(True)
        self.auto_y_chk.stateChanged.connect(self.update_config)
        conf_inner.addWidget(self.auto_y_chk)
        
        conf_inner.addWidget(QLabel("Y Min:"))
        self.y_min_spin = QDoubleSpinBox()
        self.y_min_spin.setRange(-10000, 10000)
        self.y_min_spin.setValue(-20)
        self.y_min_spin.setEnabled(False)
        self.y_min_spin.valueChanged.connect(self.update_config)
        conf_inner.addWidget(self.y_min_spin)
        
        conf_inner.addWidget(QLabel("Y Max:"))
        self.y_max_spin = QDoubleSpinBox()
        self.y_max_spin.setRange(-10000, 10000)
        self.y_max_spin.setValue(20)
        self.y_max_spin.setEnabled(False)
        self.y_max_spin.valueChanged.connect(self.update_config)
        conf_inner.addWidget(self.y_max_spin)
        
        conf_box.setLayout(conf_inner)
        top_layout.addWidget(conf_box)
        main_layout.addLayout(top_layout)
        
        # Graphs
        self.graph_layout = pg.GraphicsLayoutWidget()
        main_layout.addWidget(self.graph_layout, stretch=3)
        
        dash_pen = pg.mkPen('w', style=Qt.DashLine)
        
        # (Position Plot Removed)
        
        # Velocity Plot
        self.p_vel = self.graph_layout.addPlot(title="Velocity (RPM)")
        self.p_vel.showGrid(x=True, y=True)
        self.p_vel.addLegend()
        self.curve_vel_t = self.p_vel.plot(pen=dash_pen, name="Target", antialias=True)
        self.curve_vel = self.p_vel.plot(pen='y', name="Actual", antialias=True)
        
        self.graph_layout.nextRow()
        
        # Torque/Voltage Plot
        self.p_vq = self.graph_layout.addPlot(title="Target vs Actual Duty")
        self.p_vq.showGrid(x=True, y=True)
        self.p_vq.addLegend()
        self.curve_vq_t = self.p_vq.plot(pen=dash_pen, name="Target", antialias=True)
        self.curve_vq = self.p_vq.plot(pen='r', name="Actual", antialias=True)
        
        self.graph_layout.nextRow()
        
        # Phase Currents Plot
        self.p_phase = self.graph_layout.addPlot(title="Phase PWM Duty (U, V, W %)")
        self.p_phase.showGrid(x=True, y=True)
        self.p_phase.addLegend()
        self.curve_ia = self.p_phase.plot(pen=pg.mkPen('r', width=1.5), name="Duty U", antialias=True)
        self.curve_ib = self.p_phase.plot(pen=pg.mkPen('g', width=1.5), name="Duty V", antialias=True)
        self.curve_ic = self.p_phase.plot(pen=pg.mkPen('b', width=1.5), name="Duty W", antialias=True)
        
        # Bottom Bar: Terminal
        self.terminal = QPlainTextEdit()
        self.terminal.setReadOnly(True)
        self.terminal.setMaximumHeight(150)
        main_layout.addWidget(self.terminal)
        
        cmd_layout = QHBoxLayout()
        self.cmd_input = QLineEdit()
        self.cmd_input.setPlaceholderText("Enter command (e.g. s15, $7=0.5, $$)")
        self.cmd_input.returnPressed.connect(self.send_cmd)
        
        self.send_btn = QPushButton("Send")
        self.send_btn.clicked.connect(self.send_cmd)
        
        cmd_layout.addWidget(self.cmd_input)
        cmd_layout.addWidget(self.send_btn)
        main_layout.addLayout(cmd_layout)
        
        self.tabs.addTab(dashboard_widget, "Dashboard")
        
        # --- TAB 2: Help & Info ---
        help_widget = QWidget()
        help_layout = QVBoxLayout(help_widget)
        
        version_label = QLabel("<h2>Software Version</h2><p>Version 1.1 - Hybrid FOC Edition</p>")
        help_layout.addWidget(version_label)
        
        shortcuts_text = QPlainTextEdit()
        shortcuts_text.setReadOnly(True)
        shortcuts_text.setPlainText(
            "--- Keyboard Shortcuts ---\n"
            "Ctrl+L : Start / Stop Data Logging to CSV\n"
            "\n"
            "--- Useful Commands ---\n"
            "$$ : Show all configurations\n"
            "$save : Save settings to flash memory\n"
            "$h : Show help\n"
            "S<val> : Set Target Duty Cycle (%) (Example: S15)\n"
            "T : Stop Motor\n"
            "$8=<val> : Set Switchover RPM (from 6-step to SVPWM)\n"
            "$9=<val> : Set Switchover Delay (in seconds)\n"
        )
        help_layout.addWidget(shortcuts_text)
        
        self.tabs.addTab(help_widget, "Help & Info")
        
        # Setup Shortcuts
        log_shortcut = QShortcut(QKeySequence("Ctrl+L"), self)
        log_shortcut.activated.connect(self.toggle_logging)
        
    def toggle_logging(self):
        if not self.is_logging:
            # Start logging
            filename = datetime.now().strftime("motor_log_%Y%m%d_%H%M%S.csv")
            try:
                self.log_file = open(filename, 'w', newline='')
                self.csv_writer = csv.writer(self.log_file)
                self.csv_writer.writerow(["Timestamp", "Angle (rad)", "Velocity (RPM)", "Duty Target (%)", "Duty U (%)", "Duty V (%)", "Duty W (%)", "Mode"])
                self.is_logging = True
                self.log_btn.setText("Stop Logging (Ctrl+L)")
                self.log_btn.setStyleSheet("background-color: red; color: white; font-weight: bold;")
                self.terminal.appendPlainText(f"> Started logging to {filename}")
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Could not open file for logging: {e}")
        else:
            # Stop logging
            self.is_logging = False
            if self.log_file:
                self.log_file.close()
                self.log_file = None
            self.log_btn.setText("Start Logging (Ctrl+L)")
            self.log_btn.setStyleSheet("")
            self.terminal.appendPlainText("> Stopped logging")

    def refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for p in ports:
            self.port_combo.addItem(p.device)
            
    def toggle_connection(self):
        if self.serial_thread.is_running:
            self.serial_thread.disconnect_serial()
            self.connect_btn.setText("Connect")
            self.port_combo.setEnabled(True)
            self.refresh_btn.setEnabled(True)
        else:
            port = self.port_combo.currentText()
            if port:
                if self.serial_thread.connect_serial(port):
                    self.connect_btn.setText("Disconnect")
                    self.port_combo.setEnabled(False)
                    self.refresh_btn.setEnabled(False)
                    self.pos_data = []
                    self.vel_data = []
                    self.vq_data = []
                    self.target_data = []
                    self.ia_data = []
                    self.ib_data = []
                    self.ic_data = []
                    
    def update_config(self):
        self.max_points = self.x_pts_spin.value()
        
        auto = self.auto_y_chk.isChecked()
        self.y_min_spin.setEnabled(not auto)
        self.y_max_spin.setEnabled(not auto)
        
        if auto:
            self.p_vel.enableAutoRange(axis=pg.ViewBox.YAxis)
            self.p_vq.enableAutoRange(axis=pg.ViewBox.YAxis)
            self.p_phase.enableAutoRange(axis=pg.ViewBox.YAxis)
        else:
            ymin = self.y_min_spin.value()
            ymax = self.y_max_spin.value()
            self.p_vel.setYRange(ymin, ymax, padding=0)
            self.p_vq.setYRange(ymin, ymax, padding=0)
            self.p_phase.setYRange(ymin, ymax, padding=0)
            
    def on_new_binary_data(self, pos, vel, vq, target, ia, ib, ic, mode):
        import math
        if math.isnan(ia):
            print("NaN detected in ia!")
        
        self.pos_data.append(pos)
        self.vel_data.append(vel)
        self.vq_data.append(vq)
        self.target_data.append(target)
        self.ia_data.append(ia)
        self.ib_data.append(ib)
        self.ic_data.append(ic)
        self.active_mode = mode
        
        if self.is_logging and self.csv_writer:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            self.csv_writer.writerow([timestamp, pos, vel, target, ia, ib, ic, mode])
            
        if len(self.pos_data) > self.max_points:
            diff = len(self.pos_data) - self.max_points
            self.pos_data = self.pos_data[diff:]
            self.vel_data = self.vel_data[diff:]
            self.vq_data = self.vq_data[diff:]
            self.target_data = self.target_data[diff:]
            self.ia_data = self.ia_data[diff:]
            self.ib_data = self.ib_data[diff:]
            self.ic_data = self.ic_data[diff:]
            
        self.new_data_available = True
        
    def on_new_text_data(self, char):
        self.text_buffer += char
        if char == '\n':
            self.terminal.appendPlainText(self.text_buffer.strip())
            scrollbar = self.terminal.verticalScrollBar()
            scrollbar.setValue(scrollbar.maximum())
            self.text_buffer = ""
            
    def update_plots(self):
        if self.new_data_available:
            self.curve_vel.setData(self.vel_data)
            self.curve_vq.setData(self.vq_data)
            self.curve_ia.setData(self.ia_data)
            self.curve_ib.setData(self.ib_data)
            self.curve_ic.setData(self.ic_data)
            
            # Always show target on Vq for duty target
            empty = []
            self.curve_vq_t.setData(self.target_data)
            self.curve_vel_t.setData(empty)
                
            self.new_data_available = False
            
    def send_cmd(self):
        cmd = self.cmd_input.text()
        if cmd:
            self.serial_thread.send_command(cmd)
            self.terminal.appendPlainText(f"> {cmd}")
            self.cmd_input.clear()
            
    def closeEvent(self, event):
        if self.is_logging and self.log_file:
            self.log_file.close()
        self.serial_thread.disconnect_serial()
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    
    # Set dark theme for pyqtgraph
    pg.setConfigOption('background', 'k')
    pg.setConfigOption('foreground', 'd')
    
    window = FOCGUI()
    window.show()
    sys.exit(app.exec_())
