<template>
  <div style="display: contents;">
    <Teleport to="#header-controls-target" v-if="isActive">
      <button 
        class="file-upload-btn" 
        @click="toggleRecording"
        :style="{ backgroundColor: isRecording ? 'var(--warning)' : 'var(--accent-primary)', color: isRecording ? '#000' : '#fff' }"
      >
        {{ isRecording ? '⏹ Stop & Save Vehicle CSV' : '⏺ Record Vehicle Data' }}
      </button>

      <label class="file-upload-btn" style="cursor: pointer;">
        Upload Log (.bin / .csv)
        <input type="file" accept=".bin,.csv" @change="handleFileUpload" hidden />
      </label>

      <button 
        v-if="isOfflineMode"
        class="file-upload-btn" 
        @click="exitOfflineMode"
        style="background-color: #3b82f6; color: #fff;"
      >
        🔄 Live Stream Mode
      </button>

      <label class="file-upload-btn" style="background-color: var(--secondary); color: #fff; cursor: pointer;">
        Convert .bin to .csv
        <input type="file" accept=".bin" @change="convertBinToCsv" hidden />
      </label>

      <div class="status" :class="isConnected ? 'status-connected' : (isOfflineMode ? 'status-offline' : 'status-disconnected')">
        {{ isConnected ? 'Live Connection' : (isOfflineMode ? 'Offline Log Mode' : 'Disconnected') }}
      </div>
    </Teleport>

    <!-- VEHICLE TELEMETRY -->
    <main class="dashboard-grid" style="grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);">

      <!-- Left Column: Map & Serial Terminal (50% Width) -->
      <div class="map-column">
        <!-- GPS Map Card -->
        <div class="chart-card map-card">
          <div class="chart-title">GPS Track Map (Terrain)</div>
          <div id="leaflet-map" style="flex: 1; min-height: 380px; border-radius: 8px;"></div>
        </div>

        <!-- Telemetry Board Serial Terminal -->
        <div class="chart-card terminal-card" style="display: flex; flex-direction: column; min-height: 340px;">
          <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
            <div class="chart-title" style="margin-bottom: 0;">📟 Telemetry Serial Terminal</div>
            <button 
              class="file-upload-btn" 
              style="padding: 5px 12px; font-size: 12px;"
              :style="{ backgroundColor: isSerialConnected ? 'var(--warning)' : 'var(--accent-primary)', color: isSerialConnected ? '#000' : '#fff' }"
              @click="toggleSerialConnection"
            >
              {{ isSerialConnected ? 'Disconnect USB' : 'Connect USB Telemetry' }}
            </button>
          </div>

          <!-- Quick CLI Command Buttons -->
          <div style="display: flex; gap: 6px; margin-bottom: 8px; flex-wrap: wrap;">
            <button class="quick-cli-btn" @click="sendQuickCli('$$')" title="Show Configs ($10-$30)">$$ (Config)</button>
            <button class="quick-cli-btn" @click="sendQuickCli('$?')" title="Show Live Sensor Status">$? (Status)</button>
            <button class="quick-cli-btn" @click="sendQuickCli('$i')" title="Show Firmware & Sensor Info">$i (Info)</button>
            <button class="quick-cli-btn" @click="sendQuickCli('$sd')" title="Show MicroSD Card Status">$sd (MicroSD)</button>
            <button class="quick-cli-btn" @click="sendQuickCli('$cal')" title="Calibrate Sensors">$cal (Calibrate)</button>
          </div>

          <!-- Terminal Output -->
          <textarea 
            readonly 
            ref="terminalOutput" 
            :value="terminalText" 
            class="terminal-output" 
            style="flex: 1; width: 100%; min-height: 180px; background: #0f172a; color: #10b981; font-family: 'Fira Code', monospace; font-size: 12px; padding: 10px; border-radius: 6px; border: 1px solid var(--border); resize: vertical; margin-bottom: 8px;"
          ></textarea>

          <!-- Input row -->
          <div style="display: flex; gap: 8px;">
            <input 
              type="text" 
              v-model="cmdInput" 
              @keyup.enter="sendSerialCommand" 
              style="flex: 1; padding: 6px 12px; background: #0f172a; color: #fff; border: 1px solid var(--border); border-radius: 4px; font-family: monospace; font-size: 13px;" 
              placeholder="Ketik perintah (misal: $$, $?, $10=500, $w)..."
            />
            <button class="file-upload-btn" style="padding: 6px 14px; font-size: 13px;" @click="sendSerialCommand">Send</button>
            <button class="file-upload-btn" style="background: var(--border); color: #fff; padding: 6px 10px; font-size: 13px;" @click="terminalText = ''">Clear</button>
          </div>
        </div>
      </div>

      <!-- Right Column: Charts (50% Width) -->
      <div class="charts-column">
        <!-- 1. Motor Electrical (DC Bus Voltage & Actual Iq Current) -->
        <div class="chart-card chart-with-axis">
          <div style="flex: 1; min-width: 0;">
            <div class="chart-title">Motor Electrical (Voltage & Current)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="220" :options="elecOptions" :series="elecSeries"></apexchart>
          </div>
          <div class="axis-panel">
            <label>Y Max <input type="number" v-model.lazy="elecYMax" class="axis-input"></label>
            <label>Y Min <input type="number" v-model.lazy="elecYMin" class="axis-input"></label>
          </div>
        </div>

        <!-- 2. Wheel RPM (Timer 2 CH1 vs Timer 5 CH2) -->
        <div class="chart-card chart-with-axis">
          <div style="flex: 1; min-width: 0;">
            <div class="chart-title">Wheel RPM (TIM2 Right vs TIM5 Left)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="220" :options="rpmOptions" :series="rpmSeries"></apexchart>
          </div>
          <div class="axis-panel">
            <label>Y Max <input type="number" v-model.lazy="rpmYMax" class="axis-input"></label>
            <label>Y Min <input type="number" v-model.lazy="rpmYMin" class="axis-input"></label>
          </div>
        </div>

        <!-- 3. Temperature Sensors (DS18B20 Suhu 1 & Suhu 2) -->
        <div class="chart-card chart-with-axis">
          <div style="flex: 1; min-width: 0;">
            <div class="chart-title">Temperature Sensors (Suhu 1 & Suhu 2)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="220" :options="tempOptions" :series="tempSeries"></apexchart>
          </div>
          <div class="axis-panel">
            <label>Y Max <input type="number" v-model.lazy="tempYMax" class="axis-input"></label>
            <label>Y Min <input type="number" v-model.lazy="tempYMin" class="axis-input"></label>
          </div>
        </div>

        <!-- 4. Acceleration -->
        <div class="chart-card chart-with-axis">
          <div style="flex: 1; min-width: 0;">
            <div class="chart-title">Acceleration (G)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="200" :options="accelOptions" :series="accelSeries"></apexchart>
          </div>
          <div class="axis-panel">
            <label>Y Max <input type="number" v-model.lazy="accelYMax" class="axis-input"></label>
            <label>Y Min <input type="number" v-model.lazy="accelYMin" class="axis-input"></label>
          </div>
        </div>

        <!-- 5. Angular Velocity (Gyro) -->
        <div class="chart-card chart-with-axis">
          <div style="flex: 1; min-width: 0;">
            <div class="chart-title">Angular Velocity (Gyro)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="200" :options="gyroOptions" :series="gyroSeries"></apexchart>
          </div>
          <div class="axis-panel">
            <label>Y Max <input type="number" v-model.lazy="gyroYMax" class="axis-input"></label>
            <label>Y Min <input type="number" v-model.lazy="gyroYMin" class="axis-input"></label>
          </div>
        </div>

        <!-- 6. Vehicle Speed -->
        <div class="chart-card chart-with-axis">
          <div style="flex: 1; min-width: 0;">
            <div class="chart-title">Vehicle Speed (km/h)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="200" :options="speedOptions" :series="speedSeries"></apexchart>
          </div>
          <div class="axis-panel">
            <label>Y Max <input type="number" v-model.lazy="speedYMax" class="axis-input"></label>
            <label>Y Min <input type="number" v-model.lazy="speedYMin" class="axis-input"></label>
          </div>
        </div>

        <!-- 7. Altitude -->
        <div class="chart-card chart-with-axis">
          <div style="flex: 1; min-width: 0;">
            <div class="chart-title">Altitude (Baro vs GPS)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="200" :options="altOptions" :series="altSeries"></apexchart>
          </div>
          <div class="axis-panel">
            <label>Y Max <input type="number" v-model.lazy="altYMax" class="axis-input"></label>
            <label>Y Min <input type="number" v-model.lazy="altYMin" class="axis-input"></label>
          </div>
        </div>

        <!-- 8. GPS Quality -->
        <div class="chart-card chart-with-axis">
          <div style="flex: 1; min-width: 0;">
            <div class="chart-title">GPS Quality (Satellites & PDOP)</div>
            <apexchart :key="isOfflineMode ? ('offline-' + offlineSessionKey) : 'live'" type="line" height="200" :options="gpsOptions" :series="gpsSeries"></apexchart>
          </div>
          <div class="axis-panel">
            <label>Y Max <input type="number" v-model.lazy="gpsYMax" class="axis-input"></label>
            <label>Y Min <input type="number" v-model.lazy="gpsYMin" class="axis-input"></label>
          </div>
        </div>
      </div>
    </main>
  </div>
