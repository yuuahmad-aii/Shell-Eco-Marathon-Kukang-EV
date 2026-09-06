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
        <div class="status" :class="isConnected ? 'status-connected' : 'status-disconnected'">
          {{ isConnected ? 'Live Connection' : (isOfflineMode ? 'Offline Log Mode' : 'Connecting...') }}
        </div>
      </div>
    </header>

    <main class="dashboard-grid">
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
</template>

<script setup>
import { ref, onMounted, computed, nextTick } from 'vue'
import { db, ref as dbRef, onValue } from './firebase'
import 'leaflet/dist/leaflet.css'
import L from 'leaflet'

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

// Data arrays
const timeHistory = ref([])
const accelX = ref([]); const accelY = ref([]); const accelZ = ref([])
const gyroX = ref([]); const gyroY = ref([]); const gyroZ = ref([])
const baroAlt = ref([]); const gpsAlt = ref([])
const pdopHistory = ref([]); const satsHistory = ref([])
const liveLats = ref([]); const liveLons = ref([])

// Clear all data
const clearData = () => {
  timeHistory.value = []
  accelX.value = []; accelY.value = []; accelZ.value = []
  gyroX.value = []; gyroY.value = []; gyroZ.value = []
  baroAlt.value = []; gpsAlt.value = []
  pdopHistory.value = []; satsHistory.value = []
  liveLats.value = []; liveLons.value = []
  
  if (polyline) polyline.setLatLngs([])
  if (startMarker) mapInstance.removeLayer(startMarker)
  if (endMarker) mapInstance.removeLayer(endMarker)
}

