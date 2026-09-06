# 📁 Kukang EV Telemetry WebApp

Ini adalah direktori berharga yang menaungi perangkat lunak antarmuka *front-end* (UI/UX) modern berbasis *Web*. Aplikasi canggih ini dirancang khusus dari nol menggunakan kerangka kerja **Vue.js 3** (Composition API) mutakhir, dan dikompilasi menggunakan ekosistem **Vite** demi pengalaman performa pengembangan dan eksekusi (*runtime*) yang ngebut dan instan.

Aplikasi ini beraksi layaknya "Dasbor Pusat Kendali" visual yang sangat elegan, menyajikan rentetan parameter saintifik balapan menjadi antarmuka yang sangat indah, futuristik, dan sarat makna bagi para *Race Engineer* tim Kukang EV.

## ✨ Ekosistem Visual & Fitur-Fitur Vital

1. **Dashboard UI/UX Interaktif:** Dirancang dengan estetika tema gelap, *glassmorphism*, indikator status koneksi yang "berdenyut", dan palet neon warna-warni yang membantu penglihatan *paddock engineer* untuk segera mengetahui *trend* mesin dalam sekilas pandang walau sedang berada di sirkuit bercuaca panas.
2. **Dynamic Charting (ApexCharts):** Merender deretan *line-chart* performa tinggi yang mengelompokkan variabel data (Akselerasi G-Force, Giroskop/Sudut Bodi, Ketinggian GPS vs Barometer, Satelit & Kualitas Sinyal, hingga Kecepatan Laju Mobil).
3. **Peta Integrasi (Leaflet JS):** Menggambar lintasan aktual mobil di atas peta topografi satelit (OpenTopoMap).
4. **Sinkronisasi Sumbu *Cross-Chart* (2-Arah):** Saat *mouse* Anda menyorot (*hover*) pada detik tertentu di grafik kecepatan, **semua grafik lainnya** akan ikut menunjukkan garis ukur pada detik yang persis sama. Lebih dari itu, sebuah "Kursor Biru" virtual akan hidup di *Leaflet Map* dan bergerak menelusuri tepat di jalan mana (koordinat absolut) kondisi detik tersebut dicatat! Fitur sinkronisasi hibrida 2-Arah ini melahirkan kemampuan bedah data yang fenomenal.
5. **Algoritma Telemetri Luring & Daring:**
   - **Mode Live (*Real-time*):** Mendengarkan *socket* koneksi langsung ke **Firebase IoT** (yang dipancarkan oleh *ESP32 Co-MCU*). Kru dapat mengawasi nyawa mobil di tengah lintasan secara *live*.
   - **Mode Offline (Log Parser):** Mengizinkan *upload* file hasil rekaman MicroSD biner (`.bin`) maupun teks (`.csv`). Walaupun file rekamannya raksasa (mencapai jutaan bit), arsitektur anti-*crash* pada WebApp ini memakai teknik *Smart Downsampling* serta *shallowRef Reactivity Bypass* pada Vue 3. Tujuannya agar browser tidak pernah *hang/crash* saat melukis grafik padat (*out-of-memory avoidance*).
   - Menghitung Kecepatan Mobil secara mandiri (dalam Satuan km/h) menggunakan algoritma derivasi *Haversine Distance* berbasis perbedaan delta Waktu.

## 🚀 Cara Menjalankan Server
Pastikan ekosistem **Node.js (NPM)** sudah terpasang di komputer Anda.
1. Buka antarmuka baris perintah (*Terminal* / *Command Prompt*) dan arahkan (*cd*) masuk ke folder ini.
2. Unduh semua paket *library node_modules*: `npm install`
3. Nyalakan mesin penyaji lokal: `npm run dev`
4. Tekan tautan IP / `localhost:5173` yang muncul dan operasikan lewat Google Chrome / Edge untuk kinerja peramban terbaik.
