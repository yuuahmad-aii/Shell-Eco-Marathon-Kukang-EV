<template>
  <div class="dashboard-container">
    <header class="header">
      <div class="logo">
        <div class="pulse-dot" :class="{ 'connected': isConnected }"></div>
        <h1>🏎️ Kukang EV <span>Telemetry Analysis</span></h1>
      </div>
      
      <div class="header-controls">
        <button 
          class="file-upload-btn" 
          @click="toggleRecording"
          :style="{ backgroundColor: isRecording ? 'var(--warning)' : 'var(--accent-primary)', color: isRecording ? '#000' : '#fff' }"
        >
          {{ isRecording ? '⏹ Stop & Save CSV' : '⏺ Record Data' }}
        </button>
        <label class="file-upload-btn">
          Upload Log (.bin / .csv)
          <input type="file" accept=".bin,.csv" @change="handleFileUpload" hidden />
        </label>
        <label class="file-upload-btn" style="background-color: var(--secondary); color: #fff;" v-if="activeTab === 'vehicle'">
          Convert .bin to .csv
          <input type="file" accept=".bin" @change="convertBinToCsv" hidden />
        </label>
        <div class="status" :class="isConnected ? 'status-connected' : 'status-disconnected'" v-if="activeTab === 'vehicle'">
          {{ isConnected ? 'Live Connection' : (isOfflineMode ? 'Offline Log Mode' : 'Connecting...') }}
        </div>
        
        <button 
          class="file-upload-btn" 
          @click="toggleSerialConnection"
          :style="{ backgroundColor: isSerialConnected ? 'var(--warning)' : 'var(--accent-primary)', color: isSerialConnected ? '#000' : '#fff' }"
          v-if="activeTab === 'motor'"
        >
          {{ isSerialConnected ? 'Disconnect USB' : 'Connect USB Motor' }}
        </button>
        <div class="status" :class="isSerialConnected ? 'status-connected' : 'status-disconnected'" v-if="activeTab === 'motor'">
          {{ isSerialConnected ? 'USB Serial Connected' : 'Disconnected' }}
        </div>
        
        <button class="file-upload-btn" @click="isSidebarOpen = true" style="background-color: var(--accent-primary); font-size: 14px; padding: 6px 12px; margin-left: 10px;">
          ☰ Menu
        </button>
      </div>
    </header>

    <!-- Sidebar Overlay -->
    <div v-if="isSidebarOpen" class="sidebar-overlay" @click="isSidebarOpen = false"></div>
    
    <!-- Sidebar -->
    <div class="sidebar" :class="{ 'sidebar-open': isSidebarOpen }">
      <div class="sidebar-header">
        <h2>Menu</h2>
        <button class="close-btn" @click="isSidebarOpen = false">✖</button>
      </div>
      <div class="sidebar-content">
        <h3>Telemetry Modes</h3>
        <button class="tab-btn" style="width: 100%; margin-bottom: 10px; text-align: left;" :class="{active: activeTab === 'vehicle'}" @click="activeTab = 'vehicle'; isSidebarOpen = false">🌍 Vehicle Telemetry</button>
        <button class="tab-btn" style="width: 100%; margin-bottom: 30px; text-align: left;" :class="{active: activeTab === 'motor'}" @click="activeTab = 'motor'; isSidebarOpen = false">⚡ Motor Telemetry</button>
        
        <div class="sidebar-info">
          <h3>Web App Info</h3>
          <p><strong>App Name:</strong> Kukang EV Telemetry Dashboard</p>
          <p><strong>Stack:</strong> Vue 3, Vite, ApexCharts, Leaflet, Web Serial API</p>
          <p><strong>Authors:</strong> yuuahmad + Gemini AI</p>
        </div>
      </div>
    </div>

    <!-- VEHICLE TELEMETRY -->
    <main class="dashboard-grid" v-show="activeTab === 'vehicle'">
      <!-- Left Column: Map -->
      <div class="map-column">
        <div class="chart-card map-card">
          <div class="chart-title">GPS Track Map (Terrain)</div>
          <div id="leaflet-map" style="flex: 1; min-height: 400px; border-radius: 8px;"></div>
        </div>
      </div>

      <!-- Right Column: Charts -->
      <div class="charts-column">
        <!-- Accel -->
        <div class="chart-card">
          <div class="chart-title">Acceleration (G)</div>
          <apexchart type="line" height="200" :options="accelOptions" :series="accelSeries"></apexchart>
        </div>

        <!-- Gyro -->
        <div class="chart-card">
          <div class="chart-title">Angular Velocity (Gyro)</div>
          <apexchart type="line" height="200" :options="gyroOptions" :series="gyroSeries"></apexchart>
        </div>

        <!-- Speed -->
        <div class="chart-card">
          <div class="chart-title">Vehicle Speed (km/h)</div>
          <apexchart type="line" height="200" :options="speedOptions" :series="speedSeries"></apexchart>
        </div>

        <!-- Altitude -->
        <div class="chart-card">
          <div class="chart-title">Altitude (Baro vs GPS)</div>
          <apexchart type="line" height="200" :options="altOptions" :series="altSeries"></apexchart>
        </div>

        <!-- GPS Quality -->
        <div class="chart-card">
          <div class="chart-title">GPS Quality (Satellites & PDOP)</div>
          <apexchart type="line" height="200" :options="gpsOptions" :series="gpsSeries"></apexchart>
        </div>
      </div>
    </main>
  </div>