// Format X Axis (Time)
const formatX = (val) => {
  if (val === undefined || val === null || isNaN(val)) return val;
  if (val < 60) return Number(val).toFixed(1) + 's';
  if (val < 3600) return (Number(val) / 60).toFixed(1) + 'm';
  return (Number(val) / 3600).toFixed(2) + 'h';
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
      animations: { enabled: !isOfflineMode.value },
      toolbar: { show: true }, background: 'transparent',
      foreColor: '#e2e8f0' // Force light text for dark background
    },
    stroke: { width: 2, curve: 'straight' },
    xaxis: { 
      categories: timeHistory.value,
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

const accelOptions = computed(() => buildOptions('accel', { title: { text: 'G' }, tickAmount: 4 }))
const accelSeries = computed(() => [
  { name: 'Accel X', data: accelX.value, color: '#EF4444' }, // Vibrant Red
  { name: 'Accel Y', data: accelY.value, color: '#10B981' }, // Vibrant Green
  { name: 'Accel Z', data: accelZ.value, color: '#3B82F6' }  // Vibrant Blue
])

const gyroOptions = computed(() => buildOptions('gyro', { title: { text: 'deg/s' }, tickAmount: 4 }))
const gyroSeries = computed(() => [
  { name: 'Gyro X', data: gyroX.value, color: '#F59E0B' }, // Vibrant Orange
  { name: 'Gyro Y', data: gyroY.value, color: '#8B5CF6' }, // Vibrant Purple
  { name: 'Gyro Z', data: gyroZ.value, color: '#06B6D4' }  // Vibrant Cyan
])

const altOptions = computed(() => buildOptions('alt', { title: { text: 'Meters' }, tickAmount: 4 }))
const altSeries = computed(() => [
  { name: 'Baro Altitude', data: baroAlt.value, color: '#FCD34D' }, // Yellow
  { name: 'GPS Altitude', data: gpsAlt.value, color: '#2DD4BF' }    // Teal
])

const gpsOptions = computed(() => buildOptions('gps', [
  { seriesName: 'Satellites', title: { text: 'Satellites' }, min: 0, tickAmount: 4 },
  { opposite: true, seriesName: 'PDOP', title: { text: 'PDOP' }, min: 0, tickAmount: 4 }
]))
const gpsSeries = computed(() => [
  { name: 'Satellites', data: satsHistory.value, color: '#F472B6' }, // Pink
  { name: 'PDOP', data: pdopHistory.value, color: '#E2E8F0' }        // White-ish
])

// Recording Handler
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
const handleFileUpload = async (event) => {
  const file = event.target.files[0]
  if (!file) return

  isOfflineMode.value = true
  isConnected.value = false
  clearData()

  // Temporary arrays for bulk push
  const tempTime = [], tempAx = [], tempAy = [], tempAz = [], tempGx = [], tempGy = [], tempGz = []
  const tempBAlt = [], tempGAlt = [], tempPdop = [], tempSats = [], tempLat = [], tempLon = []

  if (file.name.endsWith('.bin')) {
    const buffer = await file.arrayBuffer()
    const view = new DataView(buffer)
    let offset = 0
    const structLen = 58
    
    while (offset + structLen <= buffer.byteLength) {
      tempTime.push(parseFloat((view.getUint32(offset, true) / 1000).toFixed(1)))
      tempAx.push(view.getFloat32(offset + 8, true))
      tempAy.push(view.getFloat32(offset + 12, true))
      tempAz.push(view.getFloat32(offset + 16, true))
      tempGx.push(view.getFloat32(offset + 20, true))
      tempGy.push(view.getFloat32(offset + 24, true))
      tempGz.push(view.getFloat32(offset + 28, true))
      tempBAlt.push(view.getFloat32(offset + 32, true))
      
      const lat = view.getFloat32(offset + 36, true)
      const lon = view.getFloat32(offset + 40, true)
      if (lat !== 0 && lon !== 0) { tempLat.push(lat); tempLon.push(lon) }
      
      tempGAlt.push(view.getFloat32(offset + 44, true))
      tempPdop.push(view.getFloat32(offset + 48, true))
      tempSats.push(view.getUint8(offset + 53))
      
      offset += structLen
    }
  } else if (file.name.endsWith('.csv')) {
    const text = await file.text()
    const lines = text.split('\n')
    if (lines.length < 2) return
    
    // Check header to determine format
    const header = lines[0].trim()
    const isNewFormat = header.startsWith("Time,Ax")
    
    for (let i = 1; i < lines.length; i++) {
      if (!lines[i].trim()) continue
      const cols = lines[i].split(',')
      
      if (isNewFormat) {
        if (cols.length < 13) continue
        
        // Time is already in ms (Date.now()) or timestamp
        tempTime.push(parseFloat((parseFloat(cols[0]) / 1000).toFixed(1)))
        tempAx.push(parseFloat(cols[1])); tempAy.push(parseFloat(cols[2])); tempAz.push(parseFloat(cols[3]))
        tempGx.push(parseFloat(cols[4])); tempGy.push(parseFloat(cols[5])); tempGz.push(parseFloat(cols[6]))
        tempBAlt.push(parseFloat(cols[7]))
        
        const lat = parseFloat(cols[8]); const lon = parseFloat(cols[9])
        if (lat !== 0 && lon !== 0) { tempLat.push(lat); tempLon.push(lon) }
        
        tempGAlt.push(parseFloat(cols[10])); tempPdop.push(parseFloat(cols[11])); tempSats.push(parseFloat(cols[12]))
      } else {
        if (cols.length < 21) continue
        
        tempTime.push(parseFloat((parseFloat(cols[0]) / 1000).toFixed(1)))
        tempAx.push(parseFloat(cols[8])); tempAy.push(parseFloat(cols[9])); tempAz.push(parseFloat(cols[10]))
        tempGx.push(parseFloat(cols[11])); tempGy.push(parseFloat(cols[12])); tempGz.push(parseFloat(cols[13]))
        tempBAlt.push(parseFloat(cols[14]))
        
        const lat = parseFloat(cols[15]); const lon = parseFloat(cols[16])
        if (lat !== 0 && lon !== 0) { tempLat.push(lat); tempLon.push(lon) }
        
        tempGAlt.push(parseFloat(cols[17])); tempPdop.push(parseFloat(cols[18])); tempSats.push(parseFloat(cols[20]))
      }
    }
  }
  
  // Assign arrays to Vue Refs
  timeHistory.value = tempTime
  accelX.value = tempAx; accelY.value = tempAy; accelZ.value = tempAz
  gyroX.value = tempGx; gyroY.value = tempGy; gyroZ.value = tempGz
  baroAlt.value = tempBAlt; gpsAlt.value = tempGAlt
  pdopHistory.value = tempPdop; satsHistory.value = tempSats
  
  updateMapPath(tempLat, tempLon)
}

const updateMapPath = (lats, lons) => {
  if (lats.length === 0) return
  const latlngs = lats.map((lat, i) => [lat, lons[i]])
  polyline.setLatLngs(latlngs)
  
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
  startMarker = L.marker(latlngs[0], {icon: greenIcon}).bindPopup("Start").addTo(mapInstance)
  endMarker = L.marker(latlngs[latlngs.length - 1], {icon: redIcon}).bindPopup("End").addTo(mapInstance)
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
  
  // Firebase Live Telemetry
  const telemetryRef = dbRef(db, 'telemetry')
  onValue(telemetryRef, (snapshot) => {
    if (isOfflineMode.value) return // Disable live updates if a file is loaded
    
    const data = snapshot.val()
    if (data) {
      isConnected.value = true
      
      // Watchdog Timer: Jika 3 detik tidak ada data baru, anggap terputus
      if (connectionTimeout) clearTimeout(connectionTimeout)
      connectionTimeout = setTimeout(() => {
        isConnected.value = false
      }, 3000)
      
      const MAX_PTS = 120 // ~1 minute at 2Hz
      
      // Gunakan timestamp dari mikrokontroler (dalam detik) agar grafik numeric x-axis berfungsi
      const timeLabel = data.ts ? (data.ts / 1000) : (Date.now() / 1000)
      timeHistory.value = [...timeHistory.value, timeLabel].slice(-MAX_PTS)
      
      // Menggunakan reassignment agar Vue Reactivity memicu render ulang pada ApexCharts
      accelX.value = [...accelX.value, data.ax || 0].slice(-MAX_PTS)
      accelY.value = [...accelY.value, data.ay || 0].slice(-MAX_PTS)
      accelZ.value = [...accelZ.value, data.az || 0].slice(-MAX_PTS)
      
      gyroX.value = [...gyroX.value, data.gx || 0].slice(-MAX_PTS)
      gyroY.value = [...gyroY.value, data.gy || 0].slice(-MAX_PTS)
      gyroZ.value = [...gyroZ.value, data.gz || 0].slice(-MAX_PTS)
      
      baroAlt.value = [...baroAlt.value, data.alt || 0].slice(-MAX_PTS)
      gpsAlt.value = [...gpsAlt.value, data.galt || 0].slice(-MAX_PTS)
      pdopHistory.value = [...pdopHistory.value, data.pd || 0].slice(-MAX_PTS)
      satsHistory.value = [...satsHistory.value, data.ns || 0].slice(-MAX_PTS)
      
      // Simpan history GPS untuk menggambar garis lintasan
      if (data.lat && data.lon && data.lat !== 0 && data.lon !== 0) {
        liveLats.value.push(data.lat)
        liveLons.value.push(data.lon)
        updateMapPath(liveLats.value, liveLons.value)
      }
      
      // Rekam data ke CSV jika mode record aktif
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
