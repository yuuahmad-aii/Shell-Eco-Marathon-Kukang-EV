<template>
  <div>
    <!-- Sidebar Overlay -->
    <div v-if="isOpen" class="sidebar-overlay" @click="closeSidebar"></div>
    
    <!-- Sidebar -->
    <div class="sidebar" :class="{ 'sidebar-open': isOpen }">
      <div class="sidebar-header">
        <h2>Menu</h2>
        <button class="close-btn" @click="closeSidebar">✖</button>
      </div>
      <div class="sidebar-content">
        <h3>Telemetry Modes</h3>
        <router-link to="/vehicle-telemetry" class="tab-btn" style="width: 100%; margin-bottom: 10px; text-align: left; display: block; text-decoration: none;" active-class="active" @click="closeSidebar">🌍 Vehicle Telemetry</router-link>
        <router-link to="/motor-telemetry" class="tab-btn" style="width: 100%; margin-bottom: 30px; text-align: left; display: block; text-decoration: none;" active-class="active" @click="closeSidebar">⚡ Motor Telemetry</router-link>
        
        <div class="sidebar-info">
          <h3>Web App Info</h3>
          <p><strong>App Name:</strong> Kukang EV Telemetry Dashboard</p>
          <p><strong>Stack:</strong> Vue 3, Vite, ApexCharts, Leaflet, Web Serial API</p>
          <p><strong>Authors:</strong> yuuahmad + Gemini AI</p>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
const props = defineProps({
  isOpen: Boolean
})

const emit = defineEmits(['update:isOpen'])

const closeSidebar = () => {
  emit('update:isOpen', false)
}
</script>

<style scoped>
.sidebar-overlay {
  position: fixed;
  top: 0; left: 0; right: 0; bottom: 0;
  background: rgba(0, 0, 0, 0.5);
  z-index: 999;
}
.sidebar {
  position: fixed;
  top: 0; left: -300px; right: auto;
  width: 300px; height: 100vh;
  background: #1e293b;
  box-shadow: 2px 0 10px rgba(0,0,0,0.5);
  z-index: 1000;
  transition: left 0.3s ease;
  display: flex; flex-direction: column;
}
.sidebar-open {
  left: 0;
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

.tab-btn { padding: 8px 16px; background: var(--secondary); color: white; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; }
.tab-btn.active { background: var(--accent-primary); color: #000; }
</style>