<!-- MOTOR TELEMETRY -->
    <main class="dashboard-grid" style="grid-template-columns: 1fr;" v-show="activeTab === 'motor'">
      <div class="charts-column" style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
        <!-- Q1: Target vs Actual Iq -->
        <div class="chart-card" style="display:flex; flex-direction:row; gap:10px; align-items:center;">
          <div style="flex:1;">
            <div class="chart-title">Target vs Actual Iq (A)</div>
            <apexchart type="line" height="280" :options="motorIqOptions" :series="motorIqSeries"></apexchart>
          </div>
          <div style="width:90px; display:flex; flex-direction:column; gap:5px; font-size:11px; color:#94a3b8;">
             <label>Y Max <input type="number" v-model="iqYMax" class="axis-input"></label>
             <label>Y Min <input type="number" v-model="iqYMin" class="axis-input"></label>
             <label>X Max (s) <input type="number" v-model="iqXMax" class="axis-input"></label>
             <label>X Min (s) <input type="number" v-model="iqXMin" class="axis-input"></label>
          </div>
        </div>

        <!-- Q2: Velocity -->
        <div class="chart-card" style="display:flex; flex-direction:row; gap:10px; align-items:center;">
          <div style="flex:1;">
            <div class="chart-title">Electrical Velocity (RPM)</div>
            <apexchart type="line" height="280" :options="motorVelOptions" :series="motorVelSeries"></apexchart>
          </div>
          <div style="width:90px; display:flex; flex-direction:column; gap:5px; font-size:11px; color:#94a3b8;">
             <label>Y Max <input type="number" v-model="velYMax" class="axis-input"></label>
             <label>Y Min <input type="number" v-model="velYMin" class="axis-input"></label>
             <label>X Max (s) <input type="number" v-model="velXMax" class="axis-input"></label>
             <label>X Min (s) <input type="number" v-model="velXMin" class="axis-input"></label>
          </div>
        </div>

        <!-- Q3: Phase Current -->
        <div class="chart-card" style="display:flex; flex-direction:row; gap:10px; align-items:center;">
          <div style="flex:1;">
            <div class="chart-title">Phase Current (U, V, W Amperes)</div>
            <apexchart type="line" height="280" :options="motorPhaseOptions" :series="motorPhaseSeries"></apexchart>
          </div>
          <div style="width:90px; display:flex; flex-direction:column; gap:5px; font-size:11px; color:#94a3b8;">
             <label>Y Max <input type="number" v-model="phaseYMax" class="axis-input"></label>
             <label>Y Min <input type="number" v-model="phaseYMin" class="axis-input"></label>
             <label>X Max (s) <input type="number" v-model="phaseXMax" class="axis-input"></label>
             <label>X Min (s) <input type="number" v-model="phaseXMin" class="axis-input"></label>
          </div>
        </div>
        
        <!-- Q4: Terminal -->
        <div class="chart-card" style="display:flex; flex-direction:column;">
          <div class="chart-title">Serial Terminal</div>
          <textarea readonly class="terminal-output" ref="terminalOutput" :value="terminalText" style="flex:1; width:100%; height:230px; background:#1e293b; color:#10b981; font-family:monospace; padding:10px; border-radius:5px; border:1px solid #334155; margin-bottom:10px; resize:none;"></textarea>
          <div style="display:flex; gap:10px;">
            <input type="text" v-model="cmdInput" @keyup.enter="sendSerialCommand" style="flex:1; padding:10px; background:#0f172a; color:#fff; border:1px solid #334155; border-radius:5px; font-family:monospace;" placeholder="Type command (e.g. s300, $?) and press Enter...">
            <button class="file-upload-btn" @click="sendSerialCommand">Send</button>
            <button class="file-upload-btn" style="background:#334155" @click="terminalText = ''">Clear</button>
          </div>
        </div>
      </div>
    </main>
</template>

<script setup>
import { ref, shallowRef, onMounted, computed, nextTick } from 'vue'
import { db, ref as dbRef, onValue } from './firebase'
import 'leaflet/dist/leaflet.css'
import L from 'leaflet'

const activeTab = ref('vehicle')
const isSidebarOpen = ref(false)

// Motor Telemetry State
const isSerialConnected = ref(false)
let serialPort = null
let serialReader = null
let serialWriter = null
let keepSerialReading = false

const motorTime = shallowRef([])
const motorVel = shallowRef([])
const motorVq = shallowRef([])
const motorTarget = shallowRef([])
const motorIa = shallowRef([])
const motorIb = shallowRef([])
const motorIc = shallowRef([])

const terminalText = ref('')
const cmdInput = ref('')
const terminalOutput = ref(null)

const isConnected = ref(false)
const isOfflineMode = ref(false)
const isRecording = ref(false)
const recordedData = ref([])
let connectionTimeout = null

// Map instance
let mapInstance = null
let polyline = null
let startMarker = null
let endMarker = null
let hoverMarker = null

// Data arrays (Using shallowRef for extreme performance on huge arrays)
const timeHistory = shallowRef([])
const accelX = shallowRef([]); const accelY = shallowRef([]); const accelZ = shallowRef([])
const gyroX = shallowRef([]); const gyroY = shallowRef([]); const gyroZ = shallowRef([])
const baroAlt = shallowRef([]); const gpsAlt = shallowRef([])
const speedHistory = shallowRef([])
const pdopHistory = shallowRef([]); const satsHistory = shallowRef([])
const liveLats = shallowRef([]); const liveLons = shallowRef([])

