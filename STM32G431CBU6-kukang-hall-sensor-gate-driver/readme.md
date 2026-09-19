# 📁 STM32G431 Firmware Motor Controller (BLDC Gate Driver)

*Firmware* kontroler motor BLDC (*Brushless DC Motor*) berkecepatan tinggi yang berjalan pada mikrokontroler **STM32G431CBU6** (Arm® Cortex®-M4 dengan FPU, CORDIC math accelerator, dan frekuensi clock 170 MHz). Firmware ini bertanggung jawab penuh atas komutasi motor elektrik, pembacaan umpan balik sensor Hall, estimasi arus fasa fasa motor (FOC transforms), keselamatan termal/arus, serta interkoneksi jaringan kendali CAN Bus dengan papan Telemetri utama.

---

## 📌 Alokasi Pin & Periferal (Pinout)

Berikut adalah daftar alokasi pin mikrokontroler STM32G431CBU6 yang dikonfigurasi dalam `main.c` dan `stm32g4xx_hal_msp.c`:

| Pin MCU | Fungsi Periferal | Mode / Tipe | Deskripsi & Koneksi Perangkat |
| :--- | :--- | :--- | :--- |
| **PA8** | `TIM1_CH1` | AF Push-Pull (AF6) | Gate Driver High-Side Fasa U (PWM) |
| **PB13** | `TIM1_CH1N` | AF Push-Pull (AF6) | Gate Driver Low-Side Fasa U (PWM Complementary) |
| **PA9** | `TIM1_CH2` | AF Push-Pull (AF6) | Gate Driver High-Side Fasa V (PWM) |
| **PB14** | `TIM1_CH2N` | AF Push-Pull (AF6) | Gate Driver Low-Side Fasa V (PWM Complementary) |
| **PA10** | `TIM1_CH3` | AF Push-Pull (AF6) | Gate Driver High-Side Fasa W (PWM) |
| **PB15** | `TIM1_CH3N` | AF Push-Pull (AF4) | Gate Driver Low-Side Fasa W (PWM Complementary) |
| **PA15** | `GPIO_Output` | Output Push-Pull | `TIM1_ENA` (Enable pin ke IC Gate Driver, Active High) |
| **PA0** | `TIM2_CH1` | AF Input (AF1) | Input Sensor Hall Fasa A / U |
| **PA1** | `TIM2_CH2` | AF Input (AF1) | Input Sensor Hall Fasa B / V |
| **PA2** | `TIM2_CH3` | AF Input (AF1) | Input Sensor Hall Fasa C / W |
| **PB0** | `ADC1_IN15` | Analog (Injected) | Sensor Arus Fasa U (ACS712-30A) |
| **PB1** | `ADC1_IN12` | Analog (Injected) | Sensor Arus Fasa V (ACS712-30A) |
| **PB2** | `ADC2_IN12` | Analog (Injected) | Sensor Arus Fasa W (ACS712-30A) |
| **PC4** | `ADC2_IN5` | Analog (Injected) | Pembagi Tegangan DC Bus (*VBUS Voltage Divider*) |
| **PB8** | `FDCAN1_RX` | AF Push-Pull (AF9, Pull-Up) | Jalur Penerimaan CAN Bus (diambil alih dari pin BOOT0) |
| **PB9** | `FDCAN1_TX` | AF Push-Pull (AF9) | Jalur Pengiriman CAN Bus (ke CAN Transceiver) |
| **PB6** | `USART1_TX` | AF Push-Pull (AF7) | Serial UART Debug TX (115200 bps) |
| **PB7** | `USART1_RX` | AF Push-Pull (AF7) | Serial UART Debug RX (115200 bps) |
| **PA11** | `USB_DM` | USB Peripheral | USB CDC Data Minus (Antarmuka Komputer / CLI / GUI) |
| **PA12** | `USB_DP` | USB Peripheral | USB CDC Data Plus (Antarmuka Komputer / CLI / GUI) |
| **PA4** | `SPI1_NSS` | AF (AF5) | SPI1 Chip Select (Pencadangan antarmuka sensor posisi/eksternal) |
| **PA5** | `SPI1_SCK` | AF (AF5) | SPI1 Serial Clock |
| **PA6** | `SPI1_MISO` | AF (AF5) | SPI1 Master In Slave Out |
| **PA7** | `SPI1_MOSI` | AF (AF5) | SPI1 Master Out Slave In |

