# STM32G431 Motor Controller (Kukang EV - Hall Sensor)

Repository ini berisi firmware motor controller berbasis mikrokontroler **STM32G431CBU6** untuk kendaraan hemat energi **Shell Eco-Marathon (Kukang EV)**. Firmware ini dirancang untuk mengendalikan motor BLDC / Hub Motor E-Bike menggunakan sensor Hall dengan dukungan algoritma **Six-Step (Trapezoidal Commutation)** maupun **Field-Oriented Control (FOC)** dengan akselerasi hardware CORDIC.

---

## 📋 Daftar Isi
- [Spesifikasi Hardware & Sistem](#-spesifikasi-hardware--sistem)
- [Konfigurasi Pinout (Mapping Pin)](#-konfigurasi-pinout-mapping-pin)
- [Arsitektur & Algoritma Kontrol](#-arsitektur--algoritma-kontrol)
  - [1. Six-Step Commutation (Trapezoidal BLDC)](#1-six-step-commutation-trapezoidal-bldc)
  - [2. Field-Oriented Control (FOC)](#2-field-oriented-control-foc)
- [Antarmuka Komunikasi & Perintah CLI (GRBL-Style)](#-antarmuka-komunikasi--perintah-cli-grbl-style)
  - [Daftar Parameter Konfigurasi ($)](#daftar-parameter-konfigurasi-)
  - [Perintah Operasional](#perintah-operasional)
  - [Format Telemetri Biner (High-Speed GUI)](#format-telemetri-biner-high-speed-gui)
- [Struktur Proyek](#-struktur-proyek)
- [Panduan Penggunaan & Kompilasi](#-panduan-penggunaan--kompilasi)

---

## ⚙️ Spesifikasi Hardware & Sistem

| Komponen / Parameter | Spesifikasi |
| :--- | :--- |
| **Microcontroller (MCU)** | STM32G431CBU6 (Arm® Cortex®-M4 @ 170 MHz, FPU, DSP) |
| **Hardware Accelerators** | CORDIC (Fast Sine/Cosine ~6 cycle) & FMAC (Filter Math Accelerator) |
| **Clock Source** | 16 MHz HSI PLL $\rightarrow$ 170 MHz SYSCLK, 48 MHz HSI48 (USB) |
| **Aplikasi Target** | Hub Motor E-Bike / BLDC Motor (Shell Eco-Marathon EV) |
| **Driver / Inverter Power** | 3-Phase Half-Bridge / MOSFET Driver (BTS7960 / Discrete Driver) |
| **Feedback Sensor** | 3x Hall Sensors (120° Spacing) / Quadrature Encoder / AS5600 |
| **Komunikasi** | USB Virtual COM Port (CDC), FDCAN1 |
| **Penyimpanan Non-Volatile**| Flash Page 63 (`0x0801F800`) untuk menyimpan parameter motor |

---

## 📌 Konfigurasi Pinout (Mapping Pin)

Berdasarkan konfigurasi STM32CubeMX (`.ioc` & `main.c`):

### 1. Inverter PWM (Timer TIM1)
| Pin STM32 | Fungsi Peripheral | Deskripsi |
| :--- | :--- | :--- |
| **PA8** | `TIM1_CH1` | PWM Output Fasa U (Fasa A) |
| **PA9** | `TIM1_CH2` | PWM Output Fasa V (Fasa B) |
| **PA10** | `TIM1_CH3` | PWM Output Fasa W (Fasa C) |

### 2. Driver / MOSFET Gate Enable (GPIO Output)
| Pin STM32 | Label Pin | Mode | Deskripsi |
| :--- | :--- | :--- | :--- |
| **PB14** | `EN_U` | GPIO Output PP | Enable Gate Driver Fasa U |
| **PB15** | `EN_V` | GPIO Output PP | Enable Gate Driver Fasa V |
| **PC6**  | `EN_W` | GPIO Output PP | Enable Gate Driver Fasa W |

### 3. Sensor Hall (Timer TIM2 - XORed Hall Sensor Interface)
| Pin STM32 | Fungsi Peripheral | Deskripsi |
| :--- | :--- | :--- |
| **PA0** | `TIM2_CH1` | Input Hall Sensor U |
| **PA1** | `TIM2_CH2` | Input Hall Sensor V |
| **PA2** | `TIM2_CH3` | Input Hall Sensor W |

### 4. Current & Voltage Sensing (ADC1 & ADC2)
| Pin STM32 | Saluran ADC | Deskripsi |
| :--- | :--- | :--- |
| **PB0** | `ADC1_IN15` | Pembacaan Arus Fasa A / Shunt Resistor |
| **PB1** | `ADC1_IN12` | Pembacaan Arus Fasa B |
| **PB2** | `ADC2_IN12` | Pembacaan Arus Fasa C |

### 5. Komunikasi & Debug
| Pin STM32 | Antarmuka | Deskripsi |
| :--- | :--- | :--- |
| **PA11** | `USB_DM` | USB Full Speed D- (CDC Virtual COM Port) |
| **PA12** | `USB_DP` | USB Full Speed D+ (CDC Virtual COM Port) |
| **PB8 (BOOT0)** | `FDCAN1_RX` | FDCAN Bus Receive |
| **PB9** | `FDCAN1_TX` | FDCAN Bus Transmit |
| **PA13** | `SYS_JTMS-SWDIO` | SWD Debug Data |
| **PA14** | `SYS_JTCK-SWCLK` | SWD Debug Clock |
| **PF0 / PF1** | `RCC_OSC_IN / OUT` | High-Speed External Crystal (Optional) |
| **PC14 / PC15** | `RCC_OSC32_IN / OUT` | Low-Speed External Crystal (RTC/LSE) |

---

## 🧠 Arsitektur & Algoritma Kontrol

Mode kendali utama dapat dikonfigurasi melalui file [`Core/Inc/hub_motor_ebike.h`](file:///c:/Users/ahmad/Documents/Project-Yuuahmad/driver-servo/Shell-Eco-Marathon-Kukang-EV/STM32G431CBU6-kukang-hall-sensor/Core/Inc/hub_motor_ebike.h):
```c
#define CONTROL_ALGORITHM CONTROL_SIX_STEP // atau CONTROL_FOC
```

```
                     ┌──────────────────────────────────────────────┐
                     │          USB CDC / Telemetry / CLI           │
                     └──────────────────────┬───────────────────────┘
                                            │
                                            ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                 CONTROL ALGORITHM                                      │
├────────────────────────────────────────┬───────────────────────────────────────────────┤
│        SIX-STEP (TRAPEZOIDAL)          │             FOC (VECTOR CONTROL)              │
├────────────────────────────────────────┼───────────────────────────────────────────────┤
│ • Pembacaan Status Hall (PA0,PA1,PA2)  │ • Estimasi Posisi & CORDIC Sin/Cos (Q1.31)    │
│ • Komutasi 6 Sektor 120°               │ • Cascaded PID: Posisi -> Kecepatan -> Arus   │
│ • Penyesuaian Duty Cycle TIM1 CCR      │ • Clarke / Park & Inverse Transform           │
│ • Enable/Disable High-Low Bridge       │ • Space Vector PWM (SVPWM Midpoint Clamping)  │
│                                        │ • Voltage-Guided State Current Observer (PLL) │
└────────────────────────────────────────┴───────────────────────────────────────────────┘
                                            │
                                            ▼
                     ┌──────────────────────────────────────────────┐
                     │   TIM1 PWM (PA8,PA9,PA10) + EN (PB14,PB15,PC6)│
                     └──────────────────────┬───────────────────────┘
                                            │
                                            ▼
                     ┌──────────────────────────────────────────────┐
                     │           3-Phase Inverter & Motor           │
                     └──────────────────────────────────────────────┘
```

---

### 1. Six-Step Commutation (Trapezoidal BLDC)
Diimplementasikan pada [`Core/Src/six_step.c`](file:///c:/Users/ahmad/Documents/Project-Yuuahmad/driver-servo/Shell-Eco-Marathon-Kukang-EV/STM32G431CBU6-kukang-hall-sensor/Core/Src/six_step.c).
Menggunakan logika komutasi standar 120° berdasarkan pola 3 sensor Hall:

| State Hall (Decimal) | Fasa U (PA8 / PB14) | Fasa V (PA9 / PB15) | Fasa W (PA10 / PC6) | Arah Aliran Arus |
| :---: | :---: | :---: | :---: | :---: |
| **5** | PWM (High) | GND (Low) | Floating (EN=0) | $U \rightarrow V$ |
| **1** | PWM (High) | Floating (EN=0) | GND (Low) | $U \rightarrow W$ |
| **3** | Floating (EN=0) | PWM (High) | GND (Low) | $V \rightarrow W$ |
| **2** | GND (Low) | PWM (High) | Floating (EN=0) | $V \rightarrow U$ |
| **6** | GND (Low) | Floating (EN=0) | PWM (High) | $W \rightarrow U$ |
| **4** | Floating (EN=0) | GND (Low) | PWM (High) | $W \rightarrow V$ |

*Jika terjadi state ilegal (`0` atau `7`), semua output langsung dimatikan (floating) untuk proteksi hubung singkat (short circuit).*

---

### 2. Field-Oriented Control (FOC)
Diimplementasikan pada [`Core/Src/foc.c`](file:///c:/Users/ahmad/Documents/Project-Yuuahmad/driver-servo/Shell-Eco-Marathon-Kukang-EV/STM32G431CBU6-kukang-hall-sensor/Core/Src/foc.c).
- **Akselerasi Trigonometri CORDIC**: Menggunakan register coprocessor STM32G4 (`CORDIC->WDATA` & `CORDIC->RDATA`) dalam format Q1.31 untuk komputasi $\sin(\theta)$ dan $\cos(\theta)$ secara presisi dan ultra-cepat (< 6 siklus CPU).
- **SVPWM Midpoint Clamping**: Memaksimalkan pemanfaatan tegangan DC Bus hingga $15.5\%$ lebih tinggi dibanding modulasi sinusoidal murni.
- **Ramp Acceleration Limiter**: Menghindari lonjakan arus (*inrush current*) dengan membatasi laju perubahan kecepatan ($\text{rad}/\text{s}^2$).
- **Observer Arus Khusus Half-Wave (BTS7960)**: Dilengkapi modul *Gradient Descent Observer* untuk merekonstruksi bentuk gelombang sinus fasa kontinu dari sensor pembacaan satu arah.

---

## 💻 Antarmuka Komunikasi & Perintah CLI (GRBL-Style)

Firmware dilengkapi antarmuka teks bergaya **GRBL** melalui **USB Virtual COM Port (CDC)** (Baudrate otomatis/apapun).

### Daftar Parameter Konfigurasi ($)

| Perintah | Deskripsi | Default | Satuan / Keterangan |
| :--- | :--- | :---: | :--- |
| `$0=x` | Frekuensi PWM | `20000` | Hz (1 kHz – 100 kHz) |
| `$1=x` | Pole Pairs | `7` | Jumlah pasang kutub motor |
| `$2=x` | Magnets | `14` | Jumlah magnet rotor |
| `$3=x` | Encoder Resolution (PPR) | `2500` | Pulse per Revolution |
| `$4=x` | Tegangan DC Bus | `24.0` | Volt (V) |
| `$5=x` | Tegangan Maksimum Output | `24.0` | Volt (V) |
| `$6=x` | Mode Kontrol | `1` | `0`: Torsi/Tegangan, `1`: Kecepatan, `2`: Posisi, `3`: Open Loop |
| `$7=x` | Velocity PID - $K_p$ | `0.5` | Proportional gain kecepatan |
| `$8=x` | Velocity PID - $K_i$ | `0.1` | Integral gain kecepatan |
| `$9=x` | Velocity PID - $K_d$ | `0.0` | Derivative gain kecepatan |
| `$10=x`| Velocity Limit | `24.0` | Batas tegangan output maksimum (V) |
| `$11=x`| Position PID - $K_p$ | `2.0` | Proportional gain posisi |
| `$12=x`| Position PID - $K_i$ | `0.0` | Integral gain posisi |
| `$13=x`| Position PID - $K_d$ | `0.1` | Derivative gain posisi |
| `$14=x`| Position Limit | `50.0` | Batas kecepatan sudut maksimum (rad/s) |
| `$15=x`| Arah Encoder | `0` | `0`: Normal, `1`: Inverted |
| `$16=x`| Verbose Telemetry Output | `1` | `0`: Nonaktif, `1`: Aktif |
| `$17=x`| Periode Verbose Output | `10` | Milidetik (ms) |
| `$18=x`| Tegangan Open Loop | `2.0` | Volt (V) untuk mode `$6=3` |
| `$19=x`| Batas Akselerasi (Ramp) | `50.0` | $\text{rad}/\text{s}^2$ (0 = tanpa limit) |
| `$20=x`| Current PID - $K_p$ | `0.5` | Proportional gain arus $I_d, I_q$ |
| `$21=x`| Current PID - $K_i$ | `0.1` | Integral gain arus $I_d, I_q$ |
| `$22=x`| Current Limit | `24.0` | Batas tegangan loop arus |
| `$23=x`| Current Sense Gain | `0.0068` | Skala ADC ke Ampere |
| `$24=x`| Current Sense Offset Phase A | `0.0` | Offset kalibrasi ADC fasa A |
| `$25=x`| Current Sense Offset Phase B | `0.0` | Offset kalibrasi ADC fasa B |
| `$26=x`| Current Sense Offset Phase C | `0.0` | Offset kalibrasi ADC fasa C |

### Perintah Operasional

| Perintah | Fungsi | Contoh Penggunaan |
| :--- | :--- | :--- |
| `$$` | Menampilkan seluruh nilai konfigurasi aktif | Kirim `$$` |
| `?` | Menampilkan status real-time (sudut, kecepatan, tegangan/duty, status) | Kirim `?` |
| `$save` | Menyimpan konfigurasi aktif ke Flash internal STM32 | Kirim `$save` |
| `$align` | Melakukan kalibrasi rotor alignment untuk FOC (2 detik) | Kirim `$align` |
| `$h` | Menampilkan bantuan ringkas daftar perintah | Kirim `$h` |
| `S<val>` | Mengatur target setpoint (kecepatan / posisi / duty cycle) & mengaktifkan motor | `S50` (Duty 50% di Six-Step atau 50 rad/s di FOC) |
| `T` | Menghentikan motor dan menonaktifkan MOSFET (Coast/Float) | Kirim `T` |

### Format Telemetri Biner (High-Speed GUI)
Ketika `$16=1`, mikrokontroler mengirimkan paket biner non-blocking 33-byte setiap `$17` ms untuk visualisasi grafik di GUI:
```
[0xAA, 0xBB] [Pos: 4B] [Vel: 4B] [Vq/Iq: 4B] [Target: 4B] [Ia: 4B] [Ib: 4B] [Ic: 4B] [Mode: 1B] [CRC: 1B] [0x55]
```
- **Header**: `0xAA, 0xBB`
- **Data (Float Little-Endian 32-bit)**: `Position`, `Velocity`, `Vq_or_Iq`, `Target`, `Ia`, `Ib`, `Ic`
- **Mode**: `uint8_t` (Sesuai `$6`)
- **CRC**: Simple XOR checksum
- **Footer**: `0x55`

---

## 📂 Struktur Proyek

```
STM32G431CBU6-kukang-hall-sensor/
├── Core/
│   ├── Inc/
│   │   ├── config.h             # Definisi struktur motor_config_t & flash storage
│   │   ├── foc.h                # Definisi FOC state, Park/Clarke, SVPWM
│   │   ├── hub_motor_ebike.h    # Pilihan mode algoritma & tipe sensor feedback
│   │   ├── motor_servo_lichuan.h# Konfigurasi encoder opsional (AS5600 I2C / Incremental)
│   │   ├── pid.h                # Struktur data & fungsi pengendali PID
│   │   ├── six_step.h           # Prototipe fungsi Six-Step Hall Commutation
│   │   ├── main.h               # Definisi global HAL & pinout
│   │   └── stm32g4xx_it.h       # Prototipe handler interrupt
│   └── Src/
│       ├── config.c             # Parser CLI GRBL, flash write/erase, serial transmit
│       ├── foc.c                # Implementasi FOC, CORDIC Sin/Cos, Observer
│       ├── main.c               # Inisialisasi sistem, peripheral clock, main loop
│       ├── pid.c                # Algoritma komputasi PID anti-windup
│       ├── six_step.c           # Implementasi tabel komutasi 6-step Hall sensor
│       ├── stm32g4xx_hal_msp.c  # Inisialisasi level MSP peripheral
│       └── stm32g4xx_it.c       # Vector interrupt handler
├── USB_Device/                  # USB CDC Virtual COM Port stack
├── STM32G431CBU6-kukang-hall-sensor.ioc # Konfigurasi proyek grafis STM32CubeMX
└── readme.md                    # Dokumentasi utama proyek
```

---

## 🚀 Panduan Penggunaan & Kompilasi

### 1. Prasyarat Software
- **STM32CubeIDE** (v1.14.0 atau lebih baru) / **Keil MDK-ARM** / **GCC ARM Toolchain** + CMake.
- **STM32CubeMX** (v6.18.0) dengan package `STM32Cube FW_G4 V1.6.3`.
- **Serial Terminal**: PuTTY, Tera Term, Arduino Serial Monitor, atau GUI Telemetri.

### 2. Kompilasi & Flash Firmware
1. Buka proyek ini di **STM32CubeIDE**: `File` $\rightarrow$ `Open Projects from File System...` $\rightarrow$ pilih folder proyek.
2. Lakukan build proyek dengan menekan tombol **Build (Hammer Icon)** atau shortcut `Ctrl + B`.
3. Hubungkan board STM32G431CBU6 ke ST-Link V2 / V3 melalui pin SWD (`SWDIO`, `SWCLK`, `GND`, `3V3`).
4. Klik **Run** atau **Debug (F11)** untuk mem-flash program ke mikrokontroler.

### 3. Menjalankan Motor
1. Hubungkan port USB STM32 ke PC. Buka Serial Monitor pada COM Port yang terdeteksi (Baudrate bebas, 115200 disarankan).
2. Kirim perintah `$$` untuk memeriksa parameter motor.
3. Untuk mode **Six-Step**, jalankan motor dengan mengirimkan perintah:
   ```text
   S20
   ```
   *(Motor akan berputar dengan Duty Cycle 20%).*
4. Untuk menghentikan motor:
   ```text
   T
   ```
   *(Motor akan coasting/float).*
5. Jika ingin menyimpan perubahan parameter ke memori flash:
   ```text
   $save
   ```

---
*Dikembangkan untuk Tim Shell Eco-Marathon Kukang EV.*