</template>

<script setup>
import { ref, shallowRef, onMounted, computed, nextTick, onActivated, onDeactivated } from 'vue'
import { db, ref as dbRef, onValue } from '../firebase'
import 'leaflet/dist/leaflet.css'
import L from 'leaflet'

const isActive = ref(true)
onActivated(() => { isActive.value = true })
onDeactivated(() => { isActive.value = false })

const isMounted = ref(false)
onMounted(() => { isMounted.value = true })

// --- STATE VARIABEL ---
const isRecording = ref(false)
const isConnected = ref(false)
const isOfflineMode = ref(false)
const offlineSessionKey = ref(0)
const recordedData = ref([])
let connectionTimeout = null

// --- WEB SERIAL TERMINAL STATE ---
const isSerialConnected = ref(false)
const terminalText = ref("--- Telemetry USB Serial Terminal ---\nHubungkan kabel USB STM32F446RE ke laptop lalu klik 'Connect USB Telemetry'.\nPerintah CLI tersedia:\n  $$      : Tampilkan parameter konfigurasi ($10-$30)\n  $?      : Tampilkan status sensor real-time\n  $i      : Tampilkan info firmware & chip ID sensor\n  $sd     : Tampilkan status microSD\n  $10=val : Ubah konfigurasi (misal $10=500)\n  $w      : Simpan konfigurasi ke memori\n\n")
const cmdInput = ref('')
const terminalOutput = ref(null)
let serialPort = null
let serialReader = null
let serialWriter = null
let keepSerialReading = false

// --- VARIABEL PETA (LEAFLET) ---
let mapInstance = null
let polyline = null
let startMarker = null
let endMarker = null
let hoverMarker = null

// --- ARRAY DATA GRAFIK ---
const timeHistory = shallowRef([])
const accelX = shallowRef([]); const accelY = shallowRef([]); const accelZ = shallowRef([])
const gyroX = shallowRef([]); const gyroY = shallowRef([]); const gyroZ = shallowRef([])
const baroAlt = shallowRef([]); const gpsAlt = shallowRef([])
const speedHistory = shallowRef([])
const pdopHistory = shallowRef([]); const satsHistory = shallowRef([])
const liveLats = shallowRef([]); const liveLons = shallowRef([])

// Variabel data telemetri baru
const vbusHistory = shallowRef([])
const iqHistory = shallowRef([])
const r1History = shallowRef([])
const r2History = shallowRef([])
const t1History = shallowRef([])
const t2History = shallowRef([])

// --- AXIS BOUNDS REFS ---
const elecYMax = ref(60)
const elecYMin = ref(-10)
const rpmYMax = ref(2000)
const rpmYMin = ref(0)
const tempYMax = ref(80)
const tempYMin = ref(0)
const accelYMax = ref(2)
const accelYMin = ref(-2)
const gyroYMax = ref(100)
const gyroYMin = ref(-100)
const speedYMax = ref(50)
const speedYMin = ref(0)
const altYMax = ref(100)
const altYMin = ref(0)
const gpsYMax = ref(20)
const gpsYMin = ref(0)

