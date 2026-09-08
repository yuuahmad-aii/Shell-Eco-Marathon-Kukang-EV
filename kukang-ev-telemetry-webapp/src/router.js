import { createRouter, createWebHistory } from 'vue-router'
import VehicleTelemetry from './components/VehicleTelemetry.vue'
import MotorTelemetry from './components/MotorTelemetry.vue'

const routes = [
  { path: '/', redirect: '/vehicle-telemetry' },
  { path: '/vehicle-telemetry', component: VehicleTelemetry },
  { path: '/motor-telemetry', component: MotorTelemetry }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router