// Clear all data
const clearData = () => {
  timeHistory.value = []
  accelX.value = []; accelY.value = []; accelZ.value = []
  gyroX.value = []; gyroY.value = []; gyroZ.value = []
  baroAlt.value = []; gpsAlt.value = []; speedHistory.value = []
  pdopHistory.value = []; satsHistory.value = []
  liveLats.value = []; liveLons.value = []
  
  if (polyline) polyline.setLatLngs([])
  if (startMarker) mapInstance.removeLayer(startMarker)
  if (endMarker) mapInstance.removeLayer(endMarker)
  if (hoverMarker) mapInstance.removeLayer(hoverMarker)
}

// Hover Marker Sync
const updateHoverMarker = (idx) => {
  if (idx < 0 || idx >= liveLats.value.length) return
  const lat = liveLats.value[idx]
  const lon = liveLons.value[idx]
  
  if (!lat || lat === 0 || !lon || lon === 0) {
    if (hoverMarker) { mapInstance.removeLayer(hoverMarker); hoverMarker = null }
    return
  }
  
  const latlng = [lat, lon]
  if (!hoverMarker) {
    hoverMarker = L.circleMarker(latlng, {
      color: '#fff', fillColor: '#3B82F6', fillOpacity: 1, radius: 8, weight: 3
    }).addTo(mapInstance)
  } else {
    hoverMarker.setLatLng(latlng)
    if (!mapInstance.hasLayer(hoverMarker)) hoverMarker.addTo(mapInstance)
  }
}

// Format X Axis (Time)
const formatX = (val) => {
  if (val === undefined || val === null || isNaN(val)) return val;
  if (val < 60) return Number(val).toFixed(1) + 's';
  if (val < 3600) return (Number(val) / 60).toFixed(1) + 'm';
  return (Number(val) / 3600).toFixed(2) + 'h';
}

// Calculate Distance using Haversine formula (returns km)
const getDistanceFromLatLonInKm = (lat1, lon1, lat2, lon2) => {
  if (!lat1 || !lon1 || !lat2 || !lon2 || lat1 === 0 || lon1 === 0 || lat2 === 0 || lon2 === 0) return 0;
  const R = 6371;
  const dLat = (lat2 - lat1) * (Math.PI/180);
  const dLon = (lon2 - lon1) * (Math.PI/180);
  const a = Math.sin(dLat/2) * Math.sin(dLat/2) +
            Math.cos(lat1 * (Math.PI/180)) * Math.cos(lat2 * (Math.PI/180)) * 
            Math.sin(dLon/2) * Math.sin(dLon/2);
  return R * (2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a)));
}

// Chart Options Builder
const darkTheme = { mode: 'dark' } // Hapus 'palette' agar warna kustom berfungsi!
const sharedGrid = { borderColor: 'rgba(255,255,255,0.05)' }
const buildOptions = (group, yAxisConfig, colors) => {
  // Use decimalsInFloat instead of a custom formatter to avoid Vue 3 Proxy dropping functions
  const formattedYAxis = Array.isArray(yAxisConfig) 
    ? yAxisConfig.map(y => ({ ...y, decimalsInFloat: 2 }))
    : { ...yAxisConfig, decimalsInFloat: 2 };

  return {
    chart: { 
      id: group + '-chart', group: 'sync-telemetry', type: 'line', 
      animations: { enabled: false }, // Completely disable animations to prevent SVG DOM memory spikes
      toolbar: { show: true }, background: 'transparent',
      foreColor: '#e2e8f0', // Force light text for dark background
      events: {
        mouseMove: (event, chartContext, config) => {
          if (config.dataPointIndex !== -1) updateHoverMarker(config.dataPointIndex)
        },
        mouseLeave: () => {
          if (hoverMarker) { mapInstance.removeLayer(hoverMarker); hoverMarker = null }
        }
      }
    },
    stroke: { width: 2, curve: 'straight' },
    xaxis: { 
      type: 'numeric',
      tickAmount: 10,
      labels: { show: true, formatter: formatX }, 
      axisBorder: { show: true } 
    },
    yaxis: formattedYAxis,
    grid: sharedGrid,
    legend: { position: 'top', horizontalAlign: 'left', offsetX: 10 },
    tooltip: { 
      theme: 'dark', 
      x: { show: true, formatter: formatX },
      y: { formatter: (val) => val } // Tampilkan nilai asli mentah (raw value) tanpa pembulatan di hover
    }
  }
}

const accelOptions = buildOptions('accel', { title: { text: 'G' }, tickAmount: 4 })
const accelSeries = computed(() => [
  { name: 'Accel X', data: accelX.value, color: '#EF4444' }, // Vibrant Red
  { name: 'Accel Y', data: accelY.value, color: '#10B981' }, // Vibrant Green
  { name: 'Accel Z', data: accelZ.value, color: '#3B82F6' }  // Vibrant Blue
])

const gyroOptions = buildOptions('gyro', { title: { text: 'deg/s' }, tickAmount: 4 })
const gyroSeries = computed(() => [
  { name: 'Gyro X', data: gyroX.value, color: '#F59E0B' }, // Vibrant Orange
  { name: 'Gyro Y', data: gyroY.value, color: '#8B5CF6' }, // Vibrant Purple
  { name: 'Gyro Z', data: gyroZ.value, color: '#06B6D4' }  // Vibrant Cyan
])