// --- FUNGSI HELPER & FORMATTER ---
const clearData = () => {
  timeHistory.value = []
  accelX.value = []; accelY.value = []; accelZ.value = []
  gyroX.value = []; gyroY.value = []; gyroZ.value = []
  baroAlt.value = []; gpsAlt.value = []; speedHistory.value = []
  pdopHistory.value = []; satsHistory.value = []
  liveLats.value = []; liveLons.value = []

  vbusHistory.value = []
  iqHistory.value = []
  r1History.value = []
  r2History.value = []
  t1History.value = []
  t2History.value = []
  
  if (polyline) polyline.setLatLngs([])
  if (startMarker && mapInstance) mapInstance.removeLayer(startMarker)
  if (endMarker && mapInstance) mapInstance.removeLayer(endMarker)
  if (hoverMarker && mapInstance) mapInstance.removeLayer(hoverMarker)
}

const exitOfflineMode = () => {
  isOfflineMode.value = false
  clearData()
}

const formatX = (val) => {
  if (val === undefined || val === null || isNaN(val)) return '';
  if (val < 60) return Number(val).toFixed(1) + 's';
  if (val < 3600) return (Number(val) / 60).toFixed(1) + 'm';
  return (Number(val) / 3600).toFixed(2) + 'h';
}

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

