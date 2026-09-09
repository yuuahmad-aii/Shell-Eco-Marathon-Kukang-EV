<template>
  <div style="display: contents;">
    <!-- Teleport Controls to Header -->
    <Teleport to="#header-controls-target" v-if="isActive">
      <button 
        class="file-upload-btn" 
        @click="toggleRecording"
        :style="{ backgroundColor: isRecording ? 'var(--warning)' : 'var(--accent-primary)', color: isRecording ? '#000' : '#fff' }"
      >
        {{ isRecording ? '⏹ Stop & Save Motor CSV' : '⏺ Record Motor Data' }}
      </button>

      <label class="file-upload-btn" style="background-color: var(--secondary); color: #fff; cursor: pointer;">
        📂 Upload Motor Log (.csv)
        <input id="motor-log-input" type="file" accept=".csv" @change="handleFileUpload" hidden />
      </label>

      <button 
        v-if="isOfflineMode"
        class="file-upload-btn" 
        @click="exitOfflineMode"
        style="background-color: #3b82f6; color: #fff;"
      >
        🔄 Live Stream Mode
      </button>
      
      <button 
        class="file-upload-btn" 
        @click="toggleSerialConnection"
        :style="{ backgroundColor: isSerialConnected ? 'var(--warning)' : 'var(--accent-primary)', color: isSerialConnected ? '#000' : '#fff' }"
      >
        {{ isSerialConnected ? 'Disconnect USB' : 'Connect USB Motor' }}
      </button>
      <div class="status" :class="isSerialConnected ? 'status-connected' : (isOfflineMode ? 'status-offline' : 'status-disconnected')">
        {{ isSerialConnected ? 'USB Serial Connected' : (isOfflineMode ? ('Offline: ' + currentLogName) : 'Disconnected') }}
      </div>
    </Teleport>

    <!-- MOTOR TELEMETRY -->
    <main class="dashboard-grid" style="grid-template-columns: 1fr; overflow-y: auto;">
      <div class="charts-column" style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
        <!-- Q1: Target vs Actual Iq -->
        <div class="chart-card" style="display:flex; flex-direction:row; gap:10px; align-items:center;">
          <div style="flex:1; min-width:0;">
            <div class="chart-title">Target vs Actual Iq (A)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="280" :options="motorIqOptions" :series="motorIqSeries"></apexchart>
          </div>
          <div style="width:90px; display:flex; flex-direction:column; gap:5px; font-size:11px; color:#94a3b8;">
             <label>Y Max <input type="number" v-model.lazy="iqYMax" class="axis-input"></label>
             <label>Y Min <input type="number" v-model.lazy="iqYMin" class="axis-input"></label>
             <label v-if="!isOfflineMode">X Max (s) <input type="number" v-model.lazy="iqXMax" class="axis-input"></label>
             <label v-if="!isOfflineMode">X Min (s) <input type="number" v-model.lazy="iqXMin" class="axis-input"></label>
          </div>
        </div>

        <!-- NEW: DC Bus Voltage -->
        <div class="chart-card" style="display:flex; flex-direction:row; gap:10px; align-items:center;">
          <div style="flex:1; min-width:0;">
            <div class="chart-title">DC Bus Voltage (V)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="280" :options="motorVbusOptions" :series="motorVbusSeries"></apexchart>
          </div>
          <div style="width:90px; display:flex; flex-direction:column; gap:5px; font-size:11px; color:#94a3b8;">
             <label>Y Max <input type="number" v-model.lazy="vbusYMax" class="axis-input"></label>
             <label>Y Min <input type="number" v-model.lazy="vbusYMin" class="axis-input"></label>
             <label v-if="!isOfflineMode">X Max (s) <input type="number" v-model.lazy="vbusXMax" class="axis-input"></label>
             <label v-if="!isOfflineMode">X Min (s) <input type="number" v-model.lazy="vbusXMin" class="axis-input"></label>
          </div>
        </div>

        <!-- Q2: Velocity -->
        <div class="chart-card" style="display:flex; flex-direction:row; gap:10px; align-items:center;">
          <div style="flex:1; min-width:0;">
            <div class="chart-title">Electrical Velocity (RPM)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="280" :options="motorVelOptions" :series="motorVelSeries"></apexchart>
          </div>
          <div style="width:90px; display:flex; flex-direction:column; gap:5px; font-size:11px; color:#94a3b8;">
             <label>Y Max <input type="number" v-model.lazy="velYMax" class="axis-input"></label>
             <label>Y Min <input type="number" v-model.lazy="velYMin" class="axis-input"></label>
             <label v-if="!isOfflineMode">X Max (s) <input type="number" v-model.lazy="velXMax" class="axis-input"></label>
             <label v-if="!isOfflineMode">X Min (s) <input type="number" v-model.lazy="velXMin" class="axis-input"></label>
          </div>
        </div>

        <!-- Q3: Phase Current -->
        <div class="chart-card" style="display:flex; flex-direction:row; gap:10px; align-items:center;">
          <div style="flex:1; min-width:0;">
            <div class="chart-title">Phase Current (U, V, W Amperes)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="280" :options="motorPhaseOptions" :series="motorPhaseSeries"></apexchart>
          </div>
          <div style="width:90px; display:flex; flex-direction:column; gap:5px; font-size:11px; color:#94a3b8;">
             <label>Y Max <input type="number" v-model.lazy="phaseYMax" class="axis-input"></label>
             <label>Y Min <input type="number" v-model.lazy="phaseYMin" class="axis-input"></label>
             <label v-if="!isOfflineMode">X Max (s) <input type="number" v-model.lazy="phaseXMax" class="axis-input"></label>
             <label v-if="!isOfflineMode">X Min (s) <input type="number" v-model.lazy="phaseXMin" class="axis-input"></label>
          </div>
        </div>
        
        <!-- Q4: Terminal (spanning full width across 2 columns) -->
        <div class="chart-card" style="grid-column: span 2; display:flex; flex-direction:column;">
          <div class="chart-title">Serial Terminal</div>
          <textarea readonly class="terminal-output" ref="terminalOutput" :value="terminalText" style="flex:1; width:100%; height:200px; background:#1e293b; color:#10b981; font-family:monospace; padding:10px; border-radius:5px; border:1px solid #334155; margin-bottom:10px; resize:none;"></textarea>
          <div style="display:flex; gap:10px;">
            <input type="text" v-model.lazy="cmdInput" @keyup.enter="sendSerialCommand" style="flex:1; padding:10px; background:#0f172a; color:#fff; border:1px solid #334155; border-radius:5px; font-family:monospace;" placeholder="Type command (e.g. s300, $?) and press Enter...">
            <button class="file-upload-btn" @click="sendSerialCommand">Send</button>
            <button class="file-upload-btn" style="background:#334155" @click="terminalText = ''">Clear</button>
          </div>
        </div>
      </div>
    </main>
  </div>