const altOptions = buildOptions('alt', { title: { text: 'Meters' }, tickAmount: 4 })
const altSeries = computed(() => [
  { name: 'Baro Altitude', data: baroAlt.value, color: '#FCD34D' }, // Yellow
  { name: 'GPS Altitude', data: gpsAlt.value, color: '#2DD4BF' }    // Teal
])

const speedOptions = buildOptions('speed', { title: { text: 'km/h' }, tickAmount: 4, min: 0 })
const speedSeries = computed(() => [
  { name: 'Speed', data: speedHistory.value, color: '#F43F5E' } // Rose red
])

const velYMax = ref(100)
const velYMin = ref(-100)
const velXMax = ref(0)
const velXMin = ref(-5)

const iqYMax = ref(10)
const iqYMin = ref(-10)
const iqXMax = ref(0)
const iqXMin = ref(-5)

const phaseYMax = ref(10)
const phaseYMin = ref(-10)
const phaseXMax = ref(0)
const phaseXMin = ref(-5)

const gpsOptions = buildOptions('gps', [
  { seriesName: 'Satellites', title: { text: 'Satellites' }, min: 0, tickAmount: 4 },
  { opposite: true, seriesName: 'PDOP', title: { text: 'PDOP' }, min: 0, tickAmount: 4 }
])
const gpsSeries = computed(() => [
  { name: 'Satellites', data: satsHistory.value, color: '#F472B6' },
  { name: 'PDOP', data: pdopHistory.value, color: '#E2E8F0' }
])

const buildMotorOptions = (id, yAxisConfig, xMinRef, xMaxRef) => {
  return computed(() => {
    return {
      chart: { id, group: 'motor-sync', type: 'line', animations: { enabled: false }, toolbar: { show: false }, background: 'transparent', foreColor: '#e2e8f0' },
      stroke: { width: 2, curve: 'straight' },
      xaxis: { 
        type: 'numeric',
        min: Number(xMinRef.value),
        max: Number(xMaxRef.value),
        tickAmount: 6,
        labels: { formatter: (val) => val.toFixed(1) + 's' } 
      },
      yaxis: { 
        title: yAxisConfig.title,
        min: Number(yAxisConfig.min.value),
        max: Number(yAxisConfig.max.value),
        decimalsInFloat: 2, 
        tickAmount: 4 
      },
      grid: { borderColor: '#334155', strokeDashArray: 3 },
      legend: { position: 'top', horizontalAlign: 'left', offsetX: 10 },
      theme: { mode: 'dark' },
      dataLabels: { enabled: false }
    }
  })
}

const motorVelOptions = buildMotorOptions('motor-vel', { title: { text: 'RPM' }, min: velYMin, max: velYMax }, velXMin, velXMax)
const motorVelSeries = computed(() => [
  { name: 'Velocity', data: motorVel.value, color: '#3B82F6' }
])

const motorIqOptions = buildMotorOptions('motor-iq', { title: { text: 'Amperes' }, min: iqYMin, max: iqYMax }, iqXMin, iqXMax)
const motorIqSeries = computed(() => [
  { name: 'Target Iq', data: motorTarget.value, color: '#EF4444' },
  { name: 'Actual Iq (LPF)', data: motorVq.value, color: '#F59E0B' }
])

const motorPhaseOptions = buildMotorOptions('motor-phase', { title: { text: 'Amperes' }, min: phaseYMin, max: phaseYMax }, phaseXMin, phaseXMax)
const motorPhaseSeries = computed(() => [
  { name: 'Current U', data: motorIa.value, color: '#EF4444' },
  { name: 'Current V', data: motorIb.value, color: '#10B981' },
  { name: 'Current W', data: motorIc.value, color: '#3B82F6' }
])

// Web Serial Logic
const appendToTerminal = (text) => {
  terminalText.value += text + '\n'
  nextTick(() => {
    if (terminalOutput.value) {
      terminalOutput.value.scrollTop = terminalOutput.value.scrollHeight
    }
  })
}

const sendSerialCommand = async () => {
  if (!serialWriter) return
  if (!cmdInput.value) return
  
  const cmd = cmdInput.value + '\r\n'
  appendToTerminal('> ' + cmdInput.value)
  cmdInput.value = ''
  
  try {
    const encoder = new TextEncoder()
    await serialWriter.write(encoder.encode(cmd))
  } catch (err) {
    console.error("Write error:", err)
    appendToTerminal("Error writing to serial port")
  }
}

let motorPendingBuffer = new Uint8Array(0)
const motorDataBuffer = {
  time: [], vel: [], vq: [], target: [], ia: [], ib: [], ic: []
}

setInterval(() => {
  if (isSerialConnected.value && motorDataBuffer.time.length > 0) {
    const latestTime = motorDataBuffer.time[motorDataBuffer.time.length - 1];
    const mapData = (arr) => arr.map(pt => [(pt[0] - latestTime) / 1000, pt[1]]);
    
    motorTime.value = [...motorDataBuffer.time]
    motorVel.value = mapData(motorDataBuffer.vel)
    motorVq.value = mapData(motorDataBuffer.vq)
    motorTarget.value = mapData(motorDataBuffer.target)
    motorIa.value = mapData(motorDataBuffer.ia)
    motorIb.value = mapData(motorDataBuffer.ib)
    motorIc.value = mapData(motorDataBuffer.ic)
  }
}, 100)

