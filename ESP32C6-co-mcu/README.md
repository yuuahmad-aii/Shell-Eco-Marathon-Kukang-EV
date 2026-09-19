# 📁 ESP32-C6 IoT Co-Processor (Telemetry Cloud Gateway)

*Firmware* gerbang komunikasi nirkabel (*wireless telemetry gateway*) berdaya rendah yang berjalan pada mikrokontroler **ESP32-C6** (arsitektur 32-bit RISC-V berkecepatan 160 MHz, dilengkapi radio Wi-Fi 6 2.4 GHz, Bluetooth® 5, dan akselerator kriptografi perangkat keras).

Mikrokontroler ini bertugas sebagai **jembatan data nirkabel (IoT Gateway)**: menerima paket data telemetri yang telah disaring dan diagregasi oleh STM32F446RE melalui komunikasi serial UART kecepatan tinggi, kemudian secara instan mentransmisikannya ke server awan **Firebase Realtime Database (RTDB)** melalui koneksi HTTPS terenkripsi agar dapat dipantau secara langsung (*real-time live telemetry*) oleh kru balap di *paddock/pit stop*.

---

## 📌 Alokasi Pin & Antarmuka Perangkat Keras

ESP32-C6 dirancang untuk bekerja secara mandiri berdampingan dengan STM32F446RE:

| Pin ESP32-C6 | Fungsi Periferal | Terhubung ke (STM32F446RE) | Deskripsi & Konfigurasi |
| :---: | :---: | :---: | :--- |
| **GPIO 4** | `UART1_RXD` | Pin **PA2** (`USART2_TX`) | Jalur penerimaan data serial telemetri JSON dari STM32F4 (115200 bps). |
| **GPIO 5** | `UART1_TXD` | Pin **PA3** (`USART2_RX`) | Jalur pengiriman data balasan / perintah uplink ke STM32F4 (115200 bps). |
| **GND** | Ground Bersama | Pin `GND` | Referensi potensial ground bersama wajib tersambung erat. |
| **3V3 / 5V** | Power Supply | VCC Sistem Kokpit | Catu daya stabil untuk modul mikrokontroler. |
| **Antena RF** | Wi-Fi 2.4 GHz | Modem 4G / Wi-Fi Kokpit | Menghubungkan kendaraan ke jaringan seluler sirkuit. |

---

## ⚙️ Cara Kerja Firmware & Arsitektur Dual-Task FreeRTOS

Firmware dibangun di atas kerangka kerja **ESP-IDF** dengan arsitektur multithreading **FreeRTOS** terpisah antara tugas penerimaan data serial (*producer*) dan tugas transmisi data internet (*consumer*).

```mermaid
flowchart TD
    subgraph STM32F4_Transmitter
        F4[STM32F4 USART2] -->|2 Hz Non-Blocking JSON Stream| UARTLine[Serial Cable 115200 bps]
    end

    subgraph ESP32C6_FreeRTOS_Architecture
        UARTLine --> UARTHW[UART1 Hardware FIFO GPIO 4/5]
        UARTHW --> Task1["Task 1: uart_rx_task\n(Priority: MAX-1, Stack: 6KB)"]
        
        Task1 -->|1. Validasi Kurung '{...}'| Validate{Valid JSON?}
        Validate -->|No| Drop[Discard Glitch]
        Validate -->|Yes| Parse[Extract Summary Metrics & Log]
        Parse --> Mutex[Take s_telemetry_mutex]
        Mutex --> Mailbox[(Shared Latest Mailbox\ns_latest_telemetry)]
        Mailbox --> Notify[xTaskNotifyGive to Task 2]

        Notify --> Task2["Task 2: firebase_tx_task\n(Priority: 5, Stack: 8KB)"]
        Task2 -->|Wakeup by Notification| FetchMailbox[Fetch Latest Telemetry from Mailbox]
        FetchMailbox --> TLSCheck{WiFi Connected?}
        TLSCheck -->|Yes| HTTPSClient[Persistent HTTP PUT Client\nkeep_alive_enable = true]
    end

    subgraph Cloud_Infrastructure
        HTTPSClient -->|TLS 1.2/1.3 Handshake Reused\nLatency ~150ms| Firebase[(Firebase Realtime Database\n/telemetry.json)]
        Firebase -->|Websocket / SSE| WebApp[kukang-ev-telemetry-webapp\nPaddock Pit Stop Monitor]
    end
```

