# Shell Eco-Marathon: Kukang EV Electronics & Software Ecosystem

Repository ini merupakan repositori utama (*monorepo*) pengembangan seluruh sistem elektronika daya, telemetri nirkabel, *firmware motor controller*, antarmuka kokpit pengemudi, dan perangkat lunak analitik untuk mobil listrik hemat energi **Kukang EV** dalam ajang kompetisi internasional **Shell Eco-Marathon**.

Proyek ini terintegrasi secara modular, mencakup perancangan papan sirkuit cetak PCB inverter daya tinggi, algoritma kontrol motor BLDC berkecepatan tinggi, sistem fusi sensor dan perekam log biner MicroSD, gerbang transmisi awan IoT Wi-Fi 6 / HTTPS, dashboard web interaktif (*real-time & offline analysis*), serta utilitas analisis berbasis Python.

---

## 🏗️ Arsitektur Sistem Keseluruhan

Ekosistem elektronika Kukang EV dibangun di atas **tiga mikrokontroler terspesialisasi (*Tri-MCU Architecture*)** yang saling terhubung melalui protokol deterministik berkeandalan tinggi:

```mermaid
graph TD
    subgraph Kokpit Kendaraan
        Driver((Pengemudi)) -->|Tekan S1 Throttle / S2-S3 Set RPM| TM1638[Dashboard Modul TM1638\n8 Tombol Tekan, 8 LED, 8-Digit 7-Segment]
        TM1638 <-->|SPI2 + DMA1 Stream 4\nPB12 CS, PB13 SCK, PB15 DIO| F4[STM32F446RE: Telemetry Hub & Sensor Fusion\nCore M4 @ 180 MHz]
    end

    subgraph Sensor Fisik Kendaraan
        PA0PA1[Sensor Pulsa Roda PA0/PA1] -->|TIM2 CH1 & TIM5 CH2| F4
        MPU[IMU 9-DOF MPU9250] -->|SPI1 CS PC5| F4
        BMP[Barometer BMP280] -->|SPI1 CS PC4| F4
        GPS[GPS Neo-6M / 7M] -->|USART1 UBX 115200| F4
        DS[Dual Suhu DS18B20] -->|1-Wire PB10 + TIM14| F4
    end

    subgraph Powertrain & Inverter
        F4 <-->|CAN Bus @ 1 Mbps\nCAN ID 0x10 Throttle Cmd\nCAN ID 0x20 Status 100ms| G4[STM32G431CBU6: BLDC Motor Controller\nCore M4 @ 170 MHz + CORDIC]
        G4 -->|TIM1 6x PWM Complementary + Deadtime| Inverter[Inverter 3-Phase MOSFETs]
        Inverter --> BLDC((Motor BLDC))
        BLDC -->|Umpan Balik Hall A, B, C| G4
        Inverter -->|Sensing Arus U, V, W & VBUS| G4
    end

    subgraph Perekaman Data Lokal
        F4 -->|SDIO 4-Bit Bus @ 24 MHz\n64-Byte Biner logX.bin| SDCard[(Kartu MicroSD)]
    end

    subgraph Jembatan IoT Nirkabel
        F4 -->|USART2 JSON Stream @ 2 Hz\nPA2 TX -> GPIO 4 RX| C6[ESP32-C6: IoT Cloud Gateway\nRISC-V @ 160 MHz + Wi-Fi 6]
        C6 -->|HTTPS PUT Persistent TLS\nLatency ~150ms| Firebase[(Firebase Realtime Database)]
    end

    subgraph Pit Stop Paddock
        Firebase <-->|Websocket / Live Mode| WebApp[kukang-ev-telemetry-webapp\nVue 3 + ApexCharts + Leaflet]
        SDCard -.->|Offline Mode (.bin / .csv)| WebApp
        SDCard -.->|Offline Parsing & Quick Plot| PyGUI[python_gui / decode_log.py]
    end
```

---

## 📂 Struktur Direktori Ekosistem

Repositori ini terbagi menjadi 6 sub-sistem utama dengan peran teknis masing-masing:

```text
Shell-Eco-Marathon-Kukang-EV/
├── 📁 PCB/                                          # Skematik & Layout Hardware Inverter EasyEDA
├── 📁 STM32G431CBU6-kukang-hall-sensor-gate-driver/ # Firmware Komutasi Motor BLDC & FOC Sensing
├── 📁 STM32F446RE-kukang-telemetry/                 # Firmware Telemetry Hub, Datalogger & Dashboard TM1638
├── 📁 ESP32C6-co-mcu/                               # Firmware IoT Gateway Wi-Fi 6 / HTTPS Firebase
├── 📁 kukang-ev-telemetry-webapp/                   # WebApp Vue 3 Paddock Monitor (Live & Offline Replay)
└── 📁 python_gui/                                   # Skrip Python Dekoder Biner SD Card & Quick Plot
```

---

## 🔍 Ringkasan Sub-Sistem