</template>

<script setup>
import { ref, shallowRef, onMounted, computed, nextTick, onActivated, onDeactivated } from 'vue'

const isActive = ref(true)
onActivated(() => { isActive.value = true })
onDeactivated(() => { isActive.value = false })

const isMounted = ref(false)
onMounted(async () => { 
  isMounted.value = true 
  if (typeof window !== 'undefined' && window.location.search.includes('sample=1')) {
    try {
      const res = await fetch('/sample_motor_telemetry.csv')
      const txt = await res.text()
      parseAndLoadCsvText(txt, 'Motor_Telemetry_1788953880563.csv')
    } catch (e) {
      console.error(e)
    }
  }
})

// Motor Telemetry State
const isSerialConnected = ref(false)
const isOfflineMode = ref(false)
const offlineSessionKey = ref(0)
const currentLogName = ref('')
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
const motorVbus = shallowRef([])

const terminalText = ref('')
const cmdInput = ref('')
const terminalOutput = ref(null)

const isRecording = ref(false)
let motorRecordedData = []

// Axis Bounds Configs
const velYMax = ref(100)
const velYMin = ref(-100)
const velXMax = ref(0)
const velXMin = ref(-5)

const iqYMax = ref(10)
const iqYMin = ref(-10)
const iqXMax = ref(0)
const iqXMin = ref(-5)

