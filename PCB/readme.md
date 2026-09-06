# 📁 PCB (Hardware Design & Schematics)

Folder ini dikhususkan untuk semua kebutuhan desain perangkat keras elektronik dari proyek mobil listrik **Kukang EV**. Jantung utamanya adalah desain skematik dan cetak PCB (*Printed Circuit Board*) dari *Motor Controller* berdaya tinggi (sering disebut *Behemoth* atau *Kukang Driver*).

## 🛠️ Apa yang ada di dalam sini?
1. **File Sumber Desain:** File berekstensi `.epro2` atau semacamnya yang merupakan file sumber yang bisa Anda buka, edit, dan rancang ulang menggunakan perangkat lunak **EasyEDA Pro**. File ini mengandung desain jaring sirkuit (skematik) beserta lapisan tembaga PCB secara utuh.
2. **Dokumen PDF:** Versi ekspor dari skematik yang bisa dibaca dan ditinjau tanpa perlu menginstal *software* desain. Sangat berguna untuk mekanik, *electrical engineer*, atau divisi lain untuk melakukan *troubleshooting* jika terjadi korsleting atau putusnya jalur kabel.

## ⚡ Karakteristik Hardware (Inverter)
Inverter yang didesain difokuskan pada:
- Topologi Inverter 3-Fasa (Half-Bridge ganda) menggunakan komponen MOSFET/IGBT performa tinggi.
- Arus puncak (Peak Current) yang mampu menangani beban ekstrem akselerasi motor BLDC.
- Desain *Gate Driver* khusus dengan komponen isolasi guna mencegah interferensi EMI (*Electromagnetic Interference*) tinggi pada logika mikrokontroler.

## 📖 Cara Menggunakan
Untuk melihat skematik, silakan buka file PDF yang tersedia. Untuk mengedit jalur atau memodifikasi komponen (misal: mengganti jenis MOSFET), impor file proyek mentahnya ke dalam **EasyEDA Pro**, lakukan modifikasi pada *Schematic Editor*, perbarui pad ke *PCB Layout*, lalu *generate* file *Gerber* untuk diserahkan ke pabrik manufaktur PCB.