### 1. [`PCB/`](PCB/readme.md)
Berisi desain skematik dan berkas manufaktur PCB (*Gerber*) untuk board inverter motor berdaya tinggi (*Behemoth Inverter* / *Kukang Driver*) beserta modul elektronika pendukung kendaraan. Desain difokuskan pada impedansi jalur fasa yang sangat rendah, disipasi panas optimal, serta proteksi derau elektromagnetik (EMI).

### 2. [`STM32G431CBU6-kukang-hall-sensor-gate-driver/`](STM32G431CBU6-kukang-hall-sensor-gate-driver/readme.md)
*Firmware bare-metal* kontroler motor BLDC:
- **Penggerak:** Komutasi *Six-Step* dengan modulasi PWM komplementer berfrekuensi 20 kHz (`TIM1`) dilengkapi *hardware dead-time* 136 siklus.
- **Sensing:** Pengukuran arus fasa fasa motor $U, V, W$ via ADC Injected disinkronkan tepat di tengah sinyal PWM, dikonversi menggunakan transformasi Clarke & Park untuk memantau arus torsi ($I_q$).
- **Interkoneksi:** Terhubung ke bus CAN (FDCAN1 @ 1 Mbps) untuk menerima perintah throttle (`CAN ID 0x10`) dan mengirim telemetri status daya (`CAN ID 0x20`) setiap 100 ms.
- **Keamanan:** Dilengkapi *Safety Watchdog* yang otomatis mematikan motor jika komunikasi perintah CAN terhenti lebih dari 350 ms.

### 3. [`STM32F446RE-kukang-telemetry/`](STM32F446RE-kukang-telemetry/readme.md)
*Firmware* pusat akuisisi data dan antarmuka kokpit pengemudi:
- **Antarmuka Kokpit:** Mengendalikan modul **TM1638** (8 tombol, 8 LED, 8 digit 7-segmen) via SPI2 DMA (20 Hz) untuk input gas, setelan kecepatan, pergantian menu sensor, dan indikator alarm efisiensi.
- **Sensor Fusion:** Membaca IMU 9-DOF MPU9250 (SPI1), Barometer BMP280 (SPI1), GPS Neo-6M protokol UBX (USART1), sensor digital suhu mesin DS18B20 (PB10 1-Wire), dan sensor pulsa kecepatan roda independen (PA0 / PA1).
- **Kalkulasi Efisiensi:** Menghitung jarak kumulatif dan integrasi konsumsi energi listrik ($P = V_{bus} \times I_q$) secara *real-time* untuk menampilkan metrik efisiensi $km/\text{kWh}$ di layar pengemudi.
- **Datalogger Biner:** Merekam struktur data 64-byte padat ke MicroSD via SDIO 4-bit (20 Hz) dengan proteksi *auto-recovery* saat guncangan.
- **IoT Feeder:** Memancarkan string telemetri JSON ringkas ke ESP32-C6 via USART2 setiap 500 ms (2 Hz).

### 4. [`ESP32C6-co-mcu/`](ESP32C6-co-mcu/README.md)
*Firmware* gerbang komunikasi nirkabel berbasis ESP-IDF & FreeRTOS:
- **Arsitektur Dual-Task:** `uart_rx_task` (prioritas tertinggi) menerima dan memvalidasi JSON serial dari STM32F4; `firebase_tx_task` memancarkan data ke awan.
- **Latensi Rendah:** Menggunakan sesi *Persistent HTTPS* (`keep_alive_enable = true`) sehingga jabat tangan TLS tidak dinegosiasikan ulang pada setiap pengiriman, menurunkan latensi pengiriman dari ~2500 ms menjadi hanya **~150 ms**.
- **Sinkronisasi SNTP:** Memperoleh acuan waktu dari `pool.ntp.org` untuk validasi sertifikat SSL/TLS.
- **Ketahanan Jaringan:** Fitur *auto-reconnect* otonom menjamin pengiriman kembali stabil saat mobil keluar dari area tanpa sinyal (*tunnel/blind spot*).

### 5. [`kukang-ev-telemetry-webapp/`](kukang-ev-telemetry-webapp/readme.md)
Aplikasi dasbor pemantauan berbasis **Vue 3, Vite, ApexCharts, dan Leaflet**:
- **Live Mode:** Menampilkan kecepatan instan, arus torsi, tegangan baterai, status satelit, visualisasi G-Force, serta pelacakan titik koordinat GPS mobil pada peta satelit secara *live* langsung dari Firebase RTDB.
- **Offline Analysis Mode:** Mampu memproses file log biner (`.bin`) raksasa dari MicroSD untuk visualisasi grafik multi-parameter tersinkronisasi dengan hover koordinat lintasan.

### 6. [`python_gui/`](python_gui/readme.md)
Perangkat bantu diagnostik berbasis **Python**:
- Skrip dekoder biner untuk mengekstrak file `logX.bin` dari MicroSD menjadi berkas tabel `.csv`.
- Utilitas visualisasi grafik cepat (*quick-plot*) menggunakan `matplotlib` untuk evaluasi performa darurat di area paddock tanpa membutuhkan server web.

---

## 🔌 Matriks Interkoneksi Fisik Lintas Board

Tabel berikut merangkum kabel penghubung utama antar modul dan mikrokontroler di kendaraan:

