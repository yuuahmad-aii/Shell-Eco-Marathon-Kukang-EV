# STM32G431 Open-Loop SVPWM Motor Controller (IR2103)

Proyek ini adalah *firmware* kontrol motor BLDC/PMSM 3-fasa secara *open-loop* menggunakan mikrokontroler **STM32G431CBU6**. Proyek ini secara spesifik dirancang untuk mengendalikan rangkaian *inverter* yang menggunakan *gate driver* **IR2103** dan MOSFET **IRFZ44N**.

## Fitur Utama

- **Open-Loop Space Vector PWM (SVPWM)**: Menghasilkan gelombang tegangan sinusoidal 3-fasa dengan teknik *midpoint clamping* untuk memaksimalkan tegangan *DC bus*.
- **Hardware CORDIC Accelerator**: Memanfaatkan *peripheral* CORDIC bawaan STM32G4 untuk komputasi trigonometri (Sine/Cosine) dengan sangat cepat, sehingga tidak membebani prosesor pada *loop* utama.
- **Dukungan IR2103 Gate Driver**: Menggunakan TIM1 untuk men- *generate* PWM *complementary* (CHx dan CHxN) beserta *deadtime*. Karena arsitektur chip IR2103 menggunakan logika *LIN* (Low-Side Input) yang bersifat *active-low*, polaritas pin *complementary* pada *timer* diubah menjadi `TIM_OCNPOLARITY_LOW` dan *idle state*-nya diset `HIGH`.
- **Antarmuka USB CDC**: Mendukung pengaturan parameter dan kontrol motor melalui *Virtual COM Port* USB dengan format perintah bergaya GRBL.
- **Penyimpanan Parameter (Non-Volatile)**: Konfigurasi sistem dapat disimpan ke memori Flash internal (Halaman 63) sehingga tidak hilang saat mikrokontroler dimatikan.
- **High-Speed Telemetry**: Mengirimkan paket data biner (*telemetry*) secara *non-blocking* untuk ditampilkan pada GUI atau *dashboard* eksternal.

## Arsitektur Hardware
- **MCU**: STM32G431CBU6 (berjalan pada 170 MHz)
- **Gate Driver**: 3x IR2103 (HIN = Active High, LIN = Active Low)
- **MOSFET**: 6x IRFZ44N
- **Komunikasi**: USB Type-C (Virtual COM Port)

## Daftar Perintah Serial (USB CDC)

Anda dapat menggunakan aplikasi *Serial Terminal* (misal: PuTTY, HTerm, atau Serial Monitor pada baudrate apa saja) untuk mengontrol motor.

### Perintah Kontrol
- `S<val>`: Memutar motor dengan target kecepatan *open-loop*. Contoh: `S50` (50 Rad/s).
- `T` atau `S0`: Menghentikan motor dan mengatur *duty cycle* menjadi 0 (atau mematikan output).

### Perintah Konfigurasi
- `$$` : Menampilkan seluruh konfigurasi saat ini.
- `$x=y` : Mengatur parameter ID `x` menjadi nilai `y`.
- `$save` : Menyimpan pengaturan saat ini ke memori Flash secara permanen.
- `$h` : Menampilkan menu bantuan (*help*).

### Daftar Parameter (GRBL Style)
| ID | Deskripsi | Default |
|---|---|---|
| `$0` | Frekuensi PWM (Hz) | `20000` |
| `$1` | Jumlah Pole Pairs Motor | `7` |
| `$2` | Tegangan DC Bus (Volt) | `24.00` |
| `$3` | Tegangan Output Maksimal (Volt) | `24.00` |
| `$4` | Mode Verbose Telemetry (0=Mati, 1=Nyala) | `1` |
| `$5` | Periode Telemetry (ms) | `10` |
| `$6` | Tegangan Open-Loop (Volt) | `2.00` |
| `$7` | Batasan Akselerasi (Rad/s²) | `50.00` |

## Alur Kerja Program (Software Architecture)
1. **`main.c`**: Pusat inisialisasi *hardware* (TIM1, USB, CORDIC) dan penanganan *loop* utama (`while(1)`). Menghitung `dt` (delta waktu) menggunakan register *DWT Cycle Counter* untuk menjaga presisi pergerakan *open-loop*.
2. **`config.c` & `config.h`**: Modul yang bertugas melakukan *parsing* perintah dari USB, mencetak nilai dari USB, dan memanajemen memori Flash.
3. **`open_loop.c` & `open_loop.h`**: Berisi seluruh algoritma pengendalian. Mulai dari pembaruan sudut *open-loop*, CORDIC trigonomentri, transformasi Inverse Park & Inverse Clarke, perhitungan *SVPWM Duty Cycle*, hingga pengaplikasian tegangan ke register komparasi TIM1 (CCR1, CCR2, CCR3).
4. **`usbd_cdc_if.c`**: File bawaan *middleware* STMicroelectronics yang telah dimodifikasi agar segera meneruskan (forward) data RX ke fungsi `Process_USB_Receive()` di `main.c`.

## Cara Penggunaan
1. Hubungkan board STM32G431 ke PC melalui kabel USB.
2. Kompilasi dan *flash* proyek ini menggunakan **STM32CubeIDE**.
3. Buka *Serial Terminal* ke COM Port yang terdeteksi dari board STM32.
4. Pastikan tegangan DC (contoh: 12V - 24V) tersambung ke inverter dan suplai untuk gate driver IR2103 (biasanya 12V) sudah aktif.
5. Kirim perintah `$6=3.5` (untuk meningkatkan tegangan open-loop apabila motor kurang torsi saat tanpa beban).
6. Kirim `S20` untuk menjalankan motor perlahan.
7. Ketik `T` untuk menghentikan motor.