### 1. `uart_rx_task` (Tugas Penerimaan Serial - Prioritas Tertinggi)
- Dijalankan pada prioritas `configMAX_PRIORITIES - 1` agar tidak pernah kehilangan byte data serial yang masuk.
- Membaca aliran byte dari buffer cincin (*ring buffer*) sebesar 2048 bytes.
- Mengumpulkan karakter hingga menemukan pembatas baris baru `\n`.
- **Validasi Integritas Frame:** Memverifikasi secara ketat bahwa panjang string > 10 karakter, berawalan `{`, dan berakhiran `}` untuk mencegah pengiriman data terpotong akibat interferensi elektromagnetik.
- **Penyimpanan Bebas Hambatan (*Thread-Safe Mailbox*):** Menyalin frame JSON terbaru ke buffer bersama `s_latest_telemetry` menggunakan semafor mutex `s_telemetry_mutex`. Jika mutex sedang sibuk, task tidak akan memblokir penerimaan serial (`timeout = 0`).
- **Pemicuan Seketika (*Direct Task Notification*):** Memanggil `xTaskNotifyGive(s_firebase_task_handle)` agar tugas pengiriman awan segera terbangun seketika saat paket baru tiba.

### 2. `firebase_tx_task` (Tugas Transmisi Awan Firebase)
- Bertugas mengeksekusi pengiriman HTTP method `PUT` ke Firebase Realtime Database REST API.
- **Latensi Rendah via Persistent HTTPS (*Keep-Alive Session*):**
  Menggunakan opsi `keep_alive_enable = true` pada komponen `esp_http_client`. Fitur ini mempertahankan sesi koneksi TCP dan enkripsi TLS/SSL agar tetap terbuka antar-siklus pengiriman. Hasilnya:
  $$\text{Latensi Handshake Penuh: } \sim 2500\,\text{ms} \quad \longrightarrow \quad \text{Latensi Reused TLS: } \sim 150\,\text{ms}$$
  Dengan optimasi ini, telemetri live di pit stop memiliki responsivitas tinggi mendekati waktu nyata.
- **Sinkronisasi Waktu SNTP:** Menjalankan fungsi `obtain_time()` via `pool.ntp.org` saat pertama kali terhubung ke Wi-Fi. Waktu sistem yang akurat mutlak diperlukan agar tumpukan TLS dapat memvalidasi masa berlaku sertifikat keamanan server Firebase.
- **Ketahanan Jaringan (*Auto-Recovery*):** Jika koneksi terputus di area tanpa sinyal seluler (*tunnel/blind spot*), task secara otomatis membersihkan pegangan HTTP client (`esp_http_client_cleanup`) dan melakukan inisialisasi ulang begitu koneksi Wi-Fi pulih kembali.

### 3. Pengendali Acara Wi-Fi (*Wi-Fi Event Handler*)
- Bekerja dalam mode Station (`WIFI_MODE_STA`).
- Jika terputus dari Access Point (`WIFI_EVENT_STA_DISCONNECTED`), driver Wi-Fi secara otomatis melakukan rekoneksi ulang hingga 10 kali percobaan berturut-turut tanpa memicu *kernel panic* atau kebocoran memori.

---

## 🎛️ Parameter Konfigurasi yang Dapat Diubah