| Dari Modul / Pin | Ke Modul / Pin | Protokol & Kecepatan | Deskripsi Fungsi |
| :--- | :--- | :--- | :--- |
| **STM32F446RE** PB8 (RX), PB9 (TX) | **STM32G431** PB9 (TX), PB8 (RX) | **CAN Bus** (1.000.000 bps) | Jalur perintah throttle motor (`0x10`) & umpan balik status daya/arus motor (`0x20`). |
| **STM32F446RE** PA2 (TX), PA3 (RX) | **ESP32-C6** GPIO 4 (RX), GPIO 5 (TX) | **Serial UART** (115.200 bps) | Pengiriman data telemetri string JSON dari data logger ke gateway IoT nirkabel. |
| **STM32F446RE** PB12 (CS), PB13 (SCK), PB15 (DIO) | **Modul TM1638** STB, CLK, DIO | **SPI2 + DMA** (20 Hz) | Kendali layar 8-digit 7-segmen, 8 lampu LED indikator, dan pembacaan 8 tombol kokpit. |
| **STM32F446RE** PA5 (SCK), PA6 (MISO), PA7 (MOSI) | **MPU9250** (CS: PC5) & **BMP280** (CS: PC4) | **SPI1 Bus** | Akuisisi data akselerasi 3-sumbu, giroskop 3-sumbu, tekanan udara, dan ketinggian. |
| **STM32F446RE** PA9 (TX), PA10 (RX) | **Modul GPS Neo-6M** RX, TX | **USART1** (115.200 bps) | Penerimaan koordinat lintang/bujur, kecepatan satelit, fix status, dan waktu UTC. |
| **STM32F446RE** PB10 | **Dual Sensor DS18B20** DQ | **1-Wire Bus** | Pemantauan temperatur digital suhu motor listrik dan inverter secara independen. |
| **STM32F446RE** PA0, PA1 | **Sensor Kecepatan Roda** Out Kanan/Kiri | **Timer Input Capture** | Pengukuran pulsa rotasi roda kiri dan kanan untuk perhitungan kecepatan dan jarak. |
| **STM32G431** PA0, PA1, PA2 | **Sensor Hall BLDC** U, V, W | **Timer Hall Mode** | Pendeteksian posisi kutub magnet rotor untuk menentukan transisi sektor komutasi fasa. |
| **STM32G431** PA8-PA10, PB13-PB15 | **Driver MOSFET Inverter** H/L U, V, W | **TIM1 PWM** (20 kHz) | Penggerak gerbang switching 6 transistor MOSFET inverter daya 3-fasa. |

---

## 🏁 Strategi Balapan: "Burn and Coast" Kukang EV

Dalam ajang Shell Eco-Marathon, mobil listrik tidak dikemudikan dengan menekan pedal gas secara konstan, melainkan menggunakan teknik hemat energi **Burn and Coast**:

```text
       Kecepatan (km/h)
          ^
$21: 30 --+        /\                /\                /\
          |       /  \              /  \              /  \
          |      /    \            /    \            /    \
$20: 18 --+     /      \          /      \          /      \
          |    /        \        /        \        /        \
          |   /          \______/          \______/          \______
          |  [  BURN  ]  [ COAST ]        [  BURN  ]  [ COAST ]
          +--------------------------------------------------------> Waktu
              (Gas S1)    (Luncur)         (Gas S1)    (Luncur)
```

1. **Fase Akselerasi (*Burn Phase*):**
   - Pengemudi menekan tombol **S1 (Throttle)**. Motor BLDC bekerja pada titik efisiensi tertingginya memacu kendaraan dari batas bawah `$20` (misal 18 km/h) hingga mencapai kecepatan optimal `$21` (misal 30 km/h).
   - Display kokpit menampilkan `A  <RPM>` dan **LED 1 menyala**.
2. **Fase Meluncur (*Coast Phase*):**
   - Pengemudi melepas tombol **S1**. Inverter segera mematikan sinyal PWM ke seluruh MOSFET (*free-wheeling / coasting*), membiarkan kendaraan meluncur bebas memanfaatkan inersia massa tanpa beban pengereman regeneratif yang tidak perlu. Konsumsi daya motor turun menjadi 0 Watt.
3. **Peringatan Batas Bawah (*Coast Alert Warning*):**
   - Saat kecepatan mobil melambat mendekati batas kritis `$20` (`coast_speed_min`), **LED 7 pada TM1638 akan berkedip cepat (4 Hz)**. Ini adalah sinyal visual bagi pengemudi untuk kembali menekan tombol **S1** dan memulai fase *burn* berikutnya.
4. **Pemantauan Efisiensi Langsung ($km/\text{kWh}$):**
   - Pada garis start, pengemudi menekan **S5** untuk memulai logging dan mereset akumulator jarak/energi. Sepanjang balapan, pengemudi dapat menekan **S4** ke sub-mode 5 untuk memantau angka efisiensi langsung `E  xxx.x` ($km/\text{kWh}$) guna menyesuaikan gaya berkendara terhadap target strategi tim pit stop.