const updateHoverMarker = (idx) => {
  if (idx < 0 || idx >= liveLats.value.length || !mapInstance) return
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

// --- TERMINAL SERIAL METHODS ---
const appendToTerminal = (text) => {
  terminalText.value += text
  if (terminalText.value.length > 8000) {
    terminalText.value = terminalText.value.slice(-8000)
  }
  nextTick(() => {
    if (terminalOutput.value) {
      terminalOutput.value.scrollTop = terminalOutput.value.scrollHeight
    }
  })
}

const toggleSerialConnection = async () => {
  if (isSerialConnected.value) {
    keepSerialReading = false
    try {
      if (serialReader) {
        await serialReader.cancel()
        serialReader.releaseLock()
        serialReader = null
      }
      if (serialWriter) {
        serialWriter.releaseLock()
        serialWriter = null
      }
      if (serialPort) {
        await serialPort.close()
        serialPort = null
      }
    } catch (e) {
      console.error(e)
    }
    isSerialConnected.value = false
    appendToTerminal("\n[Serial Disconnected]\n")
  } else {
    if (!("serial" in navigator)) {
      alert("Web Serial API tidak didukung pada browser ini. Gunakan Google Chrome, Microsoft Edge, atau Opera.")
      return
    }
    try {
      serialPort = await navigator.serial.requestPort()
      await serialPort.open({ baudRate: 115200 })
      isSerialConnected.value = true
      keepSerialReading = true
      appendToTerminal("\n[Connected to Telemetry Board @ 115200 baud]\n")

      const textDecoder = new TextDecoderStream()
      serialPort.readable.pipeTo(textDecoder.writable)
      serialReader = textDecoder.readable.getReader()

      const textEncoder = new TextEncoderStream()
      textEncoder.readable.pipeTo(serialPort.writable)
      serialWriter = textEncoder.writable.getWriter()

      readSerialLoop()
    } catch (err) {
      console.error(err)
      isSerialConnected.value = false
      appendToTerminal(`\n[Koneksi Gagal: ${err.message}]\n`)
    }
  }
}

const readSerialLoop = async () => {
  try {
    while (keepSerialReading && serialReader) {
      const { value, done } = await serialReader.read()
      if (done) break
      if (value) {
        appendToTerminal(value)
      }
    }
  } catch (err) {
    if (keepSerialReading) {
      console.error("Serial read error:", err)
      appendToTerminal(`\n[Read Error: ${err.message}]\n`)
    }
  }
}

const sendSerialCommand = async () => {
  if (!serialWriter || !cmdInput.value.trim()) return
  try {
    const cmd = cmdInput.value.trim()
    appendToTerminal("> " + cmd + "\n")
    await serialWriter.write(cmd + "\r\n")
    cmdInput.value = ""
  } catch (err) {
    console.error(err)
  }
}

const sendQuickCli = async (cmd) => {
  if (!serialWriter) {
    cmdInput.value = cmd
    return
  }
  try {
    appendToTerminal("> " + cmd + "\n")
    await serialWriter.write(cmd + "\r\n")
  } catch (err) {
    console.error(err)
  }
}

// --- RECORDING & LOGGING ---
const toggleRecording = () => {
  if (isRecording.value) {
    if (recordedData.value.length === 0) {
      alert("Belum ada data yang terekam!")
      isRecording.value = false
      return
    }
    
    let csv = "Time,Timestamp_ms,Ax,Ay,Az,Gx,Gy,Gz,BaroAlt,Lat,Lon,GPSAlt,PDOP,Satellites,SpeedLeft,SpeedRight,SpeedAvg,Vbus,Iq,RPM_TIM2CH1,RPM_TIM5CH2,Temp1,Temp2\n"
    recordedData.value.forEach(r => {
      csv += `${r.time},${r.ts ?? 0},${r.ax},${r.ay},${r.az},${r.gx},${r.gy},${r.gz},${r.alt},${r.lat},${r.lon},${r.galt},${r.pd},${r.ns},${r.sl ?? 0},${r.sr ?? 0},${r.speed ?? 0},${r.vbus ?? 0},${r.iq ?? 0},${r.r1 ?? 0},${r.r2 ?? 0},${r.t1 ?? 0},${r.t2 ?? 0}\n`
    })
    
    const blob = new Blob([csv], { type: 'text/csv' })
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `KukangEV_Vehicle_Telemetry_${new Date().getTime()}.csv`
    a.click()
    URL.revokeObjectURL(url)
    
    recordedData.value = []
    isRecording.value = false
  } else {
    recordedData.value = []
    isRecording.value = true
  }
}

// --- BUILD APEXCHARTS OPTIONS DENGAN TOOLBAR KONDISIONAL & AXIS MIN/MAX ---
const sharedGrid = { borderColor: 'rgba(255,255,255,0.08)', strokeDashArray: 3 }

const buildVehicleOptions = (group, titleText, yMinRef, yMaxRef) => {
  return computed(() => {
    const safeYMin = isNaN(parseFloat(yMinRef?.value)) ? undefined : parseFloat(yMinRef.value)
    const safeYMax = isNaN(parseFloat(yMaxRef?.value)) ? undefined : parseFloat(yMaxRef.value)

    return {
      chart: { 
        id: group + '-chart', 
        group: isOfflineMode.value ? 'sync-telemetry' : undefined, 
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
        foreColor: '#e2e8f0',
        events: {
          mouseMove: (event, chartContext, config) => {
            if (config && config.dataPointIndex !== -1) updateHoverMarker(config.dataPointIndex)
          },
          mouseLeave: () => {
            if (hoverMarker && mapInstance) { mapInstance.removeLayer(hoverMarker); hoverMarker = null }
          }
        }
      },
      stroke: { width: 2, curve: 'straight' },
      xaxis: { 
        type: 'numeric',
        tickAmount: 8,
        labels: { show: true, formatter: formatX }, 
        axisBorder: { show: true } 
      },
      yaxis: {
        title: { text: titleText },
        min: safeYMin,
        max: safeYMax,
        decimalsInFloat: 2,
        tickAmount: 4
      },
      grid: sharedGrid,
      legend: { position: 'top', horizontalAlign: 'left', offsetX: 10 },
      tooltip: { 
        theme: 'dark', 
        x: { show: true, formatter: formatX },
        y: { formatter: (val) => (val !== undefined && val !== null && !isNaN(val)) ? Number(val).toFixed(2) : '' } 
      }
    }
  })
}

// 1. Motor Electrical (DC Bus Voltage & Actual Iq)
const elecOptions = buildVehicleOptions('elec', 'V / A', elecYMin, elecYMax)
const elecSeries = computed(() => [
  { name: 'DC Bus Voltage (V)', data: vbusHistory.value, color: '#10B981' },
  { name: 'Actual Iq (A)', data: iqHistory.value, color: '#F59E0B' }
])

// 2. Wheel RPM (TIM2 CH1 & TIM5 CH2)
const rpmOptions = buildVehicleOptions('rpm', 'RPM', rpmYMin, rpmYMax)
const rpmSeries = computed(() => [
  { name: 'TIM2 CH1 (Right Wheel)', data: r1History.value, color: '#3B82F6' },
  { name: 'TIM5 CH2 (Left Wheel)', data: r2History.value, color: '#A855F7' }
])

// 3. Temperature Sensors (Suhu 1 & Suhu 2)
const tempOptions = buildVehicleOptions('temp', '°C', tempYMin, tempYMax)
const tempSeries = computed(() => [
  { name: 'Suhu 1 (°C)', data: t1History.value, color: '#F43F5E' },
  { name: 'Suhu 2 (°C)', data: t2History.value, color: '#06B6D4' }
])

// 4. Accel
const accelOptions = buildVehicleOptions('accel', 'G', accelYMin, accelYMax)
const accelSeries = computed(() => [
  { name: 'Accel X', data: accelX.value, color: '#EF4444' }, 
  { name: 'Accel Y', data: accelY.value, color: '#10B981' }, 
  { name: 'Accel Z', data: accelZ.value, color: '#3B82F6' }  
])

// 5. Gyro
const gyroOptions = buildVehicleOptions('gyro', 'deg/s', gyroYMin, gyroYMax)
const gyroSeries = computed(() => [
  { name: 'Gyro X', data: gyroX.value, color: '#F59E0B' }, 
  { name: 'Gyro Y', data: gyroY.value, color: '#8B5CF6' }, 
  { name: 'Gyro Z', data: gyroZ.value, color: '#06B6D4' }  
])

// 6. Speed
const speedOptions = buildVehicleOptions('speed', 'km/h', speedYMin, speedYMax)
const speedSeries = computed(() => [
  { name: 'Speed (km/h)', data: speedHistory.value, color: '#10B981' }
])

// 7. Altitude
const altOptions = buildVehicleOptions('alt', 'Meters', altYMin, altYMax)
const altSeries = computed(() => [
  { name: 'Baro Altitude', data: baroAlt.value, color: '#FCD34D' }, 
  { name: 'GPS Altitude', data: gpsAlt.value, color: '#2DD4BF' }    
])

// 8. GPS Quality
const gpsOptions = buildVehicleOptions('gps', 'Value', gpsYMin, gpsYMax)
const gpsSeries = computed(() => [
  { name: 'Satellites', data: satsHistory.value, color: '#10B981' },
  { name: 'PDOP', data: pdopHistory.value, color: '#F59E0B' }
])

// --- FILE UPLOAD & LOG PARSER ---
// Helper validasi header record binary:
// Memeriksa apakah header masuk akal (year: 1980 / 2020..2035, month: 1..12, day: 1..31, hour/min/sec wajar)
const isValidRecordHeader = (view, offset, bufLen) => {
  if (offset + 12 > bufLen) return false
  const yr = view.getUint16(offset + 4, true)
  const mo = view.getUint8(offset + 6)
  const day = view.getUint8(offset + 7)
  const hr = view.getUint8(offset + 8)
  const mi = view.getUint8(offset + 9)
  const se = view.getUint8(offset + 10)
  const tv = view.getUint8(offset + 11)

  if (yr !== 1980 && (yr < 2020 || yr > 2035)) return false
  if (mo < 1 || mo > 12) return false
  if (day < 1 || day > 31) return false
  if (hr > 23 || mi > 59 || se > 59) return false
  if (tv !== 0 && tv !== 1) return false
  return true
}

const isSaneFloat = (val) => {
  if (val === null || val === undefined || isNaN(val) || !isFinite(val)) return false
  const abs = Math.abs(val)
  if (abs > 1e12) return false
  if (abs > 0 && abs < 1e-15) return false
  return true
}

const convertBinToCsv = async (event) => {
  const file = event.target.files[0]
  if (!file) return
  
  const buffer = await file.arrayBuffer()
  const view = new DataView(buffer)
  let offset = 0
  
  // Deteksi format otomatis dengan memvalidasi timestamp sekuensial
  let is90Byte = false
  if (buffer.byteLength >= 116) {
    const ts0 = view.getUint32(0, true)
    const ts58 = view.getUint32(58, true)
    const ts90 = (buffer.byteLength >= 180) ? view.getUint32(90, true) : 0xFFFFFFFF
    const is58Valid = (ts58 >= ts0 && (ts58 - ts0) < 10000) && isValidRecordHeader(view, 58, buffer.byteLength)
    const is90Valid = (ts90 >= ts0 && (ts90 - ts0) < 10000) && isValidRecordHeader(view, 90, buffer.byteLength)
    if (is90Valid && !is58Valid) is90Byte = true
    else if (is58Valid && !is90Valid) is90Byte = false
    else if (buffer.byteLength % 90 === 0 && buffer.byteLength % 58 !== 0) is90Byte = true
    else if (buffer.byteLength % 58 === 0 && buffer.byteLength % 90 !== 0) is90Byte = false
    else is90Byte = (buffer.byteLength % 90 === 0)
  }
  const structLen = is90Byte ? 90 : 58
  
  const header = is90Byte
    ? "Timestamp_ms,GPS_Year,GPS_Month,GPS_Day,GPS_Hour,GPS_Min,GPS_Sec,Time_Valid,Accel_X(G),Accel_Y(G),Accel_Z(G),Gyro_X(deg/s),Gyro_Y(deg/s),Gyro_Z(deg/s),Baro_Altitude(m),Latitude,Longitude,GPS_Altitude(m),PDOP,Fix_Type,Satellites,SpeedLeft,SpeedRight,Vbus,Iq,RPM_TIM2CH1,RPM_TIM5CH2,Temp1,Temp2\n"
    : "Timestamp_ms,GPS_Year,GPS_Month,GPS_Day,GPS_Hour,GPS_Min,GPS_Sec,Time_Valid,Accel_X(G),Accel_Y(G),Accel_Z(G),Gyro_X(deg/s),Gyro_Y(deg/s),Gyro_Z(deg/s),Baro_Altitude(m),Latitude,Longitude,GPS_Altitude(m),PDOP,Fix_Type,Satellites\n"
  
  const rows = [header]
  
  while (offset + structLen <= buffer.byteLength) {
    if (isValidRecordHeader(view, offset, buffer.byteLength)) {
      const ts = view.getUint32(offset, true)
      const yr = view.getUint16(offset + 4, true)
      const mo = view.getUint8(offset + 6)
      const day = view.getUint8(offset + 7)
      const hr = view.getUint8(offset + 8)
      const mi = view.getUint8(offset + 9)
      const se = view.getUint8(offset + 10)
      const tv = view.getUint8(offset + 11)
      
      const ax = view.getFloat32(offset + 12, true)
      const ay = view.getFloat32(offset + 16, true)
      const az = view.getFloat32(offset + 20, true)
      const gx = view.getFloat32(offset + 24, true)
      const gy = view.getFloat32(offset + 28, true)
      const gz = view.getFloat32(offset + 32, true)
      const alt = view.getFloat32(offset + 36, true)
      
      const lat = view.getFloat32(offset + 40, true)
      const lon = view.getFloat32(offset + 44, true)
      const galt = view.getFloat32(offset + 48, true)
      const pdop = view.getFloat32(offset + 52, true)
      
      const fix = view.getUint8(offset + 56)
      const sats = view.getUint8(offset + 57)
      
      if (is90Byte) {
        const sl = view.getFloat32(offset + 58, true)
        const sr = view.getFloat32(offset + 62, true)
        const vbus = view.getFloat32(offset + 66, true)
        const iq = view.getFloat32(offset + 70, true)
        const r1 = view.getFloat32(offset + 74, true)
        const r2 = view.getFloat32(offset + 78, true)
        const t1 = view.getFloat32(offset + 82, true)
        const t2 = view.getFloat32(offset + 86, true)

        if (isSaneFloat(ax) && isSaneFloat(ay) && isSaneFloat(az) &&
            isSaneFloat(gx) && isSaneFloat(gy) && isSaneFloat(gz) &&
            isSaneFloat(alt) && isSaneFloat(lat) && isSaneFloat(lon) &&
            isSaneFloat(galt) && isSaneFloat(pdop) &&
            isSaneFloat(sl) && isSaneFloat(sr) && isSaneFloat(vbus) &&
            isSaneFloat(iq) && isSaneFloat(r1) && isSaneFloat(r2) &&
            isSaneFloat(t1) && isSaneFloat(t2)) {
          rows.push(`${ts},${yr},${mo},${day},${hr},${mi},${se},${tv},${ax.toFixed(4)},${ay.toFixed(4)},${az.toFixed(4)},${gx.toFixed(2)},${gy.toFixed(2)},${gz.toFixed(2)},${alt.toFixed(2)},${lat.toFixed(7)},${lon.toFixed(7)},${galt.toFixed(2)},${pdop.toFixed(2)},${fix},${sats},${sl.toFixed(2)},${sr.toFixed(2)},${vbus.toFixed(2)},${iq.toFixed(2)},${r1.toFixed(0)},${r2.toFixed(0)},${t1.toFixed(2)},${t2.toFixed(2)}\n`)
        }
      } else {
        if (isSaneFloat(ax) && isSaneFloat(ay) && isSaneFloat(az) &&
            isSaneFloat(gx) && isSaneFloat(gy) && isSaneFloat(gz) &&
            isSaneFloat(alt) && isSaneFloat(lat) && isSaneFloat(lon) &&
            isSaneFloat(galt) && isSaneFloat(pdop)) {
          rows.push(`${ts},${yr},${mo},${day},${hr},${mi},${se},${tv},${ax.toFixed(4)},${ay.toFixed(4)},${az.toFixed(4)},${gx.toFixed(2)},${gy.toFixed(2)},${gz.toFixed(2)},${alt.toFixed(2)},${lat.toFixed(7)},${lon.toFixed(7)},${galt.toFixed(2)},${pdop.toFixed(2)},${fix},${sats}\n`)
        }
      }
      offset += structLen
    } else {
      offset += 1
      while (offset + structLen <= buffer.byteLength) {
        if (isValidRecordHeader(view, offset, buffer.byteLength)) break
        offset += 1
      }
    }
  }
  
  const blob = new Blob(rows, { type: 'text/csv' })
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
  offlineSessionKey.value++
  clearData()

  const tempTime = [], tempAx = [], tempAy = [], tempAz = [], tempGx = [], tempGy = [], tempGz = []
  const tempBAlt = [], tempGAlt = [], tempSpeed = [], tempPdop = [], tempSats = [], tempLat = [], tempLon = []
  const tempVbus = [], tempIq = [], tempR1 = [], tempR2 = [], tempT1 = [], tempT2 = []
  
  let prevT = null; let prevLat = null; let prevLon = null;

  if (file.name.endsWith('.bin')) {
    const buffer = await file.arrayBuffer()
    const view = new DataView(buffer)
    
    // Deteksi format otomatis dengan memvalidasi timestamp sekuensial
    let is90Byte = false
    if (buffer.byteLength >= 116) {
      const ts0 = view.getUint32(0, true)
      const ts58 = view.getUint32(58, true)
      const ts90 = (buffer.byteLength >= 180) ? view.getUint32(90, true) : 0xFFFFFFFF
      const is58Valid = (ts58 >= ts0 && (ts58 - ts0) < 10000) && isValidRecordHeader(view, 58, buffer.byteLength)
      const is90Valid = (ts90 >= ts0 && (ts90 - ts0) < 10000) && isValidRecordHeader(view, 90, buffer.byteLength)
      if (is90Valid && !is58Valid) is90Byte = true
      else if (is58Valid && !is90Valid) is90Byte = false
      else if (buffer.byteLength % 90 === 0 && buffer.byteLength % 58 !== 0) is90Byte = true
      else if (buffer.byteLength % 58 === 0 && buffer.byteLength % 90 !== 0) is90Byte = false
      else is90Byte = (buffer.byteLength % 90 === 0)
    }
    const structLen = is90Byte ? 90 : 58

    // Kumpulkan seluruh offset record yang valid (resilient frame sync)
    const validOffsets = []
    let scanOffset = 0
    while (scanOffset + structLen <= buffer.byteLength) {
      if (isValidRecordHeader(view, scanOffset, buffer.byteLength)) {
        const lat = view.getFloat32(scanOffset + 40, true)
        const lon = view.getFloat32(scanOffset + 44, true)
        if (isSaneFloat(lat) && isSaneFloat(lon)) {
          validOffsets.push(scanOffset)
        }
        scanOffset += structLen
      } else {
        scanOffset += 1
        while (scanOffset + structLen <= buffer.byteLength) {
          if (isValidRecordHeader(view, scanOffset, buffer.byteLength)) break
          scanOffset += 1
        }
      }
    }
    
    if (validOffsets.length === 0) {
      alert("File binary kosong atau tidak ditemukan data telemetri valid!")
      event.target.value = ''
      return
    }

    // Batasi titik rendering ~400-500 poin agar UI sangat cepat dan tidak hang/unresponsive
    const step = Math.max(1, Math.floor(validOffsets.length / 400))
    
    for (let i = 0; i < validOffsets.length; i += step) {
      const offset = validOffsets[i]
      const rawTs = view.getUint32(offset, true)
      const t = parseFloat((rawTs / 1000).toFixed(2))
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
      tempLat.push(lat)
      tempLon.push(lon)
      
      tempGAlt.push([t, view.getFloat32(offset + 48, true)])
      tempPdop.push([t, view.getFloat32(offset + 52, true)])
      tempSats.push([t, view.getUint8(offset + 57)])

      if (is90Byte) {
        const sl = view.getFloat32(offset + 58, true)
        const sr = view.getFloat32(offset + 62, true)
        const vbus = view.getFloat32(offset + 66, true)
        const iq = view.getFloat32(offset + 70, true)
        let r1 = view.getFloat32(offset + 74, true)
        let r2 = view.getFloat32(offset + 78, true)
        const t1 = view.getFloat32(offset + 82, true)
        const t2 = view.getFloat32(offset + 86, true)

        // Hitung fallback Wheel RPM jika data tersimpan bernilai 0 tapi roda berputar
        const circM = (Math.PI * 500) / 1000
        if (r1 <= 0 && sr > 0) r1 = Math.round((sr * 1000) / (circM * 60))
        if (r2 <= 0 && sl > 0) r2 = Math.round((sl * 1000) / (circM * 60))

        const spd = Number((((sl || 0) + (sr || 0)) / 2).toFixed(1))
        tempSpeed.push([t, spd])
        tempVbus.push([t, vbus])
        tempIq.push([t, iq])
        tempR1.push([t, r1])
        tempR2.push([t, r2])
        tempT1.push([t, t1])
        tempT2.push([t, t2])
      } else {
        // Mode 58-byte: Isi semua array agar ApexCharts grup tersinkronisasi tidak crash/hang
        let speed = 0;
        if (prevLat && prevLon && prevT && lat !== 0 && lon !== 0) {
          const distKm = getDistanceFromLatLonInKm(prevLat, prevLon, lat, lon);
          if (t - prevT > 0) speed = (distKm / (t - prevT)) * 3600;
        }
        if (lat !== 0 && lon !== 0) { prevLat = lat; prevLon = lon; prevT = t; }
        tempSpeed.push([t, Number(speed.toFixed(1))])

        tempVbus.push([t, 0])
        tempIq.push([t, 0])
        tempR1.push([t, 0])
        tempR2.push([t, 0])
        tempT1.push([t, 0])
        tempT2.push([t, 0])
      }
    }
  } else if (file.name.endsWith('.csv')) {
    const text = await file.text()
    const lines = text.split(/\r?\n/)
    if (lines.length < 2) return
    
    const headerCols = lines[0].trim().toLowerCase().split(',').map(c => c.trim())
    
    let idxTime = headerCols.findIndex(c => c.includes('time'))
    let idxAx = headerCols.findIndex(c => c.includes('ax') || c.includes('accel_x'))
    let idxAy = headerCols.findIndex(c => c.includes('ay') || c.includes('accel_y'))
    let idxAz = headerCols.findIndex(c => c.includes('az') || c.includes('accel_z'))
    let idxGx = headerCols.findIndex(c => c.includes('gx') || c.includes('gyro_x'))
    let idxGy = headerCols.findIndex(c => c.includes('gy') || c.includes('gyro_y'))
    let idxGz = headerCols.findIndex(c => c.includes('gz') || c.includes('gyro_z'))
    let idxAlt = headerCols.findIndex(c => c.includes('baro') || c.includes('alt') && !c.includes('gps'))
    let idxLat = headerCols.findIndex(c => c.includes('lat'))
    let idxLon = headerCols.findIndex(c => c.includes('lon'))
    let idxGAlt = headerCols.findIndex(c => c.includes('gpsalt') || c.includes('gps_altitude'))
    let idxPdop = headerCols.findIndex(c => c.includes('pdop') || c.includes('pd'))
    let idxSats = headerCols.findIndex(c => c.includes('sat'))

    let idxVbus = headerCols.findIndex(c => c.includes('vbus') || c.includes('volt'))
    let idxIq = headerCols.findIndex(c => c.includes('iq') || c.includes('current'))
    let idxR1 = headerCols.findIndex(c => c.includes('tim2') || c.includes('rpm1') || c.includes('r1'))
    let idxR2 = headerCols.findIndex(c => c.includes('tim5') || c.includes('rpm2') || c.includes('r2'))
    let idxSr = headerCols.findIndex(c => c.includes('speedright') || c.includes('sr'))
    let idxSl = headerCols.findIndex(c => c.includes('speedleft') || c.includes('sl'))
    let idxSpd = headerCols.findIndex(c => c.includes('speedavg') || c.includes('speed') && !c.includes('left') && !c.includes('right'))
    let idxT1 = headerCols.findIndex(c => c.includes('temp1') || c.includes('t1') || c.includes('suhu1'))
    let idxT2 = headerCols.findIndex(c => c.includes('temp2') || c.includes('t2') || c.includes('suhu2'))

    const step = Math.max(1, Math.floor(lines.length / 400))
    
    for (let i = 1; i < lines.length; i += step) {
      if (!lines[i].trim()) continue
      const cols = lines[i].split(',')
      
      const rawTime = parseFloat(cols[idxTime >= 0 ? idxTime : 0])
      const t = parseFloat((rawTime > 10000000 ? (rawTime / 1000) : rawTime).toFixed(1))
      tempTime.push(t)
      
      if (idxAx >= 0) tempAx.push([t, parseFloat(cols[idxAx]) || 0])
      if (idxAy >= 0) tempAy.push([t, parseFloat(cols[idxAy]) || 0])
      if (idxAz >= 0) tempAz.push([t, parseFloat(cols[idxAz]) || 0])
      if (idxGx >= 0) tempGx.push([t, parseFloat(cols[idxGx]) || 0])
      if (idxGy >= 0) tempGy.push([t, parseFloat(cols[idxGy]) || 0])
      if (idxGz >= 0) tempGz.push([t, parseFloat(cols[idxGz]) || 0])
      if (idxAlt >= 0) tempBAlt.push([t, parseFloat(cols[idxAlt]) || 0])
      
      const lat = idxLat >= 0 ? parseFloat(cols[idxLat]) || 0 : 0
      const lon = idxLon >= 0 ? parseFloat(cols[idxLon]) || 0 : 0
      tempLat.push(lat)
      tempLon.push(lon)
      
      let speed = 0;
      if (idxSpd >= 0) {
        speed = parseFloat(cols[idxSpd]) || 0
      } else if (idxSl >= 0 || idxSr >= 0) {
        const slVal = idxSl >= 0 ? parseFloat(cols[idxSl]) || 0 : 0
        const srVal = idxSr >= 0 ? parseFloat(cols[idxSr]) || 0 : 0
        speed = (slVal + srVal) / 2
      } else if (prevLat && prevLon && prevT && lat !== 0 && lon !== 0) {
        const distKm = getDistanceFromLatLonInKm(prevLat, prevLon, lat, lon);
        if (t - prevT > 0) speed = (distKm / (t - prevT)) * 3600;
      }
      if (lat !== 0 && lon !== 0) { prevLat = lat; prevLon = lon; prevT = t; }
      tempSpeed.push([t, Number(speed.toFixed(1))])
      
      if (idxGAlt >= 0) tempGAlt.push([t, parseFloat(cols[idxGAlt]) || 0])
      if (idxPdop >= 0) tempPdop.push([t, parseFloat(cols[idxPdop]) || 0])
      if (idxSats >= 0) tempSats.push([t, parseFloat(cols[idxSats]) || 0])

      tempVbus.push([t, idxVbus >= 0 ? parseFloat(cols[idxVbus]) || 0 : 0])
      tempIq.push([t, idxIq >= 0 ? parseFloat(cols[idxIq]) || 0 : 0])

      let r1Val = idxR1 >= 0 ? parseFloat(cols[idxR1]) || 0 : 0
      let r2Val = idxR2 >= 0 ? parseFloat(cols[idxR2]) || 0 : 0
      const srVal = idxSr >= 0 ? parseFloat(cols[idxSr]) || 0 : 0
      const slVal = idxSl >= 0 ? parseFloat(cols[idxSl]) || 0 : 0
      if (r1Val <= 0 && srVal > 0) r1Val = Math.round((srVal * 1000) / (((Math.PI * 500) / 1000) * 60))
      if (r2Val <= 0 && slVal > 0) r2Val = Math.round((slVal * 1000) / (((Math.PI * 500) / 1000) * 60))
      tempR1.push([t, r1Val])
      tempR2.push([t, r2Val])

      tempT1.push([t, idxT1 >= 0 ? parseFloat(cols[idxT1]) || 0 : 0])
      tempT2.push([t, idxT2 >= 0 ? parseFloat(cols[idxT2]) || 0 : 0])
    }
  }

  timeHistory.value = tempTime
  accelX.value = tempAx; accelY.value = tempAy; accelZ.value = tempAz
  gyroX.value = tempGx; gyroY.value = tempGy; gyroZ.value = tempGz
  baroAlt.value = tempBAlt; gpsAlt.value = tempGAlt; speedHistory.value = tempSpeed
  pdopHistory.value = tempPdop; satsHistory.value = tempSats
  liveLats.value = tempLat; liveLons.value = tempLon

  vbusHistory.value = tempVbus
  iqHistory.value = tempIq
  r1History.value = tempR1
  r2History.value = tempR2
  t1History.value = tempT1
  t2History.value = tempT2

  updateMapPath(tempLat, tempLon)
  event.target.value = ''
}

// --- PETA LEAFLET UPDATE ---
const updateMapPath = (lats, lons) => {
  if (!mapInstance || !polyline || lats.length === 0) return
  const validLatLngs = []
  for (let i = 0; i < lats.length; i++) {
    if (lats[i] !== 0 && lons[i] !== 0 && !isNaN(lats[i]) && !isNaN(lons[i])) {
      validLatLngs.push([lats[i], lons[i]])
    }
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
  
  if (validLatLngs.length > 1) {
    mapInstance.fitBounds(polyline.getBounds(), { padding: [20, 20] })
  } else if (validLatLngs.length === 1) {
    mapInstance.setView(validLatLngs[0], 16)
  }
}

const initMap = () => {
  mapInstance = L.map('leaflet-map').setView([-7.321, 110.514], 16)
  L.tileLayer('https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png', {
    maxZoom: 17,
    attribution: '&copy; OpenTopoMap'
  }).addTo(mapInstance)
  polyline = L.polyline([], {color: 'red', weight: 4}).addTo(mapInstance)
  
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
      }, 6000)

      const MAX_PTS = 120 

      // Validasi timestamp agar tidak melonjak ke epoch miliaran detik atau mundur saat MCU reset
      let t = (data.ts !== undefined && data.ts !== null && !isNaN(data.ts))
        ? Number((data.ts / 1000).toFixed(2))
        : Number((Date.now() / 1000).toFixed(2))

      // Deteksi jika STM32 restart (waktu t mundur drastis > 3 detik dari waktu sebelumnya)
      if (timeHistory.value.length > 0) {
        const lastT = timeHistory.value[timeHistory.value.length - 1]
        if (t < lastT - 3.0) {
          clearData()
        } else if (t <= lastT) {
          t = Number((lastT + 0.05).toFixed(2))
        }
      }

      // Kalkulasi kecepatan kendaraan: prioritaskan kecepatan roda (sl & sr) dari sensor STM32
      let speed = 0
      const sl = (data.sl !== undefined && data.sl !== null) ? Number(data.sl) : null
      const sr = (data.sr !== undefined && data.sr !== null) ? Number(data.sr) : null

      if (sl !== null || sr !== null) {
        speed = Number((((sl || 0) + (sr || 0)) / ((sl !== null && sr !== null) ? 2 : 1)).toFixed(1))
      } else if (liveLats.value.length > 0 && liveLons.value.length > 0 && timeHistory.value.length > 0) {
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
          if (dtSec > 0) speed = Number(((distKm / dtSec) * 3600).toFixed(1))
        }
      }

      timeHistory.value = [...timeHistory.value, t].slice(-MAX_PTS)
      speedHistory.value = [...speedHistory.value, [t, speed]].slice(-MAX_PTS)
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

      // Parameter telemetri motor & suhu
      const vb = (data.vbus !== undefined) ? Number(data.vbus) : (data.vb ? Number(data.vb) : 0)
      const iq = (data.iq !== undefined) ? Number(data.iq) : 0

      // Wheel RPM: TIM2 Right Wheel (sr) vs TIM5 Left Wheel (sl)
      // Diameter roda Kukang EV = 500mm -> Keliling roda = PI * 500mm = ~1.5708 meter
      const WHEEL_DIAMETER_MM = 500
      const CIRCUMFERENCE_M = (Math.PI * WHEEL_DIAMETER_MM) / 1000

      let r1 = (data.r1 !== undefined && data.r1 !== null) ? Number(data.r1) : (data.rpm1 ? Number(data.rpm1) : 0)
      let r2 = (data.r2 !== undefined && data.r2 !== null) ? Number(data.r2) : (data.rpm2 ? Number(data.rpm2) : 0)

      // Sinkronisasi otomatis: Jika r1 atau r2 di database bernilai 0 / belum diset,
      // kalkulasikan RPM roda secara langsung dari sensor kecepatan roda (sr & sl)
      if (r1 <= 0 && sr !== null && sr > 0) {
        r1 = Number(((sr * 1000) / (CIRCUMFERENCE_M * 60)).toFixed(0))
      }
      if (r2 <= 0 && sl !== null && sl > 0) {
        r2 = Number(((sl * 1000) / (CIRCUMFERENCE_M * 60)).toFixed(0))
      }

      // Sensor suhu: Suhu 1 (t1) & Suhu 2 (t2)
      let t1 = (data.t1 !== undefined && data.t1 !== null) ? Number(data.t1) : (data.temp1 ? Number(data.temp1) : 0)
      let t2 = (data.t2 !== undefined && data.t2 !== null) ? Number(data.t2) : (data.temp2 ? Number(data.temp2) : 0)
      if (t2 <= 0 && t1 > 0) {
        t2 = t1
      } else if (t1 <= 0 && t2 > 0) {
        t1 = t2
      }

      vbusHistory.value = [...vbusHistory.value, [t, vb]].slice(-MAX_PTS)
      iqHistory.value = [...iqHistory.value, [t, iq]].slice(-MAX_PTS)
      r1History.value = [...r1History.value, [t, r1]].slice(-MAX_PTS)
      r2History.value = [...r2History.value, [t, r2]].slice(-MAX_PTS)
      t1History.value = [...t1History.value, [t, t1]].slice(-MAX_PTS)
      t2History.value = [...t2History.value, [t, t2]].slice(-MAX_PTS)

      updateMapPath(liveLats.value, liveLons.value)

      if (isRecording.value) {
        recordedData.value.push({
          time: Date.now(),
          ts: data.ts || 0,
          ax: data.ax || 0, ay: data.ay || 0, az: data.az || 0,
          gx: data.gx || 0, gy: data.gy || 0, gz: data.gz || 0,
          alt: data.alt || 0, lat: data.lat || 0, lon: data.lon || 0,
          galt: data.galt || 0, pd: data.pd || 0, ns: data.ns || 0,
          sl: sl !== null ? sl : 0, sr: sr !== null ? sr : 0, speed: speed,
          vbus: vb, iq: iq, r1: r1, r2: r2, t1: t1, t2: t2
        })
      }
    }
  })
})
</script>

