import struct
import csv
import sys
import os

# Define the struct format based on LogData definition in C
# uint32_t (I)
# uint16_t (H)
# uint8_t (B) x 6
# float (f) x 11
# uint8_t (B) x 2
# Total bytes: 4 + 2 + 6 + (4*11) + 2 = 58 bytes
struct_fmt = '<IHBBBBBBfffffffffffBB'
struct_len = struct.calcsize(struct_fmt)

def convert_bin_to_csv(input_bin, output_csv):
    if not os.path.exists(input_bin):
        print(f"Error: File '{input_bin}' tidak ditemukan!")
        return
        
    print(f"Membaca file binary: {input_bin} ...")
    
    with open(input_bin, 'rb') as f_in, open(output_csv, 'w', newline='') as f_out:
        writer = csv.writer(f_out)
        
        # Tulis Header (Baris Pertama) untuk Excel
        header = [
            'Timestamp_ms', 'GPS_Year', 'GPS_Month', 'GPS_Day', 
            'GPS_Hour', 'GPS_Min', 'GPS_Sec', 'Time_Valid', 
            'Accel_X(G)', 'Accel_Y(G)', 'Accel_Z(G)', 
            'Gyro_X(deg/s)', 'Gyro_Y(deg/s)', 'Gyro_Z(deg/s)', 
            'Baro_Altitude(m)', 'Latitude', 'Longitude',
            'GPS_Altitude(m)', 'PDOP', 'Fix_Type', 'Satellites'
        ]
        writer.writerow(header)
        
        row_count = 0
        while True:
            data = f_in.read(struct_len)
            
            # Berhenti jika data habis atau ukurannya tidak sesuai dengan struct
            if not data or len(data) != struct_len:
                break
                
            unpacked = struct.unpack(struct_fmt, data)
            
            # Format baris (Bisa modifikasi format desimal di sini jika perlu)
            # unpacked = (timestamp, year, month, day, hour, min, sec, time_valid,
            #             ax, ay, az, gx, gy, gz, alt, lat, lon)
            
            # Tulis ke CSV
            writer.writerow(unpacked)
            row_count += 1
            
    print(f"Selesai! Berhasil mengonversi {row_count} baris data.")
    print(f"File CSV tersimpan di: {output_csv}")

if __name__ == '__main__':
    # Jika dijalankan melalui terminal dengan argumen
    if len(sys.argv) == 3:
        input_file = sys.argv[1]
        output_file = sys.argv[2]
        convert_bin_to_csv(input_file, output_file)
    # Jika dijalankan langsung (tanpa argumen)
    elif len(sys.argv) == 1:
        # Default nama file
        default_input = 'log1.bin'
        default_output = 'log1_converted.csv'
        print("Anda menjalankan script tanpa argumen.")
        print(f"Mencoba mencari file default '{default_input}'...\n")
        convert_bin_to_csv(default_input, default_output)
    else:
        print("Penggunaan: python bin_to_csv.py <nama_file_input.bin> <nama_file_output.csv>")
        print("Contoh: python bin_to_csv.py log1.bin hasil_log1.csv")
