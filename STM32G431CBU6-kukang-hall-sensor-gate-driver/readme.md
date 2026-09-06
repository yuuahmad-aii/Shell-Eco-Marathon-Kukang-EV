# 📁 STM32G431 Firmware Motor Controller

Folder ini berisi kode *firmware* fundamental yang berjalan pada mikrokontroler **STM32G431CBU6** berarsitektur *Arm® Cortex®-M4* yang secara fisik tertanam pada papan motor *driver* utama.

## ⚙️ Peran dan Fungsi
Mikrokontroler ini memiliki satu misi krusial: **Mengendalikan rotasi Motor BLDC dengan sangat presisi dan efisien**. STM32 seri G4 dipilih karena memiliki instrumen internal yang kuat seperti *High-Resolution Timer* (HRTIM), CORDIC math co-processor, dan ADC berkecepatan tinggi yang ditujukan khusus untuk motor industri.

Algoritma kontrol yang berjalan di dalamnya secara *real-time*:
- **Hall Sensor Feedback:** Membaca status pergerakan *rotor* melalui sinyal Hall Sensor (H1, H2, H3) secara instan via pin *Interrupt* atau *Timer Input Capture*.
- **Komutasi (6-Step / FOC):** Menentukan MOSFET fasa mana (U, V, atau W) yang harus dihidupkan (High/Low) secara presisi pada fraksi milidetik tertentu.
- **Pembangkitan PWM:** Membuat pola denyut PWM untuk *Gate Driver* guna mengatur tegangan efektif yang mengalir ke motor (mempengaruhi akselerasi dan efisiensi tenaga beterai).
- **Proteksi Hardware:** Mendeteksi arus lebih (*Over-current*) atau suhu berlebih dan segera mematikan sinyal PWM demi mencegah meledaknya *driver* MOSFET jika terjadi korsleting.

## 💻 Cara *Build* dan *Flash*
Proyek ini biasanya berbasis STM32CubeIDE atau ekstensi CMake/Makefile. 
1. Buka folder ini di STM32CubeIDE.
2. Pastikan file `.ioc` sudah dikonfigurasi dengan frekuensi CPU optimal dan alokasi pin I/O yang akurat.
3. Jalankan `Build Project` (ikon palu) untuk menghasilkan binari `.elf` atau `.bin`.
4. Unggah (*Flash*) ke *board* fisik menggunakan *ST-Link* Programmer via *pin header* SWD (Serial Wire Debug).
