<template>
  <div class="dashboard-container">
    <header class="header">
      <div style="display: flex; align-items: center; gap: 15px;">
        <button class="file-upload-btn" @click="isSidebarOpen = true" style="background-color: var(--accent-primary); font-size: 14px; padding: 6px 12px;">
          ☰ Menu
        </button>
        <div class="logo">
          <h1>🏎️ Kukang EV <span>Telemetry Analysis</span></h1>
        </div>
      </div>
      
      <div class="header-controls" id="header-controls-target">
      </div>
    </header>

    <Sidebar v-model:isOpen="isSidebarOpen" />

    <router-view v-slot="{ Component }">
      <keep-alive>
        <component :is="Component" />
      </keep-alive>
    </router-view>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import Sidebar from './components/Sidebar.vue'

var isSidebarOpen = ref(false)
</script>

<style>
/* Global Resets & Theme */
* {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}
:root {
  --bg-dark: #0f172a;
  --panel-bg: #1e293b;
  --accent-primary: #38bdf8;
  --secondary: #64748b;
  --warning: #f59e0b;
  --danger: #ef4444;
  --text-main: #f8fafc;
  --border: #334155;
}
body {
  font-family: 'Inter', system-ui, -apple-system, sans-serif;
  background-color: var(--bg-dark);
  color: var(--text-main);
  overflow-y: hidden;
}

/* Dashboard Layout */
.dashboard-container {
  display: flex;
  flex-direction: column;
  height: 100vh;
  padding: 10px;
  gap: 10px;
}
.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  background: var(--panel-bg);
  padding: 10px 20px;
  border-radius: 8px;
  border: 1px solid var(--border);
}
.logo {
  display: flex;
  align-items: center;
  gap: 10px;
}
.logo h1 {
  font-size: 1.25rem;
  font-weight: 700;
  color: #fff;
  margin: 0;
}
.logo span {
  font-weight: 400;
  color: var(--secondary);
  font-size: 1rem;
}
.header-controls {
  display: flex;
  gap: 10px;
  align-items: center;
}
.file-upload-btn {
  background: var(--accent-primary);
  color: #000;
  border: none;
  padding: 8px 16px;
  border-radius: 4px;
  font-weight: 600;
  cursor: pointer;
  transition: opacity 0.2s;
  display: inline-flex;
  align-items: center;
  justify-content: center;
}
.file-upload-btn:hover {
  opacity: 0.9;
}
.status {
  padding: 6px 12px;
  border-radius: 20px;
  font-size: 0.85rem;
  font-weight: 600;
}
.status-connected {
  background: rgba(16, 185, 129, 0.2);
  color: #10b981;
  border: 1px solid #10b981;
}
.status-disconnected {
  background: rgba(239, 68, 68, 0.2);
  color: #ef4444;
  border: 1px solid #ef4444;
}

/* Grids */
.dashboard-grid {
  display: grid;
  grid-template-columns: 350px 1fr;
  gap: 10px;
  flex: 1;
  min-height: 0;
}
.chart-card {
  background: var(--panel-bg);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 15px;
  display: flex;
  flex-direction: column;
}
</style>