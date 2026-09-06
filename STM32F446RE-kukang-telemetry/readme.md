# 📁 STM32F446RE Telemetry & Data Logger

Repositori ini menyimpan *firmware* otak dari sistem **Akuisisi Data Kendaraan**, yang dibangun dan berjalan di atas mikrokontroler **STM32F446RE**. Mikrokontroler ini bertindak selaku pusat pengumpul informasi (*sensor hub*) dan pencatat rekam jejak (*data logger*) utama selama balapan kompetisi Shell Eco-Marathon berlangsung.

## 🛰️ Sensor dan Antarmuka yang Ditangani
- **IMU MPU9250:** Membaca nilai Akselerasi (G-Force 3 sumbu) dan *Angular Velocity* (Gyro 3 sumbu) menggunakan jalur komunikasi SPI kecepatan tinggi. Data ini amat krusial untuk menganalisis dinamika suspensi, rem, dan keseimbangan mobil saat menikung tajam.
- **Barometer BMP280:** Mengukur tekanan dan suhu udara sekitar untuk mengalkulasi ketinggian (*Altitude*) secara absolut terhadap permukaan laut.
- **Modul GPS (Serial UART):** Menangkap sinyal satelit untuk mendapatkan koordinat posisi Latitude dan Longitude secara *real-time* via komunikasi NMEA/UBLOX.
- **Antarmuka MicroSD (SPI/SDIO):** Menyimpan seluruh gabungan parameter data di atas.

## 📦 Logika Utama Program (`main.c`)
Papan ini terus-menerus mengambil sampel data dari sensor (misalnya pada rasio ~10 Hz). Setiap siklus akan membungkus data tersebut ke dalam bentuk paket biner rapat yang efisien memori (*C Struct* ukuran tetap, contoh: 58 bytes per baris). Data biner ini langsung ditendang dan disimpan ke kartu MicroSD.

**Fitur Proteksi *Logger*:** Jika sistem mendeteksi gagalnya komunikasi dengan MicroSD (entah karena pin kendor, memori penuh, korup, atau *mount error*), sebuah fungsi peringatan akan segera menyalakan lampu indikator **LED Merah Berkedip / Solid** secara otomatis di *dashboard*, memberitahu pengemudi bahwa telemetri sedang mati. Pengemudi kemudian dapat menggunakan **Tombol 4** untuk menghapus notifikasi peringatan tersebut dan memerintahkan *logger* memulai ulang file `log...bin` yang baru dari awal.

## 💻 Cara *Build* dan Analisis
1. *Firmware* ini di-*build* menggunakan STM32CubeIDE dengan integrasi FatFS yang telah dioptimasi untuk rutinitas penulisan *SD Card* dengan latensi penulisan sesingkat mungkin.
2. File biner hasil rekaman (contoh: `log29.bin`) nantinya dapat langsung dimasukkan ke dalam **kukang-ev-telemetry-webapp** atau diuraikan menggunakan skrip di folder `python_gui`.