> [!NOTE]
> Pin **PB8** secara default pada keluarga STM32G4 terikat pada fungsi `BOOT0`. Fungsi `Check_PB8_BOOT0_Config()` pada `main.c` secara otomatis memprogram ulang *Option Bytes* internal MCU (`nSWBOOT0 = 0`, `nBOOT0 = 1`) saat startup pertama kali agar PB8 terbebas menjadi pin GPIO/FDCAN1_RX tanpa perlu jumper fisik eksternal.

---

## ⚙️ Cara Kerja Sistem & Algoritma Kontrol

```mermaid
flowchart TD
    subgraph Sensing & Calibration
        ADC[ADC1 & ADC2 Injected Conversion] -->|Triggered by TIM1 TRGO| Sense[Current U, V, W & VBUS]
        Sense --> ZeroCal[Zero-Offset Calibration at Boot]
        Sense --> FOC[Clarke & Park Transforms]
        FOC --> IdIq[Current Id & Iq Filtered]
        Hall[TIM2 Hall Sensors PA0-PA2] --> Angle[Electrical Angle Estimation]
    end

    subgraph Control Loop ~20kHz
        DWT[DWT Cycle Counter] -->|dt ~50us| SixStepUpdate[SixStep_Update dt]
        Angle --> SixStepUpdate
        IdIq --> SixStepUpdate
        PID[Speed PID Controller] -->|Target RPM vs Actual RPM| Duty[PWM Duty Cycle & Commutation State]
        Duty --> TIM1[TIM1 Complementary PWM with Deadtime]
        TIM1 --> Bridge[Inverter 3-Phase MOSFETs]
    end

    subgraph Interkoneksi CAN & Safety
        CANRx[FDCAN1 RX ID 0x10] -->|run_state, target_rpm| Cmd[Set Target RPM]
        Cmd --> SixStepUpdate
        Watchdog{CAN Timeout >350ms?} -->|Yes| Stop[SixStep_Stop Auto Shutdown]
        Heartbeat[FDCAN1 TX ID 0x20] -->|Every 100ms| Telemetry[Kirim Vbus & Iq ke STM32F4]
    end
```

### 1. Komutasi Motor (Six-Step & Estimasi Sudut Listrik)
- **Deteksi Posisi:** Sensor Hall digital dibaca melalui `TIM2` yang bekerja dalam mode *Hall Sensor Interface*. Perubahan status fasa sensor memicu transisi 6 sektor komutasi standar:
  $$\text{Sektor Urutan Normal: } 3 \rightarrow 2 \rightarrow 6 \rightarrow 4 \rightarrow 5 \rightarrow 1$$
- **Pembangkitan PWM:** Menggunakan `TIM1` *Advanced Motor Control Timer* dengan frekuensi dasar 20 kHz (dapat disetel via parameter `$0`). Output PWM fasa tinggi dan fasa rendah dilengkapi *Dead-Time* perangkat keras sebesar 136 siklus clock untuk mencegah *shoot-through* arus pendek antara sisi atas dan bawah inverter.
- **Inversi Arah Putar:** Arah putaran motor dapat dibalik secara digital melalui parameter `$14` (`invert_direction`) tanpa perlu menukar kabel fasa motor secara fisik.

### 2. Pengukuran Arus Fasa & Transformasi FOC
- Pembacaan sinyal arus fasa ($U$, $V$, $W$) dilakukan secara *injected* menggunakan ADC1 dan ADC2 yang disinkronkan tepat di tengah pulsa PWM (`TIM1_TRGO`).
- Saat startup, firmware menjalankan **Auto-Zero Current Calibration** (`CurrentSensor_Init`) sebanyak 500 sampel untuk mengunci nilai tegangan *quiescent* sensor arus ACS712 tanpa beban.
- **Clarke & Park Transform:** Sinyal arus fasa dikonversi menjadi komponen orthogonal stator ($I_\alpha, I_\beta$) dan dikoordinasikan ke kerangka rotor yang berputar menghasilkan arus fluks ($I_d$) serta arus torsi aktual ($I_q$). Nilai $I_q$ difilter menggunakan *Low Pass Filter* (LPF $\alpha = 0.05$) untuk perhitungan daya dan efisiensi mobil.

