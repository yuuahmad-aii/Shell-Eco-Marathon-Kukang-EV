<template>
  <div class="dashboard-container">
    <header class="header">
      <div class="logo">
        <div class="pulse-dot" :class="{ 'connected': isConnected }"></div>
        <h1>🏎️ Kukang EV <span>Telemetry Analysis</span></h1>
      </div>
      
      <div class="header-controls">
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

// Clear all data
const clearData = () => {
  timeHistory.value = []
  accelX.value = []; accelY.value = []; accelZ.value = []
  gyroX.value = []; gyroY.value = []; gyroZ.value = []
  baroAlt.value = []; gpsAlt.value = []
  pdopHistory.value = []; satsHistory.value = []
  
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
const darkTheme = { mode: 'dark', palette: 'palette1' }
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
      toolbar: { show: true }, background: 'transparent' 
    },
    theme: darkTheme,
    colors: colors,
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
    legend: { position: 'top', horizontalAlign: 'right' },
    tooltip: { 
      theme: 'dark', 
      x: { show: true, formatter: formatX },
      y: { formatter: (val) => val } // Tampilkan nilai asli mentah (raw value) tanpa pembulatan di hover
    }
  }
}

const accelOptions = computed(() => buildOptions('accel', { title: { text: 'G' }, tickAmount: 4 }, ['#ff3333', '#33ff33', '#3333ff']))
const accelSeries = computed(() => [
  { name: 'Accel X', data: accelX.value },
  { name: 'Accel Y', data: accelY.value },
  { name: 'Accel Z', data: accelZ.value }
])

const gyroOptions = computed(() => buildOptions('gyro', { title: { text: 'deg/s' }, tickAmount: 4 }, ['#ff9933', '#cc33ff', '#33ccff']))
const gyroSeries = computed(() => [
  { name: 'Gyro X', data: gyroX.value },
  { name: 'Gyro Y', data: gyroY.value },
  { name: 'Gyro Z', data: gyroZ.value }
])

const altOptions = computed(() => buildOptions('alt', { title: { text: 'Meters' }, tickAmount: 4 }, ['#ffff33', '#00ffcc']))
const altSeries = computed(() => [
  { name: 'Baro Altitude', data: baroAlt.value },
  { name: 'GPS Altitude', data: gpsAlt.value }
])

const gpsOptions = computed(() => buildOptions('gps', [
  { seriesName: 'Satellites', title: { text: 'Satellites' }, min: 0, tickAmount: 4 },
  { opposite: true, seriesName: 'PDOP', title: { text: 'PDOP' }, min: 0, tickAmount: 4 }
], ['#ff66ff', '#ffffff']))
const gpsSeries = computed(() => [
  { name: 'Satellites', data: satsHistory.value },
  { name: 'PDOP', data: pdopHistory.value }
])

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
    
    for (let i = 1; i < lines.length; i++) {
      if (!lines[i].trim()) continue
      const cols = lines[i].split(',')
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
      
      const timeLabel = new Date().toLocaleTimeString('en-US', { hour12: false })
      timeHistory.value.push(timeLabel)
      accelX.value.push(data.accel_x || 0); accelY.value.push(data.accel_y || 0); accelZ.value.push(data.accel_z || 0)
      gyroX.value.push(data.gyro_x || 0); gyroY.value.push(data.gyro_y || 0); gyroZ.value.push(data.gyro_z || 0)
      baroAlt.value.push(data.baro_alt || 0); gpsAlt.value.push(data.gps_alt || 0)
      pdopHistory.value.push(data.pdop || 0); satsHistory.value.push(data.satellites || 0)
      
      if (data.lat && data.lng) {
        updateMapPath([data.lat], [data.lng]) // Note: Live mode needs proper path appending
      }
      
      const MAX_PTS = 60
      if (timeHistory.value.length > MAX_PTS) {
        timeHistory.value.shift()
        accelX.value.shift(); accelY.value.shift(); accelZ.value.shift()
        gyroX.value.shift(); gyroY.value.shift(); gyroZ.value.shift()
        baroAlt.value.shift(); gpsAlt.value.shift()
        pdopHistory.value.shift(); satsHistory.value.shift()
      }
    }
  })
})
</script>
