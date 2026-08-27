# Shell Eco-Marathon: Kukang EV Motor Driver & Electronics

Repository ini merupakan repositori utama pengembangan sistem elektronika daya (*power electronics*) dan *firmware motor controller* untuk mobil listrik hemat energi **Kukang EV** pada kompetisi **Shell Eco-Marathon**.

Proyek ini mencakup desain skematik hardware PCB inverter 3-fasa berdaya tinggi (**Behemoth**) serta dua varian implementasi firmware berbasis mikrokontroler keluarga STM32G4 (*Arm® Cortex®-M4 dengan CORDIC & High-Resolution Timer*).

---

## 📂 Struktur Direktori Utama

Repositori ini terbagi menjadi 3 folder utama pada direktori root:

```
Shell-Eco-Marathon-Kukang-EV/
├── 📁 PCB/                                  # Desain Hardware & Skematik Motor Driver Behemoth
├── 📁 STM32G431CBU6-kukang-hall-sensor/     # Firmware BLDC/FOC (STM32G431CBU6 + Hall Sensor)
├── 📁 STM32G474RBT3-kukang-driver-behemoth/ # Firmware Utama Dedicated Board Behemoth (STM32G474RBT3)
└── 📄 readme.md                             # Dokumentasi utama proyek (file ini)
```

---

## 🔍 Penjelasan Folder pada Root

### 1. 📁 [`PCB/`](PCB/readme.md) — *Hardware Design & Skematik Driver Behemoth*
Folder ini berisi file proyek desain skematik EasyEDA Pro (`.epro2`) dan dokumen PDF skematik resmi untuk board inverter daya tinggi **Shell Eco Marathon Behemoth**.

- **File Utama:**
  - [`SCH_SCH Shell Eco Marathon Behemoth_2026-08-27.pdf`](PCB/SCH_SCH%20Shell%20Eco%20Marathon%20Behemoth_2026-08-27.pdf): Skematik lengkap 8 halaman (Power Stage, STM32 MCU, Sensor, Interface, Power Supply).
  - [`ProPrj_ebike-controller_copy_2026-08-27.epro2`](PCB/ProPrj_ebike-controller_copy_2026-08-27.epro2): File proyek CAD EasyEDA Pro.
  - [`readme.md`](PCB/readme.md): Penjelasan teknis detail komponen, rumus sensing, rating daya, dan konektor eksternal.
- **Spesifikasi Kunci Hardware:**
  - **Tegangan Operasi:** 48V – 60V DC (*Max Limit 100V DC*).
  - **Arus:** 50A RMS kontinu, 120A *peak* (< 5 detik).
  - **Power Stage:** 6x MOSFET Infineon `IPT015N10N5` (100V, 1.5 mΩ, TOLL).
  - **Gate Driver:** Texas Instruments `DRV8353SR` (100V Smart Gate Driver dengan antarmuka SPI & 3 Shunt Amplifiers).
  - **Regulator Daya:** TI `LM5161PWPR` 100V Synchronous Buck (5V) + `AMS1117-3.3` (3.3V).
  - **Komunikasi & Sensor:** CAN Bus (`SN65HVD230`), RS-485 Modbus (`THVD1420`), Differential Line Receiver (`TPT480L1`), DB25 Sensor Interface, dan RJ45 Daisy-Chain.

---

### 2. 📁 [`STM32G431CBU6-kukang-hall-sensor/`](STM32G431CBU6-kukang-hall-sensor/readme.md) — *Firmware Hall Sensor & Vector Control*
Folder ini berisi proyek firmware berbasis mikrokontroler **STM32G431CBU6** (UFQFPN48 @ 170 MHz) yang dioptimalkan untuk pengujian motor BLDC / Hub Motor E-Bike.

- **Fitur & Kemampuan Firmware:**
  - **Algoritma Ganda:** Mendukung **Six-Step Commutation** (komutasi 6-sektor Hall 120°) dan **Field-Oriented Control (FOC)** yang dapat dipilih via konfigurasi.
  - **Akselerasi Trigonometri CORDIC:** Menggunakan hardware co-processor STM32G4 untuk perhitungan sinus/kosinus instan (< 6 siklus CPU).
  - **State Current Observer:** Estimator arus berbasis *Gradient Descent (PLL)* untuk merekonstruksi fasa arus sinus kontinu dari sensor half-wave (BTS7960/Shunt).
  - **Antarmuka CLI Bergaya GRBL:** Pengaturan parameter motor (`$0`–`$26`) via USB Virtual COM Port (CDC).
  - **Penyimpanan Flash Non-Volatile:** Parameter tersimpan permanen pada Flash Page 63.
  - **Telemetri Biner Real-Time:** Pengiriman data kecepatan tinggi 33-byte untuk visualisasi kurva di GUI.

