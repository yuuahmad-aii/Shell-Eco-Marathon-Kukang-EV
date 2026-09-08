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
      
      <button 
        class="file-upload-btn" 
        @click="toggleSerialConnection"
        :style="{ backgroundColor: isSerialConnected ? 'var(--warning)' : 'var(--accent-primary)', color: isSerialConnected ? '#000' : '#fff' }"
      >
        {{ isSerialConnected ? 'Disconnect USB' : 'Connect USB Motor' }}
      </button>
      <div class="status" :class="isSerialConnected ? 'status-connected' : 'status-disconnected'">
        {{ isSerialConnected ? 'USB Serial Connected' : 'Disconnected' }}
      </div>
    </Teleport>

    <!-- MOTOR TELEMETRY -->
    <main class="dashboard-grid" style="grid-template-columns: 1fr;">
      <div class="charts-column" style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
        <!-- Q1: Target vs Actual Iq -->
        <div class="chart-card" style="display:flex; flex-direction:row; gap:10px; align-items:center;">
          <div style="flex:1;">
            <div class="chart-title">Target vs Actual Iq (A)</div>
            <apexchart type="line" height="280" :options="motorIqOptions" :series="motorIqSeries"></apexchart>
          </div>
          <div style="width:90px; display:flex; flex-direction:column; gap:5px; font-size:11px; color:#94a3b8;">
             <label>Y Max <input type="number" v-model.lazy="iqYMax" class="axis-input"></label>
             <label>Y Min <input type="number" v-model.lazy="iqYMin" class="axis-input"></label>
             <label>X Max (s) <input type="number" v-model.lazy="iqXMax" class="axis-input"></label>
             <label>X Min (s) <input type="number" v-model.lazy="iqXMin" class="axis-input"></label>
          </div>
        </div>

        <!-- Q2: Velocity -->
        <div class="chart-card" style="display:flex; flex-direction:row; gap:10px; align-items:center;">
          <div style="flex:1;">
            <div class="chart-title">Electrical Velocity (RPM)</div>
            <apexchart type="line" height="280" :options="motorVelOptions" :series="motorVelSeries"></apexchart>
          </div>
          <div style="width:90px; display:flex; flex-direction:column; gap:5px; font-size:11px; color:#94a3b8;">
             <label>Y Max <input type="number" v-model.lazy="velYMax" class="axis-input"></label>
             <label>Y Min <input type="number" v-model.lazy="velYMin" class="axis-input"></label>
             <label>X Max (s) <input type="number" v-model.lazy="velXMax" class="axis-input"></label>
             <label>X Min (s) <input type="number" v-model.lazy="velXMin" class="axis-input"></label>
          </div>
        </div>

        <!-- Q3: Phase Current -->
        <div class="chart-card" style="display:flex; flex-direction:row; gap:10px; align-items:center;">
          <div style="flex:1;">
            <div class="chart-title">Phase Current (U, V, W Amperes)</div>
            <apexchart type="line" height="280" :options="motorPhaseOptions" :series="motorPhaseSeries"></apexchart>
          </div>
          <div style="width:90px; display:flex; flex-direction:column; gap:5px; font-size:11px; color:#94a3b8;">
             <label>Y Max <input type="number" v-model.lazy="phaseYMax" class="axis-input"></label>
             <label>Y Min <input type="number" v-model.lazy="phaseYMin" class="axis-input"></label>
             <label>X Max (s) <input type="number" v-model.lazy="phaseXMax" class="axis-input"></label>
             <label>X Min (s) <input type="number" v-model.lazy="phaseXMin" class="axis-input"></label>
          </div>
        </div>
        
        <!-- Q4: Terminal -->
        <div class="chart-card" style="display:flex; flex-direction:column;">
          <div class="chart-title">Serial Terminal</div>
          <textarea readonly class="terminal-output" ref="terminalOutput" :value="terminalText" style="flex:1; width:100%; height:230px; background:#1e293b; color:#10b981; font-family:monospace; padding:10px; border-radius:5px; border:1px solid #334155; margin-bottom:10px; resize:none;"></textarea>
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
onMounted(() => { isMounted.value = true })

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

const phaseYMax = ref(10)
const phaseYMin = ref(-10)
const phaseXMax = ref(0)
const phaseXMin = ref(-5)

// Motor Charts Builder
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
        // 2. HAPUS 'group' agar grafik berdiri sendiri dan tidak saling reset saat diubah
        type: 'line', 
        animations: { enabled: false }, 
        toolbar: { show: false }, 
        background: 'transparent', 
        foreColor: '#e2e8f0' 
      },
      stroke: { width: 2, curve: 'straight' },
      xaxis: { 
        type: 'numeric',
        min: safeXMin,
        max: safeXMax,
        tickAmount: 6,
        labels: { formatter: (val) => val.toFixed(1) + 's' } 
      },
      yaxis: { 
        title: yAxisConfig.title,
        min: safeYMin,
        max: safeYMax,
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
const motorDataBuffer = { time: [], vel: [], vq: [], target: [], ia: [], ib: [], ic: [] }
let motorPendingBuffer = new Uint8Array()

// Throttled UI update
setInterval(() => {
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
  }
}, 100)

const processSerialBinary = (buffer) => {
  const MAX_MOTOR_PTS = 1500 
  let offset = 0
  let textOut = ""
  
  while (offset <= buffer.length - 33) {
    if (buffer[offset] === 0xAA && buffer[offset+1] === 0xBB) {
      if (buffer[offset+32] === 0x55) {
        const crcExpected = buffer[offset+31]
        let crcCalc = 0
        for(let i=2; i<31; i++) crcCalc ^= buffer[offset+i]
        
        if (crcCalc === crcExpected) {
          const view = new DataView(buffer.buffer, buffer.byteOffset + offset, 33)
          
          const vel = view.getFloat32(2, true)
          const target = view.getFloat32(6, true)
          const vq = view.getFloat32(10, true)
          const id = view.getFloat32(14, true)
          const ia = view.getFloat32(18, true)
          const ib = view.getFloat32(22, true)
          const ic = view.getFloat32(26, true)
          
          const tStr = Date.now()
          motorDataBuffer.time.push(tStr)
          motorDataBuffer.vel.push([tStr, vel])
          motorDataBuffer.vq.push([tStr, vq])
          motorDataBuffer.target.push([tStr, target])
          motorDataBuffer.ia.push([tStr, ia])
          motorDataBuffer.ib.push([tStr, ib])
          motorDataBuffer.ic.push([tStr, ic])
          
          if (isRecording.value) {
            motorRecordedData.push({ time: tStr, vel, target, vq, id, ia, ib, ic })
          }
          
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
    
    let csv = "Timestamp_ms,Velocity(RPM),TargetIq,ActualIq,Id,PhaseU,PhaseV,PhaseW\n"
    motorRecordedData.forEach(r => {
      csv += `${r.time},${r.vel.toFixed(4)},${r.target.toFixed(4)},${r.vq.toFixed(4)},${r.id.toFixed(4)},${r.ia.toFixed(4)},${r.ib.toFixed(4)},${r.ic.toFixed(4)}\n`
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
</style>