const processSerialBinary = (buffer) => {
  const MAX_MOTOR_PTS = 2000 
  let offset = 0
  let textOut = ""
  
  while (offset <= buffer.length - 33) {
    if (buffer[offset] === 0xAA && buffer[offset+1] === 0xBB) {
      if (buffer[offset+32] === 0x55) {
        let crc = 0;
        for(let i=2; i<=30; i++) crc ^= buffer[offset+i];
        
        if (crc === buffer[offset+31]) {
          const view = new DataView(buffer.buffer, buffer.byteOffset + offset + 2, 28)
          const pos = view.getFloat32(0, true)
          const vel = view.getFloat32(4, true)
          const vq = view.getFloat32(8, true)
          const target = view.getFloat32(12, true)
          const ia = view.getFloat32(16, true)
          const ib = view.getFloat32(20, true)
          const ic = view.getFloat32(24, true)
          
          const tStr = Date.now()
          motorDataBuffer.time.push(tStr)
          motorDataBuffer.vel.push([tStr, vel])
          motorDataBuffer.vq.push([tStr, vq])
          motorDataBuffer.target.push([tStr, target])
          motorDataBuffer.ia.push([tStr, ia])
          motorDataBuffer.ib.push([tStr, ib])
          motorDataBuffer.ic.push([tStr, ic])
          
          if (motorDataBuffer.time.length > MAX_MOTOR_PTS) {
            motorDataBuffer.time.shift()
            motorDataBuffer.vel.shift()
            motorDataBuffer.vq.shift()
            motorDataBuffer.target.shift()
            motorDataBuffer.ia.shift()
            motorDataBuffer.ib.shift()
            motorDataBuffer.ic.shift()
          }
          
          offset += 33;
          continue;
        }
      }
    }
    
    const b = buffer[offset];
    if ((b >= 32 && b <= 126) || b === 10 || b === 13) {
       textOut += String.fromCharCode(b);
    }
    offset++;
  }
  
  if (textOut.length > 0) {
      terminalText.value += textOut;
      nextTick(() => {
        if (terminalOutput.value) terminalOutput.value.scrollTop = terminalOutput.value.scrollHeight
      })
  }
  
  return buffer.slice(offset);
}

const toggleSerialConnection = async () => {
  if (isSerialConnected.value) {
    keepSerialReading = false
    if (serialReader) {
      await serialReader.cancel()
    }
    isSerialConnected.value = false
    appendToTerminal("Disconnected from USB")
    return
  }
  
  try {
    serialPort = await navigator.serial.requestPort()
    await serialPort.open({ baudRate: 115200 })
    
    isSerialConnected.value = true
    keepSerialReading = true
    appendToTerminal("Connected to STM32 USB Serial")
    
    serialWriter = serialPort.writable.getWriter()
    
    readLoop()
    
  } catch (err) {
    console.error("Serial Connection Error:", err)
    appendToTerminal("Failed to connect: " + err.message)
  }
}

async function readLoop() {
  while (serialPort.readable && keepSerialReading) {
    serialReader = serialPort.readable.getReader()
    try {
      while (true) {
        const { value, done } = await serialReader.read()
        if (done) break
        if (value) {
          const merged = new Uint8Array(motorPendingBuffer.length + value.length)
          merged.set(motorPendingBuffer)
          merged.set(value, motorPendingBuffer.length)
          
          motorPendingBuffer = processSerialBinary(merged)
        }
      }
    } catch (error) {
      console.error("Read Error:", error)
      break
    } finally {
      serialReader.releaseLock()
    }
  }
  
  if (serialWriter) {
      serialWriter.releaseLock()
      serialWriter = null
  }
  await serialPort.close()
  isSerialConnected.value = false
}

const toggleRecording = () => {
  if (isRecording.value) {
    if (recordedData.value.length === 0) {
      alert("No data recorded yet!")
      isRecording.value = false
      return
    }
    
    let csv = "Time,Ax,Ay,Az,Gx,Gy,Gz,BaroAlt,Lat,Lon,GPSAlt,PDOP,Satellites\n"
    recordedData.value.forEach(r => {
      csv += `${r.time},${r.ax},${r.ay},${r.az},${r.gx},${r.gy},${r.gz},${r.alt},${r.lat},${r.lon},${r.galt},${r.pd},${r.ns}\n`
    })
    
    const blob = new Blob([csv], { type: 'text/csv' })
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `KukangEV_Telemetry_${new Date().toISOString().replace(/[:.]/g, '-')}.csv`
    a.click()
    URL.revokeObjectURL(url)
    
    recordedData.value = []
    isRecording.value = false
  } else {
    recordedData.value = []
    isRecording.value = true
  }
}



