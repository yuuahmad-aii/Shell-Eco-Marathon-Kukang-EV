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
        <label class="file-upload-btn" style="background-color: var(--secondary); color: #fff;">
          Convert .bin to .csv
          <input type="file" accept=".bin" @change="convertBinToCsv" hidden />
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
</template>

<script setup>
import { ref, shallowRef, onMounted, computed, nextTick } from 'vue'
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

const gpsOptions = buildOptions('gps', [
  { seriesName: 'Satellites', title: { text: 'Satellites' }, min: 0, tickAmount: 4 },
  { opposite: true, seriesName: 'PDOP', title: { text: 'PDOP' }, min: 0, tickAmount: 4 }
])
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
      const t = data.ts ? (data.ts / 1000) : (Date.now() / 1000)
      
      // Calculate speed
      let speed = 0
      if (liveLats.value.length > 0 && liveLons.value.length > 0 && timeHistory.value.length > 0) {
         // Find the last valid GPS point to compute delta distance
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
      
      // Simpan history GPS untuk menggambar garis lintasan dan melacak titik hover
      liveLats.value = [...liveLats.value, data.lat || 0].slice(-MAX_PTS)
      liveLons.value = [...liveLons.value, data.lon || 0].slice(-MAX_PTS)
      updateMapPath(liveLats.value, liveLons.value)
      
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