<style scoped>
/* Layout 50% / 50% split screen */
.dashboard-grid {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
  gap: 10px;
  flex: 1;
  min-height: 0;
  overflow: hidden;
}

.map-column {
  display: flex;
  flex-direction: column;
  gap: 10px;
  min-width: 0;
  overflow-y: auto;
}

.charts-column {
  display: flex;
  flex-direction: column;
  gap: 10px;
  min-width: 0;
  overflow-y: auto;
  padding-right: 4px;
}

.chart-title {
  font-size: 0.95rem;
  font-weight: 600;
  color: #f1f5f9;
  margin-bottom: 8px;
}

.chart-with-axis {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 10px;
  padding: 12px;
}

.axis-panel {
  width: 85px;
  display: flex;
  flex-direction: column;
  gap: 6px;
  font-size: 11px;
  color: #94a3b8;
}

.axis-input {
  width: 100%;
  padding: 4px;
  border-radius: 4px;
  background: #0f172a;
  border: 1px solid #334155;
  color: #fff;
  margin-top: 2px;
  font-family: monospace;
}

.quick-cli-btn {
  background: #1e293b;
  border: 1px solid #334155;
  color: #38bdf8;
  padding: 3px 8px;
  border-radius: 4px;
  font-size: 11px;
  font-family: monospace;
  cursor: pointer;
  transition: background 0.15s;
}

.quick-cli-btn:hover {
  background: #334155;
  color: #fff;
}
</style>