const vbusYMax = ref(60)
const vbusYMin = ref(0)
const vbusXMax = ref(0)
const vbusXMin = ref(-5)

const phaseYMax = ref(10)
const phaseYMin = ref(-10)
const phaseXMax = ref(0)
const phaseXMin = ref(-5)

// Motor Charts Builder
const buildMotorOptions = (id, yAxisConfig, xMinRef, xMaxRef) => {
  return computed(() => {
    // 1. Validasi Angka Aman: Jika input kosong atau baru diketik "-", gunakan undefined agar grafik tidak crash
    const safeYMin = isNaN(parseFloat(yAxisConfig.min.value)) ? undefined : parseFloat(yAxisConfig.min.value);
    const safeYMax = isNaN(parseFloat(yAxisConfig.max.value)) ? undefined : parseFloat(yAxisConfig.max.value);
    const safeXMin = isNaN(parseFloat(xMinRef.value)) ? undefined : parseFloat(xMinRef.value);
    const safeXMax = isNaN(parseFloat(xMaxRef.value)) ? undefined : parseFloat(xMaxRef.value);

    return {
      chart: { 
        id, 
        group: isOfflineMode.value ? 'motor-sync-telemetry' : undefined,
        type: 'line', 
        animations: { enabled: false }, 
        toolbar: { 
          show: isOfflineMode.value,
          tools: {
            download: true,
            selection: true,
            zoom: true,
            zoomin: true,
            zoomout: true,
            pan: true,
            reset: true
          },
          autoSelected: 'zoom'
        },
        zoom: {
          enabled: isOfflineMode.value,
          type: 'x',
          autoScaleYaxis: false
        },
        pan: {
          enabled: isOfflineMode.value
        },
        background: 'transparent', 
        foreColor: '#e2e8f0' 
      },
      stroke: { width: 2, curve: 'straight' },
      xaxis: { 
        type: 'numeric',
        min: isOfflineMode.value ? undefined : safeXMin,
        max: isOfflineMode.value ? undefined : safeXMax,
        tickAmount: 8,
        labels: { formatter: (val) => (val !== undefined && val !== null && !isNaN(val)) ? Number(val).toFixed(1) + 's' : '' } 
      },
      yaxis: { 
        title: yAxisConfig.title,
        min: safeYMin,
        max: safeYMax,
        decimalsInFloat: 2, 
        tickAmount: 4 
      },
      tooltip: {
        theme: 'dark',
        x: {
          formatter: (val) => (val !== undefined && val !== null && !isNaN(val)) ? Number(val).toFixed(2) + 's' : ''
        }
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

const motorVbusOptions = buildMotorOptions('motor-vbus', { title: { text: 'Volts' }, min: vbusYMin, max: vbusYMax }, vbusXMin, vbusXMax)
const motorVbusSeries = computed(() => [
  { name: 'DC Bus Voltage', data: motorVbus.value, color: '#10B981' }
])

const motorPhaseOptions = buildMotorOptions('motor-phase', { title: { text: 'Amperes' }, min: phaseYMin, max: phaseYMax }, phaseXMin, phaseXMax)
const motorPhaseSeries = computed(() => [
  { name: 'Current U', data: motorIa.value, color: '#EF4444' },
  { name: 'Current V', data: motorIb.value, color: '#10B981' },
  { name: 'Current W', data: motorIc.value, color: '#3B82F6' }
])

// Terminal Methods
const appendToTerminal = (text) => {
  terminalText.value += text + "\n"
  
  // Potong teks jika sudah terlalu panjang agar browser tidak nge-lag
  if (terminalText.value.length > 5000) {
    terminalText.value = terminalText.value.slice(-5000)
  }
  
  nextTick(() => {
    if (terminalOutput.value) {
      terminalOutput.value.scrollTop = terminalOutput.value.scrollHeight
    }
  })
}

const sendSerialCommand = async () => {
  if (!serialWriter || !cmdInput.value) return
  try {
    const cmd = cmdInput.value + "\n"
    appendToTerminal("> " + cmdInput.value)
    const encoder = new TextEncoder()
    await serialWriter.write(encoder.encode(cmd))
    cmdInput.value = ""
  } catch (err) {
    console.error(err)
  }
}

// Data Buffer
const motorDataBuffer = { time: [], vel: [], vq: [], target: [], ia: [], ib: [], ic: [], vbus: [] }
let motorPendingBuffer = new Uint8Array()

// Throttled UI update (live stream only)
setInterval(() => {
  if (isOfflineMode.value) return; // Don't overwrite loaded log with empty/live buffer!
  if (motorDataBuffer.time.length > 0) {
    const latestTime = motorDataBuffer.time[motorDataBuffer.time.length - 1];
    const mapData = (arr) => arr.map(pt => [(pt[0] - latestTime) / 1000, pt[1]]);
    
    motorTime.value = [...motorDataBuffer.time]
    motorVel.value = mapData(motorDataBuffer.vel)
    motorVq.value = mapData(motorDataBuffer.vq)
    motorTarget.value = mapData(motorDataBuffer.target)
    motorIa.value = mapData(motorDataBuffer.ia)
    motorIb.value = mapData(motorDataBuffer.ib)
    motorIc.value = mapData(motorDataBuffer.ic)
    motorVbus.value = mapData(motorDataBuffer.vbus)
  }
}, 100)

// --- OFFLINE LOG VIEWER METHODS ---
const parseAndLoadCsvText = (text, fileName = 'Motor_Telemetry.csv') => {
  const lines = text.split(/\r?\n/)
  if (lines.length < 2) {
    alert("File is empty or invalid format!")
    return
  }

  const header = lines[0].trim().toLowerCase()
  const headerCols = header.split(',').map(c => c.trim())

  // Dynamically find column indices based on header names
  let idxTime = headerCols.findIndex(c => c.includes('time'))
  let idxVel = headerCols.findIndex(c => c.includes('vel') || c.includes('rpm'))
  let idxTarget = headerCols.findIndex(c => c.includes('target'))
  let idxVq = headerCols.findIndex(c => c.includes('actual') || c.includes('vq'))
  let idxU = headerCols.findIndex(c => c.includes('phaseu') || c.includes('current u') || c.includes('ia') || c.includes('duty u'))
  let idxV = headerCols.findIndex(c => c.includes('phasev') || c.includes('current v') || c.includes('ib') || c.includes('duty v'))
  let idxW = headerCols.findIndex(c => c.includes('phasew') || c.includes('current w') || c.includes('ic') || c.includes('duty w'))
  let idxVbus = headerCols.findIndex(c => c.includes('vbus') || c.includes('volt'))

  // Fallbacks if header doesn't match standard names
  if (idxTime === -1) idxTime = 0
  if (idxVel === -1) idxVel = 1
  if (idxTarget === -1) idxTarget = 2
  if (idxVq === -1) idxVq = 3
  if (idxU === -1) idxU = 4
  if (idxV === -1) idxV = 5
  if (idxW === -1) idxW = 6
  if (idxVbus === -1) idxVbus = headerCols.length > 7 ? 7 : -1

  isOfflineMode.value = true
  offlineSessionKey.value++
  currentLogName.value = fileName

  let startTimestamp = null
  const parsedRows = []

  for (let i = 1; i < lines.length; i++) {
    const line = lines[i].trim()
    if (!line) continue
    const rawCols = line.split(',')
    if (rawCols.length < 4) continue

    const timeVal = parseFloat(rawCols[idxTime])
    if (isNaN(timeVal)) continue

    if (startTimestamp === null) {
      startTimestamp = timeVal
    }

    // Convert timestamp into relative seconds from beginning of recording (e.g. 0.0s to 70.8s)
    let tSec = 0
    if (timeVal > 1000000000) {
      // Absolute millisecond epoch
      tSec = parseFloat(((timeVal - startTimestamp) / 1000).toFixed(3))
    } else {
      // Relative seconds or ms
      tSec = parseFloat((timeVal > 10000 ? (timeVal - startTimestamp) / 1000 : timeVal).toFixed(3))
    }

    const vel = !isNaN(parseFloat(rawCols[idxVel])) ? parseFloat(rawCols[idxVel]) : 0
    const target = !isNaN(parseFloat(rawCols[idxTarget])) ? parseFloat(rawCols[idxTarget]) : 0
    const vq = !isNaN(parseFloat(rawCols[idxVq])) ? parseFloat(rawCols[idxVq]) : 0
    const ia = idxU !== -1 && !isNaN(parseFloat(rawCols[idxU])) ? parseFloat(rawCols[idxU]) : 0
    const ib = idxV !== -1 && !isNaN(parseFloat(rawCols[idxV])) ? parseFloat(rawCols[idxV]) : 0
    const ic = idxW !== -1 && !isNaN(parseFloat(rawCols[idxW])) ? parseFloat(rawCols[idxW]) : 0
    const vbus = idxVbus !== -1 && !isNaN(parseFloat(rawCols[idxVbus])) ? parseFloat(rawCols[idxVbus]) : 0

    parsedRows.push({ t: tSec, vel, target, vq, ia, ib, ic, vbus })
  }

  if (parsedRows.length === 0) {
    alert("No valid data rows found in CSV!")
    return
  }

  // Downsample if dataset is large (max 1500 points) to guarantee fast rendering
  const MAX_PTS = 1500
  const step = Math.max(1, Math.floor(parsedRows.length / MAX_PTS))

  const tempVel = []
  const tempTarget = []
  const tempVq = []
  const tempIa = []
  const tempIb = []
  const tempIc = []
  const tempVbus = []

  let minVel = Infinity, maxVel = -Infinity
  let minIq = Infinity, maxIq = -Infinity
  let minPhase = Infinity, maxPhase = -Infinity
  let minVbus = Infinity, maxVbus = -Infinity

  for (let i = 0; i < parsedRows.length; i += step) {
    const r = parsedRows[i]
    tempVel.push([r.t, r.vel])
    tempTarget.push([r.t, r.target])
    tempVq.push([r.t, r.vq])
    tempIa.push([r.t, r.ia])
    tempIb.push([r.t, r.ib])
    tempIc.push([r.t, r.ic])
    tempVbus.push([r.t, r.vbus])

    minVel = Math.min(minVel, r.vel)
    maxVel = Math.max(maxVel, r.vel)
    minIq = Math.min(minIq, r.target, r.vq)
    maxIq = Math.max(maxIq, r.target, r.vq)
    minPhase = Math.min(minPhase, r.ia, r.ib, r.ic)
    maxPhase = Math.max(maxPhase, r.ia, r.ib, r.ic)

    if (r.vbus > 0) {
      minVbus = Math.min(minVbus, r.vbus)
      maxVbus = Math.max(maxVbus, r.vbus)
    }
  }

  // Always include the very last point
  if (parsedRows.length > 1 && (parsedRows.length - 1) % step !== 0) {
    const r = parsedRows[parsedRows.length - 1]
    tempVel.push([r.t, r.vel])
    tempTarget.push([r.t, r.target])
    tempVq.push([r.t, r.vq])
    tempIa.push([r.t, r.ia])
    tempIb.push([r.t, r.ib])
    tempIc.push([r.t, r.ic])
    tempVbus.push([r.t, r.vbus])
  }

  const duration = parsedRows[parsedRows.length - 1].t
  const xMaxSafe = Math.ceil(duration) || 10

  // Set X bounds from 0 to total duration
  velXMin.value = 0; velXMax.value = xMaxSafe
  iqXMin.value = 0; iqXMax.value = xMaxSafe
  phaseXMin.value = 0; phaseXMax.value = xMaxSafe
  vbusXMin.value = 0; vbusXMax.value = xMaxSafe

  // Auto-adjust Y bounds with nice margins
  if (maxVel > -Infinity) {
    velYMin.value = Math.floor(minVel < 0 ? minVel * 1.1 : 0)
    velYMax.value = Math.ceil(maxVel > 0 ? maxVel * 1.15 : 100)
  }
  if (maxIq > -Infinity) {
    iqYMin.value = Math.floor(minIq - 1)
    iqYMax.value = Math.ceil(maxIq + 1)
  }
  if (maxPhase > -Infinity) {
    phaseYMin.value = Math.floor(minPhase - 1)
    phaseYMax.value = Math.ceil(maxPhase + 1)
  }
  if (maxVbus > -Infinity && minVbus < Infinity) {
    vbusYMin.value = Math.max(0, Math.floor(minVbus - 5))
    vbusYMax.value = Math.ceil(maxVbus + 5)
  }

  // Assign to reactive chart series
  motorVel.value = tempVel
  motorTarget.value = tempTarget
  motorVq.value = tempVq
  motorIa.value = tempIa
  motorIb.value = tempIb
  motorIc.value = tempIc
  motorVbus.value = tempVbus

  appendToTerminal(`[LOG VIEWER] Loaded ${fileName} | Samples: ${parsedRows.length} | Duration: ${duration.toFixed(1)}s`)
}

const handleFileUpload = async (event) => {
  const file = event.target.files[0]
  if (!file) return

  try {
    const text = await file.text()
    parseAndLoadCsvText(text, file.name)
  } catch (err) {
    console.error(err)
    alert("Failed to parse log file: " + err.message)
  } finally {
    event.target.value = ''
  }
}

if (typeof window !== 'undefined') {
  window.__loadMotorLogFromText = parseAndLoadCsvText
}

const exitOfflineMode = () => {
  isOfflineMode.value = false
  offlineSessionKey.value++
  currentLogName.value = ''
  
  // Reset axis configs to live defaults
  velXMin.value = -5; velXMax.value = 0; velYMin.value = -100; velYMax.value = 100
  iqXMin.value = -5; iqXMax.value = 0; iqYMin.value = -10; iqYMax.value = 10
  phaseXMin.value = -5; phaseXMax.value = 0; phaseYMin.value = -10; phaseYMax.value = 10
  vbusXMin.value = -5; vbusXMax.value = 0; vbusYMin.value = 0; vbusYMax.value = 60
  
  // Clear charts
  motorVel.value = []
  motorTarget.value = []
  motorVq.value = []
  motorIa.value = []
  motorIb.value = []
  motorIc.value = []
  motorVbus.value = []
  
  appendToTerminal("[LOG VIEWER] Switched to Live Stream Mode")
}

const processSerialBinary = (buffer) => {
  const MAX_MOTOR_PTS = 1500 
  let offset = 0
  let textOut = ""
  
  while (offset <= buffer.length - 37) {
    if (buffer[offset] === 0xAA && buffer[offset+1] === 0xBB) {
      if (buffer[offset+36] === 0x55) {
        const crcExpected = buffer[offset+35]
        let crcCalc = 0
        for(let i=2; i<35; i++) crcCalc ^= buffer[offset+i]
        
        if (crcCalc === crcExpected) {
          const view = new DataView(buffer.buffer, buffer.byteOffset + offset, 37)
          
          const pos = view.getFloat32(2, true)
          const vel = view.getFloat32(6, true)
          const vq = view.getFloat32(10, true)
          const target = view.getFloat32(14, true)
          const ia = view.getFloat32(18, true)
          const ib = view.getFloat32(22, true)
          const ic = view.getFloat32(26, true)
          const vbus = view.getFloat32(30, true)
          
          const tStr = Date.now()
          motorDataBuffer.time.push(tStr)
          motorDataBuffer.vel.push([tStr, vel])
          motorDataBuffer.vq.push([tStr, vq])
          motorDataBuffer.target.push([tStr, target])
          motorDataBuffer.ia.push([tStr, ia])
          motorDataBuffer.ib.push([tStr, ib])
          motorDataBuffer.ic.push([tStr, ic])
          motorDataBuffer.vbus.push([tStr, vbus])
          
          if (isRecording.value) {
            motorRecordedData.push({ time: tStr, vel, target, vq, id: pos, ia, ib, ic, vbus })
          }
          
          if (motorDataBuffer.time.length > MAX_MOTOR_PTS) {
            motorDataBuffer.time.shift()
            motorDataBuffer.vel.shift()
            motorDataBuffer.vq.shift()
            motorDataBuffer.target.shift()
            motorDataBuffer.ia.shift()
            motorDataBuffer.ib.shift()
            motorDataBuffer.ic.shift()
            motorDataBuffer.vbus.shift()
          }
          
          offset += 37;
          continue;
        }
      }
    }
    
    const b = buffer[offset];
    if ((b >= 32 && b <= 126) || b === 10 || b === 13) {
      textOut += String.fromCharCode(b)
    }
    offset++
  }
  
  if (textOut.length > 0) {
    appendToTerminal(textOut)
  }
  
  return buffer.slice(offset)
}

const toggleSerialConnection = async () => {
  if (isSerialConnected.value) {
    keepSerialReading = false
    if (serialReader) await serialReader.cancel()
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
    // STOP RECORDING & SAVE CSV
    if (motorRecordedData.length === 0) {
      alert("No data recorded yet!")
      isRecording.value = false
      return
    }
    
    let csv = "Timestamp_ms,Velocity(RPM),TargetIq,ActualIq,PhaseU,PhaseV,PhaseW,Vbus(V)\n"
    motorRecordedData.forEach(r => {
      csv += `${r.time},${r.vel.toFixed(4)},${r.target.toFixed(4)},${r.vq.toFixed(4)},${r.ia.toFixed(4)},${r.ib.toFixed(4)},${r.ic.toFixed(4)},${r.vbus.toFixed(2)}\n`
    })
    
    const blob = new Blob([csv], { type: 'text/csv' })
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `Motor_Telemetry_${new Date().getTime()}.csv`
    a.click()
    URL.revokeObjectURL(url)
    
    motorRecordedData = []
    isRecording.value = false
  } else {
    // START RECORDING
    motorRecordedData = []
    isRecording.value = true
  }
}
</script>

<style scoped>
.axis-input {
  width: 100%;
  padding: 4px;
  border-radius: 4px;
  background: #0f172a;
  border: 1px solid #334155;
  color: #fff;
  margin-top: 2px;
}
.chart-title {
  font-size: 1rem;
  font-weight: 600;
  color: #f1f5f9;
  margin-bottom: 10px;
}

:deep(.apexcharts-toolbar) {
  top: -4px !important;
  right: 6px !important;
  z-index: 5;
}
:deep(.apexcharts-toolbar svg) {
  fill: #94a3b8 !important;
  transition: fill 0.15s ease;
}
:deep(.apexcharts-toolbar svg:hover) {
  fill: #38bdf8 !important;
}
:deep(.apexcharts-toolbar .apexcharts-selected svg) {
  fill: #3b82f6 !important;
}
:deep(.apexcharts-menu) {
  background: #1e293b !important;
  border: 1px solid #334155 !important;
  color: #e2e8f0 !important;
}
:deep(.apexcharts-menu-item:hover) {
  background: #334155 !important;
}
</style>