### 3. Loop Kontrol Kecepatan (PID Loop)
- Siklus kontrol dieksekusi di `main.c` dengan pembatas laju berbasis pencacah siklus perangkat keras *DWT Cycle Counter* (`CoreDebug->DEMCR` & `DWT->CYCCNT`) pada interval mendekati **20 kHz** ($dt \approx 50\,\mu\text{s}$).
- Kecepatan aktual diatur oleh kontroler PID kecepatan (`$11: vel_kp`, `$12: vel_ki`) dengan pembatas akselerasi ramp konfigurable (`$13: accel_rpm_s`).

### 4. Protokol Komunikasi FDCAN & Safety Watchdog
Motor driver terhubung langsung ke bus CAN sistem kendaraan dengan bitrate **1 Mbps**:
- **Penerimaan Perintah Throttle (CAN ID `0x10`):**
  - `Byte 0`: `run_state` (`1` = Motor Aktif, `0` = Motor Bebas / Coast)
  - `Byte 1..2`: `target_rpm` (uint16_t, Little-Endian, contoh: 1200 RPM)
- **Pengiriman Status & Telemetri (CAN ID `0x20` setiap 100 ms):**
  - `Byte 0`: `can_motor_active` (`1` jika motor berputar, `0` jika mati)
  - `Byte 1..2`: `vbus_centi` (Tegangan bus DC dalam centivolt, $V_{bus} \times 100$)
  - `Byte 3..4`: `iq_centi` (Arus torsi $I_q$ aktual dalam centiampere, $I_q \times 100$)
- **CAN Safety Watchdog:** Jika motor sedang aktif namun paket perintah CAN ID `0x10` dari telemetri terhenti atau hilang selama lebih dari **350 ms**, watchdog internal otomatis memanggil `SixStep_Stop()`, mematikan driver MOSFET, dan mengubah status menjadi `idle` demi keamanan pengemudi.

---

## 🎛️ Parameter Konfigurasi Non-Volatile (Flash Config)

Parameter operasional disimpan secara permanen di Flash internal sektor terakhir (Page 63, alamat `0x0801F800`). Pengguna dapat membaca dan mengubah nilai parameter melalui perintah berbasis teks mirip standar GRBL lewat antarmuka USB CDC.

| ID Param | Nama Variabel | Satuan | Nilai Default | Rentang / Opsi | Deskripsi Fungsi |
| :--- | :--- | :---: | :---: | :---: | :--- |
| **$0** | `pwm_freq` | Hz | `20000` | 1.000 – 100.000 | Frekuensi switching PWM inverter MOSFET. Mengubah ARR timer secara otomatis. |
| **$1** | `pole_pairs` | - | `7` | 1 – 64 | Jumlah pasangan kutub magnet permanen (*pole pairs*) pada rotor BLDC. |
| **$2** | `dc_bus_voltage` | Volt | `24.00` | 6.0 – 60.0 | Nilai acuan nominal tegangan DC bus baterai. |
| **$3** | `max_output_voltage`| Volt | `24.00` | 1.0 – 60.0 | Batas tegangan efektif maksimum keluaran fasa yang diizinkan. |
| **$4** | `verbose_output` | - | `1` | 0 = Off, 1 = On | Mengaktifkan aliran telemetri streaming biner via USB untuk GUI monitor. |
| **$5** | `verbose_period` | ms | `10` | 1 – 1000 | Periode transmisi data telemetri USB (default 10 ms = laju 100 Hz). |
| **$6** | `open_loop_voltage` | Volt | `2.00` | 0.5 – 12.0 | Tegangan fasa yang diterapkan saat mode open-loop atau kalibrasi Hall. |
| **$7** | `accel_limit` | rad/s²| `100.00` | 0 – 10000 | Batas akselerasi sudut vektor listrik (0 = tanpa limit). |
| **$8** | `switchover_rpm` | RPM | `50.00` | 0 – 1000 | Ambang RPM transisi komutasi. |
| **$9** | `switchover_delay`| detik | `8.00` | 0 – 60 | Waktu tunda stabilitas transisi mode komutasi. |
| **$10** | `hall_offset_deg` | Derajat| `90.00` | 0.0 – 360.0 | Offset pergeseran sudut sensor Hall terhadap vektor gaya gerak listrik fasa. |
| **$11** | `vel_kp` | - | `0.050` | 0.001 – 10.0 | Penguatan Proporsional (*Kp*) kontroler loop kecepatan. |
| **$12** | `vel_ki` | - | `0.100` | 0.000 – 10.0 | Penguatan Integral (*Ki*) kontroler loop kecepatan. |
| **$13** | `accel_rpm_s` | RPM/s | `500.0` | 10.0 – 10000.0 | Batas laju akselerasi perubahan target RPM per detik (*ramp-rate*). |
| **$14** | `invert_direction` | - | `0` | 0 = Normal, 1 = Invert | Membalikkan arah rotasi maju/mundur motor secara digital. |
| **$15** | `startup_align_duty`| % | `5.0` | 0.0 – 100.0 | Duty cycle PWM saat penguncian awal sudut rotor sebelum komutasi. |
| **$16** | `startup_align_ms` | ms | `300` | 10 – 2000 | Durasi penahanan sudut rotor saat proses alignment startup. |

