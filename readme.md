# Shell Eco-Marathon: Kukang EV Electronics & Software Ecosystem

Repository ini merupakan repositori utama (*monorepo*) pengembangan seluruh sistem elektronika, telemetri, *firmware motor controller*, dan perangkat lunak visualisasi untuk mobil listrik hemat energi **Kukang EV** dalam ajang kompetisi **Shell Eco-Marathon**.

Proyek ini sangat ekstensif, mencakup desain perangkat keras PCB berdaya tinggi (inverter motor), *firmware* kontrol motor BLDC, mikrokontroler pendamping (*co-mcu*), sistem perekaman dan pengiriman data telemetri, antarmuka web, serta alat analisis data berbasis Python.

---

## 📂 Struktur Direktori Ekosistem

Repositori ini terbagi menjadi 6 folder utama pada direktori root, masing-masing menangani spesialisasi sistem yang berbeda:

```text
Shell-Eco-Marathon-Kukang-EV/
├── 📁 PCB/                                          # Desain Hardware, Skematik Motor Driver & Elektronik
├── 📁 STM32G431CBU6-kukang-hall-sensor-gate-driver/ # Firmware Utama Motor Controller BLDC/FOC (STM32G4)
├── 📁 STM32F446RE-kukang-telemetry/                 # Firmware Data Logger & Akuisisi Sensor (STM32F4)
├── 📁 ESP32C6-co-mcu/                               # Firmware Co-MCU untuk transmisi data IoT & Komunikasi
├── 📁 kukang-ev-telemetry-webapp/                   # WebApp Vue.js untuk Real-time Telemetry & Analisis Offline
└── 📁 python_gui/                                   # Script Python untuk plot grafik & analisis data (Offline)
```

---

## 🔍 Ringkasan Modul

Setiap folder memiliki fungsi spesifik dan dokumentasi `readme.md` terpisah yang sangat mendetail. Berikut adalah rangkuman singkat dari masing-masing modul:

### 1. [`PCB/`](PCB/readme.md)
Berisi file desain skematik dan layout PCB (menggunakan EasyEDA) untuk board *inverter* motor (*Behemoth* / *Kukang Driver*) beserta sirkuit pendukung elektronika lainnya. Desain berfokus pada efisiensi daya dan keandalan tinggi.

### 2. [`STM32G431CBU6-kukang-hall-sensor-gate-driver/`](STM32G431CBU6-kukang-hall-sensor-gate-driver/readme.md)
*Firmware* kritikal (*bare-metal/HAL*) untuk mikrokontroler STM32G431. Bertugas memutar dan mengatur komutasi motor BLDC dengan umpan balik sensor Hall dan sinyal PWM beresolusi tinggi, mengatur algoritma kontrol motor untuk efisiensi kelistrikan maksimum di lintasan.

### 3. [`STM32F446RE-kukang-telemetry/`](STM32F446RE-kukang-telemetry/readme.md)
*Firmware* papan Telemetri dan Data Logger Utama menggunakan STM32F446RE. Mengakuisisi seluruh data dari berbagai modul (Sensor IMU MPU9250, Barometer BMP280, modul GPS), mengolah data dari motor driver, dan merekam *log* dalam format biner (`.bin`) kecepatan tinggi secara lokal ke MicroSD. Modul ini menjadi otak dari *data logger*.

### 4. [`ESP32C6-co-mcu/`](ESP32C6-co-mcu/readme.md)
*Firmware* berbasis mikrokontroler cerdas ESP32-C6 (*Co-MCU*). Bertugas menjembatani dan menerima paket data telemetri yang telah disatukan oleh papan STM32F4, lalu mengirimkannya membelah udara langsung ke *cloud* (seperti platform Firebase) secara nirkabel agar keadaan mobil dapat dimonitor *real-time* oleh tim *pit stop*.

### 5. [`kukang-ev-telemetry-webapp/`](kukang-ev-telemetry-webapp/readme.md)
Dasbor kontrol dan analitik komprehensif berbasis **Vue 3, Vite, dan ApexCharts**. Ini adalah perangkat lunak *front-end* premium bagi kru *paddock*:
- **Live Mode:** Membaca dan menampilkan status mobil, kecepatan, GPS, G-Force secara langsung (*real-time*) dari koneksi Firebase.
- **Offline Mode:** Dapat menelan file *log* biner/CSV raksasa hasil rekaman MicroSD dan menampilkannya menjadi deretan grafik super halus (*downsampled*) dengan visualisasi sinkronisasi hover pada peta satelit (Leaflet).

### 6. [`python_gui/`](python_gui/readme.md)
Kumpulan skrip utilitas berbasis **Python** (menggunakan `matplotlib`, `struct`, dsb). Berguna sebagai jembatan *backend offline* untuk melakukan dekode (*parsing*) terhadap file *binary* mentah dari SD Card yang berisi data *struct* C agar dapat dibaca oleh manusia dalam format `.csv`, dan alat plot darurat jika analisis sederhana diperlukan tanpa menyalakan *server web*.




> $cal=2.0

==================================================
           HALL SENSOR CALIBRATION START          
===================================
===============
DC Bus Voltage :  V
C
al Voltage    :  V (Duty: %)
Pole Pairs     : 15
Current Offset :  deg

WARNING: Motor will rotate slowly in both directions.
Ensure wheel is compl
etely free to spin without load!

[1/3] Lo
cking rotor to Electrical 0 deg...
      Rotor locked. Pha
se Currents: U=A, V=A, W=A
[2/3] Sweeping Electrica
l Vector FORWARD (0 -> 720 deg)...
[3/3] Sweeping Electrica
l Vector REVERSE (720 -> 0 deg)...

--------------------------------------------------
                 CALIBRATION RESULT               
-
-------------------------------------------------
Hall Sequence: 3 -> 2 -> 6 -> 4 -> 5 -> 1
Direction    : NOR
MAL (Setting $14=0)

Sector Transitions (Electrical Degrees):
  State 5: Fwd=, Rev= -> Mid= deg (Ref=, Diff=)
  State 1: Fwd=, Rev= -> Mid= deg (Ref=, Diff=)
  State 3: Fwd=, Rev= -> Mid= deg (Ref=, Diff=)
  State 2: Fwd=, Rev= -> Mid= deg (Ref=, Diff=)
  State 6: Fwd=, Rev= -> Mid= deg (Ref=, Diff=)
  State 4: Fwd=, Rev= -> Mid= deg (Ref=, Diff=)

Calculated Hall Offset :  deg
Old Config Offset      :  deg

SUCCESS: motor_config.hall_offset_deg updated to  deg!
Type '$save' to permanently save this configuration to Flash.
==================================================
ok

