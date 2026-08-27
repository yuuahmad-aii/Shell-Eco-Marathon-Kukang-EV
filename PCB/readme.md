# Hardware Documentation: Motor Driver Behemoth (Shell Eco-Marathon Kukang EV)

Dokumentasi ini menjelaskan skematik, pemilihan komponen, spesifikasi kelistrikan, dan konfigurasi pinout dari board **Motor Driver Behemoth** yang dirancang untuk kendaraan hemat energi **Shell Eco-Marathon (Kukang EV)**.

Sumber desain skematik mengacu pada dokumen [`SCH_SCH Shell Eco Marathon Behemoth_2026-08-27.pdf`](file:///c:/Users/ahmad/Documents/Project-Yuuahmad/driver-servo/Shell-Eco-Marathon-Kukang-EV/PCB/SCH_SCH%20Shell%20Eco%20Marathon%20Behemoth_2026-08-27.pdf).

---

## 📋 Daftar Isi
- [Ringkasan & Spesifikasi Kelistrikan](#-ringkasan--spesifikasi-kelistrikan)
- [Komponen Utama (Key ICs & Power Stage)](#-komponen-utama-key-ics--power-stage)
- [Blok Diagram Arsitektur Hardware](#-blok-diagram-arsitektur-hardware)
- [Mapping Pinout Mikrokontroler (STM32G474RBT3)](#-mapping-pinout-mikrokontroler-stm32g474rbt3)
- [Sistem Penginderaan & Pengkondisi Sinyal (Sensing)](#-sistem-penginderaan--pengkondisi-sinyal-sensing)
- [Antarmuka Komunikasi & Konektor Eksternal](#-antarmuka-komunikasi--konektor-eksternal)
- [Sistem Catu Daya (Power Management)](#-sistem-catu-daya-power-management)
- [Fitur Proteksi & Desain Keandalan](#-fitur-proteksi--desain-keandalan)

---

## ⚡ Ringkasan & Spesifikasi Kelistrikan

| Parameter | Nilai / Rating | Keterangan |
| :--- | :--- | :--- |
| **Tegangan Operasi Nominal (DC Bus)** | **48V – 60V DC** | Optimal untuk regulasi Shell Eco-Marathon EV |
| **Tegangan Maksimum Absolut** | **100V DC** | Batas breakdown komponen (MOSFET & Gate Driver) |
| **Overvoltage Protection (OVP)** | **80V – 85V DC** | Trigger proteksi pemutus PWM regeneratif pada firmware |
| **Arus Fasa Kontinu (Continuous Current)** | **50A RMS** | Dengan pendinginan / heatsink memadai |
| **Arus Fasa Puncak (Peak Current)** | **120A** | Durasi pendek (< 5 detik akselerasi) |
| **Frekuensi Switching PWM** | **20 kHz – 40 kHz** | Trade-off optimal antara switching loss & current ripple |
| **Algoritma Kendali Utama** | **Field-Oriented Control (FOC)** | Mendukung *Regenerative Braking* & *Coast-and-Burn* |

---

## 🧩 Komponen Utama (Key ICs & Power Stage)

| Sub-Sistem | Komponen / IC | Pabrikan | Deskripsi & Fungsi |
| :--- | :--- | :--- | :--- |
| **Microcontroller (MCU)** | `STM32G474RBT3` (LQFP64) | STMicroelectronics | Arm® Cortex®-M4 @ 170 MHz, Math Accelerators (CORDIC, FMAC), High-Resolution Timer (HRTIM), Triple-ADC 12-bit 4Msps. |
| **Gate Driver 3-Fasa** | `DRV8353SRTAR` (WQFN40) | Texas Instruments | 100V Triple Half-Bridge Smart Gate Driver, interface SPI, 3 integrated current shunt amplifiers, slew rate control, hardware fault monitoring. |
| **Power Stage MOSFETs** | 6x `IPT015N10N5` (TOLL) | Infineon | 100V N-Channel MOSFET, $R_{DS(on)}$ ultra-rendah **1.5 mΩ**, package TO-Leadless dengan resistansi termal sangat kecil. |
| **Current Shunt Resistors** | 3x `7 mΩ` | Precision Shunt | Shunt resistor low-side fasa A, B, C dengan koneksi Kelvin sense diferensial. |
| **Primary DC-DC Buck** | `LM5161PWPR` (HTSSOP14) | Texas Instruments | 100V Wide-Input 1A Synchronous Step-Down Converter (menurunkan tegangan Bus 48V/60V ke 5V Logic). |
| **Secondary Linear LDO** | `AMS1117-3.3` (SOT-223) | Advanced Monolithic | Menurunkan rail 5V ke **3.3V** untuk logic MCU & sensor analog. |
| **CAN Bus Transceiver** | `SN65HVD230DR` (SOIC8) | Texas Instruments | 3.3V CAN Transceiver dengan terminasi bus 120Ω. |
| **RS-485 / Modbus IC** | `THVD1420DR` (SOIC8) | Texas Instruments | 3.3V Half-Duplex RS-485 Transceiver dengan terminasi 120Ω. |
| **Differential Line Receiver** | 3x `TPT480L1-SO1R` (SOIC8) | 3PEAK | Penerima sinyal diferensial berkecepatan tinggi (RS-422) untuk Encoder ABZ / SPI. |

---

## 🏗️ Blok Diagram Arsitektur Hardware

```
                                  ┌──────────────────────────┐
                                  │   DC BUS (48V - 60V DC)  │
                                  └─────────────┬────────────┘
                                                │
                 ┌──────────────────────────────┼──────────────────────────────┐
                 │                              │                              │
                 ▼                              ▼                              ▼
    ┌──────────────────────────┐   ┌──────────────────────────┐   ┌──────────────────────────┐
    │  Power Supply Regulator  │   │   Power Stage Inverter   │   │  Voltage Sense Dividers  │
    │  LM5161 (100V -> 5V Buck)│   │  6x IPT015N10N5 MOSFETs  │   │  3x Phase + 1x DC Bus    │
    │  AMS1117 (5V -> 3.3V LDO)│   │  (100V, 1.5mΩ, TOLL)     │   │  (Scale Factor: ~40.24x) │
    └────────────┬─────────────┘   └────────────▲─────────────┘   └────────────┬─────────────┘
                 │ (3.3V / 5V)                  │ (Gate Drive)                 │ (Analog ADC)
                 ▼                              │                              ▼
┌────────────────────────────────────────────────────────────────────────────────────────────┐
│                              TEXAS INSTRUMENTS DRV8353SR                                    │
│   • 100V Smart Gate Drive Architecture      • 3x Integrated Current Shunt Amplifiers       │
│   • SPI Configuration & Diagnostics         • Independent High/Low Logic Inputs            │
└───────────────────────────────────────────────▲──────────────────────────────┬─────────────┘
                                                │ (PWM / SPI)                  │ (Current SOA/B/C)
                                                │                              ▼
┌───────────────────────────────────────────────┴────────────────────────────────────────────┐
│                                STM32G474RBT3 MICROCONTROLLER                               │
│   • 170 MHz Cortex-M4 Core                  • CORDIC Hardware Co-processor (Vector FOC)    │
│   • Advanced Control Timer (TIM1 PWM)       • Triple High-Speed ADC (Current/Voltage/Temp) │
└───────┬───────────────────┬───────────────────┬───────────────────┬────────────────────────┘
        │ (USB FS)          │ (CAN Bus)         │ (RS-485 Modbus)   │ (Encoder / Hall)
        ▼                   ▼                   ▼                   ▼
 ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────────────────────┐
 │ USB Type-C   │    │ SN65HVD230   │    │  THVD1420    │    │ 3x TPT480L1 Line Receivers   │
 │ (CDC VCP)    │    │ (RJ45 Port)  │    │  (RJ45 Port) │    │ & Hall Filters (DB25 Port)   │
 └──────────────┘    └──────────────┘    └──────────────┘    └──────────────────────────────┘
```

---

## 📌 Mapping Pinout Mikrokontroler (STM32G474RBT3)

### 1. Kontrol Gate Driver & PWM (DRV8353SR)
| Pin MCU | Nama Net Skematik | Peripheral STM32 | Deskripsi Fungsi |
| :--- | :--- | :--- | :--- |
| **PA8** | `DRIVER_HIGH_A` | `TIM1_CH1` | PWM High-Side Fasa A |
| **PB15** | `DRIVER_LOW_A` | `TIM1_CH1N` / GPIO | PWM Low-Side Fasa A |
| **PA9** | `DRIVER_HIGH_B` | `TIM1_CH2` | PWM High-Side Fasa B |
| **PB14** | `DRIVER_LOW_B` | `TIM1_CH2N` / GPIO | PWM Low-Side Fasa B |
| **PA10** | `DRIVER_HIGH_C` | `TIM1_CH3` | PWM High-Side Fasa C |
| **PB13** | `DRIVER_LOW_C` | `TIM1_CH3N` / GPIO | PWM Low-Side Fasa C |
| **PC8** | `STM32_EN_GATE` | GPIO Output | Enable Gate Driver DRV8353 (Active High) |
| **PB11** | `GATE_DRIVER_FAULT` | GPIO Input | Indikator Fault nFAULT (Active Low) |

### 2. Antarmuka SPI Gate Driver (DRV8353SR)
| Pin MCU | Nama Net Skematik | Peripheral STM32 | Deskripsi Fungsi |
| :--- | :--- | :--- | :--- |
| **PA15** | `SPI_DRIVER_CS` | GPIO Output / `SPI3_NSS` | Chip Select DRV8353 (Active Low) |
| **PC10** | `SPI_DRIVER_CLK` | `SPI3_SCK` | Serial Clock SPI |
| **PC11** | `SPI_DRIVER_MISO` | `SPI3_MISO` | Serial Data Out (SDO DRV8353) |
| **PC12** | `SPI_DRIVER_MOSI` | `SPI3_MOSI` | Serial Data In (SDI DRV8353) |

### 3. Penginderaan Arus 3-Fasa (Current Sensing)
| Pin MCU | Nama Net Skematik | Channel ADC | Deskripsi Fungsi |
| :--- | :--- | :--- | :--- |
| **PB0** | `STM32_CUR_A` | `ADC1_IN15` / `ADC2_IN15` | Output Amplifier Arus Fasa A (`SOA` DRV8353) |
| **PB1** | `STM32_CUR_B` | `ADC1_IN12` / `ADC3_IN1` | Output Amplifier Arus Fasa B (`SOB` DRV8353) |
| **PB2** | `STM32_CUR_C` | `ADC2_IN12` | Output Amplifier Arus Fasa C (`SOC` DRV8353) |

### 4. Penginderaan Tegangan Fasa & Bus (Voltage Sensing)
| Pin MCU | Nama Net Skematik | Channel ADC | Deskripsi Fungsi |
| :--- | :--- | :--- | :--- |
| **PC2** | `VOLTAGE_SENSE_A` | `ADC12_IN8` | Pembacaan Tegangan Fasa A |
| **PC1** | `VOLTAGE_SENSE_B` | `ADC12_IN7` | Pembacaan Tegangan Fasa B |
| **PC0** | `VOLTAGE_SENSE_C` | `ADC12_IN6` | Pembacaan Tegangan Fasa C |
| **PC3** | `VOLTAGE_SENSE` | `ADC12_IN9` | Pembacaan Total Tegangan DC Bus ($V_{BUS}$) |

### 5. Sensor Suhu (NTC Thermistor)
| Pin MCU | Nama Net Skematik | Channel ADC | Deskripsi Fungsi |
| :--- | :--- | :--- | :--- |
| **PC4** | `STM32_TEMP_1` | `ADC2_IN5` | Monitoring Suhu MOSFET / Motor Channel 1 |
| **PC5** | `STM32_TEMP_2` | `ADC2_IN11` | Monitoring Suhu MOSFET / Motor Channel 2 |

### 6. Sensor Hall 3-Fasa
| Pin MCU | Nama Net Skematik | Peripheral STM32 | Deskripsi Fungsi |
| :--- | :--- | :--- | :--- |
| **PA0** | `STM32_HALL_U` | `TIM2_CH1` (Hall XOR) | Sinyal Hall Sensor Fasa U |
| **PA1** | `STM32_HALL_V` | `TIM2_CH2` (Hall XOR) | Sinyal Hall Sensor Fasa V |
| **PA2** | `STM32_HALL_W` | `TIM2_CH3` (Hall XOR) | Sinyal Hall Sensor Fasa W |

### 7. Encoder Quadrature (ABZ)
| Pin MCU | Nama Net Skematik | Peripheral STM32 | Deskripsi Fungsi |
| :--- | :--- | :--- | :--- |
| **PB5** | `STM32_ENC_A` | `TIM3_CH2` (Encoder Mode) | Sinyal Quadrature Kanal A |
| **PB4** | `STM32_ENC_B` | `TIM3_CH1` (Encoder Mode) | Sinyal Quadrature Kanal B |
| **PB3** | `STM32_ENC_Z` | `TIM2_CH2` / GPIO EXTI | Sinyal Index / Zero Pulse Z |

### 8. Encoder Absolut (SPI Differensial)
| Pin MCU | Nama Net Skematik | Peripheral STM32 | Deskripsi Fungsi |
| :--- | :--- | :--- | :--- |
| **PA4** | `SPI_ENC_CS` | `SPI1_NSS` / GPIO | Chip Select Encoder SPI |
| **PA5** | `SPI_ENC_CLK` | `SPI1_SCK` | Clock Encoder SPI |
| **PA6** | `SPI_ENC_MISO` | `SPI1_MISO` | Data In dari Encoder SPI |
| **PA7** | `SPI_ENC_MOSI` | `SPI1_MOSI` | Data Out ke Encoder SPI |

### 9. Jalur Komunikasi Eksternal
| Pin MCU | Nama Net Skematik | Peripheral STM32 | Deskripsi Fungsi |
| :--- | :--- | :--- | :--- |
| **PA11** | `USB-` | `USB_DM` | Jalur USB Data Minus (Type-C) |
| **PA12** | `USB+` | `USB_DP` | Jalur USB Data Plus (Type-C) |
| **PB8 (BOOT0)** | `STM32_CAN_RX` | `FDCAN1_RX` | Receive CAN Bus via SN65HVD230 |
| **PB9** | `STM32_CAN_TX` | `FDCAN1_TX` | Transmit CAN Bus via SN65HVD230 |
| **PB6** | `STM32_MODBUS_TX` | `USART1_TX` | Transmit Modbus RS-485 via THVD1420 |
| **PB7** | `STM32_MODBUS_RX` | `USART1_RX` | Receive Modbus RS-485 via THVD1420 |
| **PD2** | `STM32_MODBUS_SEL`| GPIO Output | Direction Control RS-485 (DE/nRE) |
| **PC6** | `I2C_SCL` | `I2C1_SCL` | I2C Clock (Auxiliary / Sensor I2C) |
| **PC7** | `I2C_SDA` | `I2C1_SDA` | I2C Data (Auxiliary / Sensor I2C) |

### 10. Sistem & Debug
| Pin MCU | Nama Net Skematik | Deskripsi Fungsi |
| :--- | :--- | :--- |
| **PG10** | `STM32_NRST` | Master Reset (terhubung ke tombol reset SW1) |
| **PA13** | `STM32_SWDIO` | SWD Data Debug |
| **PA14** | `STM32_SWCLK` | SWD Clock Debug |
| **PB10** | `LED2` | User Status LED (Active High, seri 4.7kΩ) |
| **PF0 / PF1** | `PF0-OSC_IN / PF1-OSC_OUT` | Osilator Kristal Eksternal 25 MHz (X1) |

---

## 🔍 Sistem Penginderaan & Pengkondisi Sinyal (Sensing)

### 1. Pembagi Tegangan Fasa & DC Bus (Voltage Dividers)
Rangkaian pembagi tegangan menggunakan resistor presisi tinggi dengan clamping diode Schottky `BAT54S` untuk melindungi pin ADC MCU dari tegangan lebih ($> 3.3\text{V}$):

$$\text{V}_{\text{ADC}} = \text{V}_{\text{BUS}} \times \frac{R_{\text{low}}}{R_{\text{high}} + R_{\text{low}}} = \text{V}_{\text{BUS}} \times \frac{9.76\text{ k}\Omega}{383\text{ k}\Omega + 9.76\text{ k}\Omega} = \text{V}_{\text{BUS}} \times 0.024849$$

$$\text{Rasio Pembagi} \approx \frac{1}{40.244}$$

- **Tegangan Bus 48V** $\rightarrow$ Tegangan ADC $\approx 1.193\text{ V}$
- **Tegangan Bus 60V** $\rightarrow$ Tegangan ADC $\approx 1.491\text{ V}$
- **Tegangan Bus 100V** $\rightarrow$ Tegangan ADC $\approx 2.485\text{ V}$ (Aman di bawah $V_{REF+} = 3.3\text{V}$)

### 2. Pengukuran Arus 3-Fasa (3-Shunt Low-Side Sensing)
- Menggunakan shunt resistor **$7\text{ m}\Omega$** pada masing-masing low-side fasa (A, B, C).
- Dihubungkan secara diferensial (*Kelvin sense*) ke pin `SPA/SNA`, `SPB/SNB`, `SPC/SNC` pada driver `DRV8353SR`.
- Penguatan (*gain*) amplifier internal DRV8353SR dapat diprogram via SPI ($10\text{V/V}, 20\text{V/V}, 40\text{V/V}, 80\text{V/V}$).
- Output arus `SOA`, `SOB`, `SOC` disaring menggunakan filter low-pass RC ($120\Omega + 1\text{nF}$) sebelum masuk ke ADC STM32.

### 3. Pengukuran Suhu (NTC Thermistors)
- Rangkaian pembagi tegangan NTC dengan resistor pull-up **$10\text{ k}\Omega$** ke rail $+3.3\text{V}$ dan filter kapasitor $100\text{nF}$ ke ground.

---

## 🔌 Antarmuka Komunikasi & Konektor Eksternal

### 1. Konektor DB25 Motor Feedback (`DSUB1`)
Konektor DB-25 membawa seluruh sinyal umpan balik sensor motor dari kendaraan:
- **Kanal Encoder Inkremental Diferensial**: `A+ / A-`, `B+ / B-`, `Z+ / Z-`
- **Kanal Encoder SPI Diferensial**: `CLK+ / CLK-`, `MOSI+ / MOSI-`, `CS+ / CS-`
- **Kanal Sensor Hall**: `HALL_U`, `HALL_V`, `HALL_W`
- **Kanal Sensor Suhu**: `NTC_RESISTOR_1`, `NTC_RESISTOR_2`
- **Catu Daya Sensor**: $+5\text{V}$, $+3.3\text{V}$, dan $\text{GND}$

### 2. Konektor RJ45 Daisy-Chain (`RJ1` & `RJ2`)
Digunakan untuk komunikasi antar-modul telemetry / dashboard mobil:
- **CAN Bus**: `CAN+`, `CAN-`
- **RS-485 Modbus**: `MODBUS+`, `MODBUS-`
- **Power Bus**: $+5\text{V}$ dan $\text{GND}$

### 3. Port USB Type-C (`USB1`)
- Dilengkapi resistor pull-down $5.1\text{ k}\Omega$ pada pin CC1 & CC2 untuk kompatibilitas standar USB-C.
- Digunakan untuk CLI, tuning PID FOC real-time, dan telemetri serial.

### 4. Header SWD & I2C (`H2`)
- 8-pin pitch 2.54mm header untuk flashing/debugging firmware ST-Link (`SWDIO`, `SWCLK`, `NRST`, `+3V3`, `GND`) serta bus I2C eksternal.

---

## 🔋 Sistem Catu Daya (Power Management)

1. **Step-Down Primer (100V $\rightarrow$ 5V)**:
   - IC `LM5161PWPR` beroperasi sebagai synchronous buck converter efisiensi tinggi.
   - Mampu menerima input hingga $100\text{V DC}$, menurunkan tegangan ke $+5\text{V}$ untuk menyuplai transceiver bus dan LDO sekunder.
   - Dilengkapi induktor daya $100\mu\text{H}$ (`L1`) dan filter output kapasitor keramik $2\times 22\mu\text{F}$.
2. **Step-Down Sekunder (5V $\rightarrow$ 3.3V)**:
   - IC `AMS1117-3.3` menghasilkan tegangan logic bersih $+3.3\text{V}$ untuk mikrokontroler STM32G474 dan periferal digital.
   - Dipisahkan dengan Ferrite Bead `L2` (`PBY160808T-221Y-N`) untuk menyuplai domain analog ADC (`+3V3_A`).
3. **Bulk Capacitor Bank**:
   - $7\times 220\mu\text{F}$ kapasitor elektrolitik low-ESR paralel ditambah deretan kapasitor keramik $1\mu\text{F}$ dan $100\text{nF}$ pada setiap *half-bridge* untuk meredam ripple tegangan transien akibat switching frekuensi tinggi.

---

## 🛡️ Fitur Proteksi & Desain Keandalan

- **Smart Gate Drive Protection (DRV8353SR)**:
  - *VDS Overcurrent Protection* independen pada setiap MOSFET fasa.
  - *Gate Drive Fault* & *Under-Voltage Lockout (UVLO)*.
  - Sinyal `nFAULT` langsung memicu interrupt ke pin `PB11` STM32 untuk *safe shut-off*.
- **Clamping Proteksi Input Analog**:
  - Semua jalur sensing tegangan (Fasa A, B, C & Bus) diproteksi menggunakan dioda Schottky `BAT54S` ganda ke rail $3.3\text{V}$ dan ground.
- **Isolasi Noise Sinyal Sensor**:
  - Sinyal encoder jarak jauh ditransmisikan dalam bentuk diferensial (RS-422) dan dikonversi kembali ke single-ended oleh receiver `TPT480L1` tepat sebelum masuk ke STM32.
  - Sinyal Hall sensor dilengkapi pull-up $1\text{ k}\Omega$ dan filter low-pass RC ($1\text{ k}\Omega + 1\text{ nF}$).

---
*Dokumentasi ini disiapkan untuk Tim Shell Eco-Marathon Kukang EV.*