---

## 💻 Perintah Antarmuka Serial USB CDC (CLI & Diagnostics)

Saat motor driver dihubungkan ke komputer melalui port USB, board akan terdeteksi sebagai port COM virtual (*Virtual COM Port*). Gunakan serial monitor (115200 bps, Newline LF/CRLF) untuk mengakses konsol interaktif:

| Perintah | Fungsi & Contoh Penggunaan | Respon Balik Board |
| :--- | :--- | :--- |
| `$$` | Menampilkan seluruh daftar parameter konfigurasi beserta nilainya saat ini. | Rincian nilai parameter `$0` s/d `$16` dan status `ok`. |
| `$x=y` | Mengubah parameter ID `$x` dengan nilai baru `y` (contoh: `$13=750` untuk set limit akselerasi ke 750 RPM/s). | Respon `ok` jika berhasil, atau `error` jika ID salah. |
| `$save` | Menyimpan konfigurasi aktif saat ini ke memori Flash permanen. | `Config saved to flash.` disusul `ok`. |
| `$cal` | Menjalankan **Auto-Calibration Sensor Hall** (Opsional: `$cal=2.5` dengan tegangan 2.5V). Roda motor akan berputar perlahan maju dan mundur untuk mencari offset sudut sektor. | Urutan sektor Hall terdeteksi, nilai offset sudut baru, dan instruksi simpan. |
| `$?` | Memeriksa status real-time sensor Hall, status logika MOSFET fasa, dan sudut elektrik. | Laporan debug status fasa fasa motor. |
| `$can` atau `C` | Menampilkan diagnostik mendalam modul FDCAN1 (status Bus-Off, Error Counter TEC/REC, Tx FIFO level, kondisi logika pin fisik PB8/PB9, dan status watchdog). | Dump register FDCAN1 & status bus kesehatan CAN. |
| `$h` | Menampilkan menu bantuan perintah konsol. | Rangkuman daftar perintah CLI. |
| `S<rpm>` | Menyetel putaran motor langsung secara manual (contoh: `S1500` untuk memutar motor 1500 RPM maju, atau `S-1000` untuk mundur). | `ok: Motor berputar MAJU pada 1500.0 RPM`. |
| `T` | Mematikan dan menghentikan putaran motor seketika (*coast stop*). | `Motor Stopped`. |

### Format Paket Streaming Telemetri Biner (100 Hz)
Jika `$4=1`, board memancarkan paket data biner 37-byte setiap 10 ms ke USB untuk perangkat lunak GUI plot:
```text
[Header 2B: 0xAA 0xBB] [Pos: 4B float] [Vel: 4B float] [Vq: 4B float] [Target: 4B float] 
[Ia: 4B float] [Ib: 4B float] [Ic: 4B float] [Vbus: 4B float] [Mode: 1B] [CRC: 1B XOR] [Footer: 0x55]
```

---

## 🛠️ Panduan Kalibrasi Sudut Sensor Hall (`$cal`)

Sebelum motor digunakan untuk pertama kali di kendaraan:
1. Pastikan roda penggerak terangkat bebas dari tanah dan dapat berputar tanpa beban mekanis.
2. Hubungkan kabel USB ke komputer dan buka Serial Monitor.
3. Ketik perintah `$cal=2.0` (menggunakan tegangan uji 2.0 Volt).
4. Motor akan melakukan 3 tahap kalibrasi otomatis:
   - Mengunci posisi rotor pada sudut listrik 0° (*Electrical Angle Lock*).
   - Memutar vektor fasa perlahan ke arah maju (0° $\rightarrow$ 720°).
   - Memutar vektor fasa perlahan ke arah mundur (720° $\rightarrow$ 0°).
5. Sistem akan menghitung titik tengah transisi sektor Hall dan memperbarui nilai `$10` (`hall_offset_deg`).
6. Ketik `$save` untuk mengunci nilai kalibrasi ke dalam Flash.