// File Upload Handler (Offline Data)
const convertBinToCsv = async (event) => {
  const file = event.target.files[0]
  if (!file) return
  
  const buffer = await file.arrayBuffer()
  const view = new DataView(buffer)
  let offset = 0
  const structLen = 58
  
  let csv = "Timestamp_ms,GPS_Year,GPS_Month,GPS_Day,GPS_Hour,GPS_Min,GPS_Sec,Time_Valid,Accel_X(G),Accel_Y(G),Accel_Z(G),Gyro_X(deg/s),Gyro_Y(deg/s),Gyro_Z(deg/s),Baro_Altitude(m),Latitude,Longitude,GPS_Altitude(m),PDOP,Fix_Type,Satellites\n"
  
  while (offset + structLen <= buffer.byteLength) {
    const ts = view.getUint32(offset, true)
    const yr = view.getUint16(offset + 4, true)
    const mo = view.getUint8(offset + 6)
    const day = view.getUint8(offset + 7)
    const hr = view.getUint8(offset + 8)
    const mi = view.getUint8(offset + 9)
    const se = view.getUint8(offset + 10)
    const tv = view.getUint8(offset + 11)
    
    const ax = view.getFloat32(offset + 12, true).toFixed(6)
    const ay = view.getFloat32(offset + 16, true).toFixed(6)
    const az = view.getFloat32(offset + 20, true).toFixed(6)
    const gx = view.getFloat32(offset + 24, true).toFixed(6)
    const gy = view.getFloat32(offset + 28, true).toFixed(6)
    const gz = view.getFloat32(offset + 32, true).toFixed(6)
    const alt = view.getFloat32(offset + 36, true).toFixed(2)
    
    const lat = view.getFloat32(offset + 40, true).toFixed(7)
    const lon = view.getFloat32(offset + 44, true).toFixed(7)
    const galt = view.getFloat32(offset + 48, true).toFixed(2)
    const pdop = view.getFloat32(offset + 52, true).toFixed(2)
    
    const fix = view.getUint8(offset + 56)
    const sats = view.getUint8(offset + 57)
    
    csv += `${ts},${yr},${mo},${day},${hr},${mi},${se},${tv},${ax},${ay},${az},${gx},${gy},${gz},${alt},${lat},${lon},${galt},${pdop},${fix},${sats}\n`
    offset += structLen
  }
  
  const blob = new Blob([csv], { type: 'text/csv' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = file.name.replace('.bin', '.csv')
  a.click()
  URL.revokeObjectURL(url)
  event.target.value = ''
}

const handleFileUpload = async (event) => {
  const file = event.target.files[0]
  if (!file) return

  isOfflineMode.value = true
  isConnected.value = false
  clearData()

  // Temporary arrays for bulk push
  const tempTime = [], tempAx = [], tempAy = [], tempAz = [], tempGx = [], tempGy = [], tempGz = []
  const tempBAlt = [], tempGAlt = [], tempSpeed = [], tempPdop = [], tempSats = [], tempLat = [], tempLon = []
  
  let prevT = null; let prevLat = null; let prevLon = null;

  if (file.name.endsWith('.bin')) {
    const buffer = await file.arrayBuffer()
    const view = new DataView(buffer)
    let offset = 0
    const structLen = 58
    
    // Downsample large files to prevent browser crashes (max ~400 points)
    const totalStructs = Math.floor(buffer.byteLength / structLen)
    const step = Math.max(1, Math.floor(totalStructs / 400))
    
    while (offset + structLen <= buffer.byteLength) {
      const t = parseFloat((view.getUint32(offset, true) / 1000).toFixed(1))
      tempTime.push(t)
      tempAx.push([t, view.getFloat32(offset + 12, true)])
      tempAy.push([t, view.getFloat32(offset + 16, true)])
      tempAz.push([t, view.getFloat32(offset + 20, true)])
      tempGx.push([t, view.getFloat32(offset + 24, true)])
      tempGy.push([t, view.getFloat32(offset + 28, true)])
      tempGz.push([t, view.getFloat32(offset + 32, true)])
      tempBAlt.push([t, view.getFloat32(offset + 36, true)])
      
      const lat = view.getFloat32(offset + 40, true)
      const lon = view.getFloat32(offset + 44, true)
      tempLat.push(lat); tempLon.push(lon) // Push all points to maintain 1:1 index mapping
      
      let speed = 0;
      if (prevLat && prevLon && prevT && lat !== 0 && lon !== 0) {
        const distKm = getDistanceFromLatLonInKm(prevLat, prevLon, lat, lon);
        if (t - prevT > 0) speed = (distKm / (t - prevT)) * 3600;
      }
      if (lat !== 0 && lon !== 0) { prevLat = lat; prevLon = lon; prevT = t; }
      tempSpeed.push([t, speed])
      
      tempGAlt.push([t, view.getFloat32(offset + 48, true)])
      tempPdop.push([t, view.getFloat32(offset + 52, true)])
      tempSats.push([t, view.getUint8(offset + 57)])
      
      offset += structLen * step
    }
  } else if (file.name.endsWith('.csv')) {
    const text = await file.text()
    const lines = text.split('\n')
    if (lines.length < 2) return
    
    // Check header to determine format
    const header = lines[0].trim()
    const isNewFormat = header.startsWith("Time,Ax")
    
    // Downsample large files to prevent browser crashes (max ~400 points)
    const step = Math.max(1, Math.floor(lines.length / 400))
    
    for (let i = 1; i < lines.length; i += step) {
      if (!lines[i].trim()) continue
      const cols = lines[i].split(',')
      
      if (isNewFormat) {
        if (cols.length < 13) continue
        
        // Time is already in ms (Date.now()) or timestamp
        const t = parseFloat((parseFloat(cols[0]) / 1000).toFixed(1))
        tempTime.push(t)
        tempAx.push([t, parseFloat(cols[1])]); tempAy.push([t, parseFloat(cols[2])]); tempAz.push([t, parseFloat(cols[3])])
        tempGx.push([t, parseFloat(cols[4])]); tempGy.push([t, parseFloat(cols[5])]); tempGz.push([t, parseFloat(cols[6])])
        tempBAlt.push([t, parseFloat(cols[7])])
        
        const lat = parseFloat(cols[8]); const lon = parseFloat(cols[9])
        tempLat.push(lat); tempLon.push(lon) // Push all points to maintain 1:1 index mapping
        
        let speed = 0;
        if (prevLat && prevLon && prevT && lat !== 0 && lon !== 0) {
          const distKm = getDistanceFromLatLonInKm(prevLat, prevLon, lat, lon);
          if (t - prevT > 0) speed = (distKm / (t - prevT)) * 3600;
        }
        if (lat !== 0 && lon !== 0) { prevLat = lat; prevLon = lon; prevT = t; }
        tempSpeed.push([t, speed])
        
        tempGAlt.push([t, parseFloat(cols[10])]); tempPdop.push([t, parseFloat(cols[11])]); tempSats.push([t, parseFloat(cols[12])])
      } else {
        if (cols.length < 21) continue
        
        const t = parseFloat((parseFloat(cols[0]) / 1000).toFixed(1))
        tempTime.push(t)
        tempAx.push([t, parseFloat(cols[8])]); tempAy.push([t, parseFloat(cols[9])]); tempAz.push([t, parseFloat(cols[10])])
        tempGx.push([t, parseFloat(cols[11])]); tempGy.push([t, parseFloat(cols[12])]); tempGz.push([t, parseFloat(cols[13])])
        tempBAlt.push([t, parseFloat(cols[14])])
        
        const lat = parseFloat(cols[15]); const lon = parseFloat(cols[16])
        tempLat.push(lat); tempLon.push(lon) // Push all points to maintain 1:1 index mapping
        
        let speed = 0;
        if (prevLat && prevLon && prevT && lat !== 0 && lon !== 0) {
          const distKm = getDistanceFromLatLonInKm(prevLat, prevLon, lat, lon);
          if (t - prevT > 0) speed = (distKm / (t - prevT)) * 3600;
        }
        if (lat !== 0 && lon !== 0) { prevLat = lat; prevLon = lon; prevT = t; }
        tempSpeed.push([t, speed])
        
        tempGAlt.push([t, parseFloat(cols[17])]); tempPdop.push([t, parseFloat(cols[18])]); tempSats.push([t, parseFloat(cols[20])])
      }
    }
  }
  
  // Assign arrays to Vue Refs
  timeHistory.value = tempTime
  accelX.value = tempAx; accelY.value = tempAy; accelZ.value = tempAz
  gyroX.value = tempGx; gyroY.value = tempGy; gyroZ.value = tempGz
  baroAlt.value = tempBAlt; gpsAlt.value = tempGAlt; speedHistory.value = tempSpeed
  pdopHistory.value = tempPdop; satsHistory.value = tempSats
  liveLats.value = tempLat; liveLons.value = tempLon
  
  updateMapPath(tempLat, tempLon)
}

const updateMapPath = (lats, lons) => {
  const validLatLngs = []
  for (let i = 0; i < lats.length; i++) {
    if (lats[i] !== 0 && lons[i] !== 0) validLatLngs.push([lats[i], lons[i]])
  }
  if (validLatLngs.length === 0) return
  
  polyline.setLatLngs(validLatLngs)
  
  const greenIcon = new L.Icon({
    iconUrl: 'https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-2x-green.png',
    shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/0.7.7/images/marker-shadow.png',
    iconSize: [25, 41], iconAnchor: [12, 41], popupAnchor: [1, -34], shadowSize: [41, 41]
  })
  const redIcon = new L.Icon({
    iconUrl: 'https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-2x-red.png',
    shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/0.7.7/images/marker-shadow.png',
    iconSize: [25, 41], iconAnchor: [12, 41], popupAnchor: [1, -34], shadowSize: [41, 41]
  })
  
  if (startMarker) mapInstance.removeLayer(startMarker)
  if (endMarker) mapInstance.removeLayer(endMarker)
  startMarker = L.marker(validLatLngs[0], {icon: greenIcon}).bindPopup("Start").addTo(mapInstance)
  endMarker = L.marker(validLatLngs[validLatLngs.length - 1], {icon: redIcon}).bindPopup("End").addTo(mapInstance)
  mapInstance.fitBounds(polyline.getBounds(), { padding: [20, 20] })
}

// Leaflet Map Initialization
const initMap = () => {
  mapInstance = L.map('leaflet-map').setView([-7.321, 110.514], 16)
  L.tileLayer('https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png', {
    maxZoom: 17,
    attribution: '&copy; OpenTopoMap'
  }).addTo(mapInstance)
  polyline = L.polyline([], {color: 'red', weight: 4}).addTo(mapInstance)
  
  // Fix Icon Paths
  delete L.Icon.Default.prototype._getIconUrl;
  L.Icon.Default.mergeOptions({
    iconRetinaUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.7.1/images/marker-icon-2x.png',
    iconUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.7.1/images/marker-icon.png',
    shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.7.1/images/marker-shadow.png',
  });
}


onMounted(() => {
  initMap()
  const telemetryRef = dbRef(db, 'telemetry')
  onValue(telemetryRef, (snapshot) => {
    if (isOfflineMode.value) return 
    const data = snapshot.val()
    if (data) {
      isConnected.value = true
      if (connectionTimeout) clearTimeout(connectionTimeout)
      connectionTimeout = setTimeout(() => {
        isConnected.value = false
      }, 3000)
      const MAX_PTS = 120 
      const t = data.ts ? (data.ts / 1000) : (Date.now() / 1000)
      let speed = 0
      if (liveLats.value.length > 0 && liveLons.value.length > 0 && timeHistory.value.length > 0) {
         let lastValidIdx = liveLats.value.length - 1
         while (lastValidIdx >= 0 && (liveLats.value[lastValidIdx] === 0 || !liveLats.value[lastValidIdx])) {
           lastValidIdx--
         }
         if (lastValidIdx >= 0 && data.lat && data.lon && data.lat !== 0 && data.lon !== 0) {
           const prevLat = liveLats.value[lastValidIdx]
           const prevLon = liveLons.value[lastValidIdx]
           const prevT = timeHistory.value[lastValidIdx]
           const distKm = getDistanceFromLatLonInKm(prevLat, prevLon, data.lat, data.lon)
           const dtSec = t - prevT
           if (dtSec > 0) speed = (distKm / dtSec) * 3600
         }
      }
      speedHistory.value = [...speedHistory.value, [t, speed]].slice(-MAX_PTS)
      timeHistory.value = [...timeHistory.value, t].slice(-MAX_PTS)
      accelX.value = [...accelX.value, [t, data.ax || 0]].slice(-MAX_PTS)
      accelY.value = [...accelY.value, [t, data.ay || 0]].slice(-MAX_PTS)
      accelZ.value = [...accelZ.value, [t, data.az || 0]].slice(-MAX_PTS)
      gyroX.value = [...gyroX.value, [t, data.gx || 0]].slice(-MAX_PTS)
      gyroY.value = [...gyroY.value, [t, data.gy || 0]].slice(-MAX_PTS)
      gyroZ.value = [...gyroZ.value, [t, data.gz || 0]].slice(-MAX_PTS)
      baroAlt.value = [...baroAlt.value, [t, data.alt || 0]].slice(-MAX_PTS)
      gpsAlt.value = [...gpsAlt.value, [t, data.galt || 0]].slice(-MAX_PTS)
      pdopHistory.value = [...pdopHistory.value, [t, data.pd || 0]].slice(-MAX_PTS)
      satsHistory.value = [...satsHistory.value, [t, data.ns || 0]].slice(-MAX_PTS)
      liveLats.value = [...liveLats.value, data.lat || 0].slice(-MAX_PTS)
      liveLons.value = [...liveLons.value, data.lon || 0].slice(-MAX_PTS)
      updateMapPath(liveLats.value, liveLons.value)
      if (isRecording.value) {
        recordedData.value.push({
          time: Date.now(),
          ax: data.ax || 0, ay: data.ay || 0, az: data.az || 0,
          gx: data.gx || 0, gy: data.gy || 0, gz: data.gz || 0,
          alt: data.alt || 0, lat: data.lat || 0, lon: data.lon || 0,
          galt: data.galt || 0, pd: data.pd || 0, ns: data.ns || 0
        })
      }
    }
  })
})
</script>

<style>
.sidebar-overlay {
  position: fixed;
  top: 0; left: 0; right: 0; bottom: 0;
  background: rgba(0, 0, 0, 0.5);
  z-index: 999;
}
.sidebar {
  position: fixed;
  top: 0; right: -300px;
  width: 300px; height: 100vh;
  background: #1e293b;
  box-shadow: -2px 0 10px rgba(0,0,0,0.5);
  z-index: 1000;
  transition: right 0.3s ease;
  display: flex; flex-direction: column;
}
.sidebar-open {
  right: 0;
}
.sidebar-header {
  display: flex; justify-content: space-between; align-items: center;
  padding: 20px;
  border-bottom: 1px solid #334155;
}
.sidebar-header h2 { margin: 0; color: #f1f5f9; font-size: 1.2rem; }
.close-btn {
  background: transparent; border: none; color: #94a3b8; font-size: 1.5rem; cursor: pointer;
}
.close-btn:hover { color: #fff; }
.sidebar-content { padding: 20px; flex: 1; }
.sidebar-content h3 { color: #94a3b8; font-size: 0.9rem; text-transform: uppercase; margin-bottom: 15px; }
.sidebar-info {
  margin-top: auto;
  padding: 15px;
  background: #0f172a;
  border-radius: 8px;
  font-size: 0.85rem;
  color: #cbd5e1;
}
.sidebar-info p { margin: 5px 0; }
.sidebar-info strong { color: #38bdf8; }

.tab-container { margin-bottom: 10px; }
.tab-btn { padding: 8px 16px; background: var(--secondary); color: white; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; }
.tab-btn.active { background: var(--accent-primary); color: #000; }
.chart-title {
  font-size: 1rem;
  font-weight: 600;
  color: #f1f5f9;
  margin-bottom: 10px;
}
.axis-input {
  width: 100%;
  padding: 4px;
  border-radius: 4px;
  background: #0f172a;
  border: 1px solid #334155;
  color: #fff;
  margin-top: 2px;
}
</style>