---

### 3. 📁 [`STM32G474RBT3-kukang-driver-behemoth/`](STM32G474RBT3-kukang-driver-behemoth/readme.md) — *Firmware Dedicated Board Behemoth*
Folder ini berisi proyek firmware yang secara khusus disesuaikan dengan mapping pinout dan periferal board kustom **Behemoth** berbasis mikrokontroler **STM32G474RBT3** (LQFP64 @ 170 MHz).

- **Integrasi Hardware Firmware:**
  - **Advanced Timer (TIM1):** Konfigurasi 6 output PWM independen (*High-Side* & *Low-Side* terdedikasi) untuk driver `DRV8353SR`.
  - **Triple High-Speed ADC (ADC1, ADC2, ADC3):** Pengambilan sampel arus 3-fasa simultan (`STM32_CUR_A/B/C`) dan pengukuran tegangan 4-kanal (Fasa A, B, C & Bus Voltage $V_{BUS}$).
  - **SPI3 Interface:** Komunikasi digital untuk konfigurasi register proteksi, dead-time, dan gain amplifier pada `DRV8353SR`.
  - **TIM2 Interface:** Pembacaan 3x sinyal Hall Sensor (`PA0`, `PA1`, `PA2`) dan input encoder diferensial.
  - **Konektivitas Industri:** Dukungan periferal hardware untuk FDCAN1, UART Modbus, dan USB Type-C CDC.

---

## 📊 Matriks Perbandingan Modul

| Parameter | [`PCB`](PCB/readme.md) | [`STM32G431CBU6-kukang-hall-sensor`](STM32G431CBU6-kukang-hall-sensor/readme.md) | [`STM32G474RBT3-kukang-driver-behemoth`](STM32G474RBT3-kukang-driver-behemoth/readme.md) |
| :--- | :--- | :--- | :--- |
| **Kategori** | Hardware Schematic & CAD | Firmware (General / Hub Motor) | Firmware (Dedicated Behemoth Board) |
| **Target MCU** | STM32G474RBT3 | STM32G431CBU6 | STM32G474RBT3 |
| **Kemasan MCU** | LQFP64 | UFQFPN48 | LQFP64 |
| **Gate Driver** | TI DRV8353SR (SPI) | Generic 3-Half Bridge / BTS7960 | TI DRV8353SR (SPI) |
| **Arus Sensing** | 3x Low-Side Shunt (7 mΩ) | 2/3 Shunt ADC / Half-Wave | 3x Shunt + Triple ADC Simultan |
| **Algoritma** | - | Six-Step & CORDIC FOC | Advanced FOC & HRTIM Support |
| **Antarmuka Sensor**| DB25 (Encoder ABZ, SPI, Hall, NTC) | TIM2 Hall & I2C AS5600 | DB25 Integrated Routing |

---

## 🛠️ Toolchain & Prasyarat Pengembangan

1. **Hardware Design:**
   - [EasyEDA Pro Edition](https://pro.easyeda.com/) untuk membuka dan memodifikasi file `.epro2`.
2. **Firmware Development:**
   - **STM32CubeIDE** (v1.14.0 atau lebih baru).
   - **STM32CubeMX** (v6.18.0) dengan firmware package `STM32Cube FW_G4 V1.6.3`.
   - **ARM GCC Toolchain** & **ST-Link V2 / V3 Debugger**.
3. **Monitoring & Telemetri:**
   - Serial Terminal (PuTTY, Tera Term, Serial Studio) untuk antarmuka CLI USB CDC.

---

## 🚀 Panduan Memulai (Quick Start)

1. **Mempelajari Desain Hardware:**
   Buka dokumentasi skematik pada [`PCB/readme.md`](PCB/readme.md) untuk memahami jalur daya, koneksi sensor pada DB25, dan proteksi board.
2. **Menjalankan Firmware:**
   - Untuk motor hub / evaluasi fasa hall, buka project di [`STM32G431CBU6-kukang-hall-sensor/`](STM32G431CBU6-kukang-hall-sensor/readme.md).
   - Untuk board inverter lengkap Behemoth, gunakan project di [`STM32G474RBT3-kukang-driver-behemoth/`](STM32G474RBT3-kukang-driver-behemoth/readme.md).
3. **Build & Flash:**
   Impor proyek ke **STM32CubeIDE** (`File` $\rightarrow$ `Open Projects from File System...`), compile (`Ctrl + B`), dan flash menggunakan ST-Link ke target MCU.

---
*Dikembangkan dengan standar efisiensi tinggi untuk Tim Shell Eco-Marathon Kukang EV.*

