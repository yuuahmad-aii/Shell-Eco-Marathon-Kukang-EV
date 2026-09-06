# 📁 Python GUI & Offline Analytics Tools

Folder ini menaungi perkakas pendukung komputasi ilmiah dan analisis *desktop* berbasis skrip pemrograman **Python**. 
Meski proyek sudah memiliki WebApp canggih, folder ini memegang fungsi esensial sebagai "Kotak Perkakas" (*Toolbox*) tambahan bagi insinyur mekanik dan telemetri jika sewaktu-waktu membutuhkan kemampuan bedah data biner tingkat rendah (*low-level parsing*), atau sekadar butuh visualisasi *offline* tanpa perlu repot membangun/menyewa *server Node.js*.

## 🧰 Fungsionalitas Skrip

1. **Parser & *Decoder* Data Mentah MicroSD (`plot_log.py`)**: 
   - Sebagaimana kita ketahui, demi mencapai kecepatan tertinggi operasi tulis MicroSD, papan *Logger* (STM32F4) hanya menyimpan data variabel secara memori berdempetan (format biner mentah `.bin`) sebagai bongkahan blok 58 *byte* kode bahasa C. Wujud asli data tersebut sepenuhnya berantakan layaknya mesin.
   - Skrip Python ini diprogram menggunakan modul inti `struct` dari Python. Ia akan menebas susunan data 58-*byte* tersebut balok demi balok, dan membukanya paksa (*unpack*) untuk menerjemahkan bit biner itu kembali menjadi deretan tipe data *Float* dan *Integer* yang bisa dibaca akal sehat manusia (Contoh: `Timestamp`, `Accel_X`, `Latitude`, `Satellites`).
   - Begitu diterjemahkan, *script* ini dapat mengekspornya menjadi format lajur kolom tradisional **`.csv`** (yang sangat disukai oleh Excel dan Matlab), lalu melempar isinya ke modul **`matplotlib.pyplot`** untuk diplot melintang menjadi tampilan grafik analisis 2D interaktif (*cross-hair hovering*).
2. **Penampil Telemetri Statis (`telemetry_viewer.py`)**: (Contoh alternatif/antarmuka statis)
   - Adalah GUI primitif portabel (*Tkinter/PyQt*) untuk menggambar grafik statistik rangkuman. Membantu anggota regu yang buta baris perintah (*command line*) untuk tinggal mengklik tombol *Open File* dan membaca grafik balapan di dalam tenda secara luring (*offline*) saat waktu terpepet.

## 💻 Panduan Singkat Eksekusi
Agar dapat mengeksekusi peralatan mumpuni di dalam folder ini, syarat satu-satunya hanyalah keberadaan *Interpreter* Python 3 modern dan *library* penganalisa numerik:
```bash
pip install matplotlib pandas numpy
```
Gunakan *Command Prompt* untuk mengurai data Anda layaknya saintis:
```bash
# Menjalankan parser dan men-decode file log MicroSD
python plot_log.py board_telemetry/log29.bin
```
