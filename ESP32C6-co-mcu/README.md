# 📁 ESP32C6 Co-Microcontroller (IoT)

Folder ini mengandung *firmware* sekunder / *co-processor* berbasis cip nirkabel hemat daya dari Espressif: **ESP32-C6**. Meskipun papan telemetri utama (STM32F4) sangat cepat dalam mengolah data dari sensor-sensor fisik, ia tetaplah *chip* luring (*offline*) yang tidak memiliki radio internet. Di sinilah ESP32-C6 hadir dan mengambil peran utama.

## 📡 Misi Sistem Utama
ESP32C6 ditugaskan khusus sebagai "Kurir Komunikasi Data Internet".
1. **Sinkronisasi Data Lokal:** Terhubung dengan papan STM32F4 (menggunakan antarmuka UART/I2C/SPI). ESP32 ini akan senantiasa bertugas "mendengarkan" aliran paket data telemetri yang sudah difilter dan dikumpulkan oleh STM32.
2. **Koneksi Jaringan Seluler / Wi-Fi:** Secara otonom mencari koneksi dan menyambungkan dirinya ke *tethering hotspot* / modem 4G yang dipasang di dalam kokpit mobil balap, serta membangun sesi TLS dengan server *cloud*.
3. **Pengiriman Data (*Telemetry Publishing*):** Memformat nilai-nilai mentah (Kecepatan, G-Force, GPS, Ketinggian, Suhu, Daya Baterai) menjadi paket muatan yang efisien (biasanya protokol JSON / RTDB API) dan menembakkannya menuju antarmuka **Firebase Real-time Database (RTDB)** secara masif setiap sekian detik.

Berkat adanya modul inilah para kru teknis dan *race engineer* yang sedang berada di area *pit stop* / *paddock* bisa memantau laju vital mobil di atas aspal sirkuit secara sungguhan pada saat itu juga (*live telemetry*).

## 🔧 Platform Pengembangan
Dapat dikembangkan dan di-*build* menggunakan **ESP-IDF** (Espressif IoT Development Framework) profesional ataupun **PlatformIO** (jika memakai *core* Arduino yang lebih ramah pengguna). 
Kode ini telah didesain tangguh dengan menonjolkan fitur penjaga ketahanan jaringan (*auto-reconnect*), sehingga jika mobil balap melewati terowongan atau *blind spot* sinyal seluler, ESP32C6 akan langsung kembali tersambung dan melanjutkan pekerjaannya tanpa *crash*.