Seluruh parameter penting didefinisikan secara modular pada bagian awal file [`main.c`](file:///c:/Users/ahmad/Documents/Project-Yuuahmad/driver-servo/Shell-Eco-Marathon-Kukang-EV/ESP32C6-co-mcu/main/main.c):

```c
// -----------------------------------------------------------------------------
// USER CONFIGURATION
// -----------------------------------------------------------------------------
#define WIFI_SSID       "ESP32TEST"
#define WIFI_PASS       "PASS_TEST"
#define FIREBASE_URL    "https://kukang-ev-default-rtdb.asia-southeast1.firebasedatabase.app/telemetry.json?auth=AIzaSyCkwhZaXiJ1YkHU792jhgdiCZVIZLXcLu0"

// UART Configuration
#define UART_NUM        UART_NUM_1
#define UART_TX_PIN     5
#define UART_RX_PIN     4
#define UART_BAUD_RATE  115200
#define BUF_SIZE        1024
```

| Parameter | Deskripsi & Saran Pengaturan |
| :--- | :--- |
| `WIFI_SSID` | Nama SSID dari modem Wi-Fi portabel / MiFi 4G / Tethering HP di dalam mobil. |
| `WIFI_PASS` | Kata sandi keamanan WPA2-PSK dari modem Wi-Fi kokpit. |
| `FIREBASE_URL` | Alamat endpoint REST API Firebase Realtime Database. Wajib mencakup path `.json` dan query parameter `auth=<TOKEN>` untuk otentikasi tulis basis data. |
| `UART_NUM` | Nomor periferal perangkat keras UART ESP32-C6 (menggunakan `UART_NUM_1`). |
| `UART_TX_PIN` | Pin GPIO transmisi serial (Default: `GPIO 5`). |
| `UART_RX_PIN` | Pin GPIO penerimaan serial (Default: `GPIO 4`). |
| `UART_BAUD_RATE` | Kecepatan transmisi data serial (Wajib sama dengan STM32F4: `115200` bps). |
| `BUF_SIZE` | Alokasi memori buffer penampung string JSON (Default: `1024` byte). |
| `timeout_ms` | Batas waktu tunggu respon HTTP dari server (dikonfigurasi `4000` ms di `firebase_tx_task`). |

---

## 📋 Struktur Data JSON Telemetri

Data yang diterima dari STM32F4 dan diteruskan langsung oleh ESP32-C6 ke Firebase memiliki format pasangan kunci-nilai (*key-value pairs*) berikut:

```json
{
  "ts": 142500,     // Timestamp milidetik sejak sistem dinyalakan
  "y": 2026,        // Tahun (dari kalender satelit GPS)
  "m": 9,           // Bulan
  "d": 19,          // Hari
  "h": 10,          // Jam (UTC)
  "min": 15,        // Menit
  "s": 42,          // Detik
  "v": 1,           // Validitas waktu GPS (1 = Valid, 0 = Belum valid)
  "ax": 0.02,       // Akselerasi sumbu X (g)
  "ay": 0.11,       // Akselerasi sumbu Y (g)
  "az": 0.98,       // Akselerasi sumbu Z gravitasi (g)
  "gx": 0.1,        // Kecepatan sudut Gyro X (derajat/detik)
  "gy": -0.2,       // Kecepatan sudut Gyro Y (derajat/detik)
  "gz": 0.0,        // Kecepatan sudut Gyro Z (derajat/detik)
  "alt": 26.1,      // Ketinggian barometer BMP280 di atas permukaan laut (meter)
  "lat": -7.284512, // Koordinat garis lintang GPS
  "lon": 112.795412,// Koordinat garis bujur GPS
  "galt": 25.4,     // Ketinggian satelit GPS (meter)
  "pd": 1.2,        // Nilai PDOP (Positional Dilution of Precision)
  "fix": 3,         // Kualitas fix GPS (3 = 3D Fix, 2 = 2D, 0 = No Fix)
  "ns": 14,         // Jumlah satelit GPS yang sedang terkunci
  "sl": 22.4,       // Kecepatan roda kiri kendaraan (km/jam)
  "sr": 22.5,       // Kecepatan roda kanan kendaraan (km/jam)
  "vbus": 24.35,    // Tegangan bus DC baterai utama (Volt)
  "iq": 4.28,       // Arus torsi motor penggerak aktif (Ampere)
  "r1": 342,        // Putaran RPM roda kanan
  "r2": 340,        // Putaran RPM roda kiri
  "t1": 42.50,      // Suhu motor listrik dari sensor DS18B20 #1 (°C)
  "t2": 38.25       // Suhu inverter / baterai dari sensor DS18B20 #2 (°C)
}
```

---

## 💻 Kompilasi dan Flash Firmware

Firmware ini dikompilasi menggunakan **ESP-IDF** (versi v5.1 ke atas yang mendukung target `esp32c6`):

1. Buka terminal ESP-IDF di direktori `ESP32C6-co-mcu/`:
   ```bash
   idf.py set-target esp32c6
   ```
2. Lakukan konfigurasi bila diperlukan:
   ```bash
   idf.py menuconfig
   ```
3. Kompilasi, flash ke board fisik, dan buka serial monitor:
   ```bash
   idf.py build flash monitor
   ```
4. Verifikasi melalui log bahwa Wi-Fi terhubung, waktu SNTP tersinkronisasi, dan respon pengiriman ke Firebase menampilkan status kode `200`:
   ```text
   I (3240) Telemetry_Coprocessor: got ip: 192.168.43.102
   I (5120) Telemetry_Coprocessor: Current time: Sat Sep 19 10:15:40 2026
   I (6200) Telemetry_Coprocessor: -> Rx Telemetry: Vbus=24.35V, Iq=4.28A, RPM1=342, RPM2=340
   I (6350) Telemetry_Coprocessor: Firebase PUT OK (200)
   ```
