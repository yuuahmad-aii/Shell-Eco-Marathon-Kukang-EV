import struct
import csv
import sys
import os
import math

# Define the struct formats based on LogData definition in C
# Format baru (90 bytes):
# uint32_t (I), uint16_t (H), uint8_t (B) x 6, float (f) x 11, uint8_t (B) x 2, float (f) x 8
# Total: 4 + 2 + 6 + (4*11) + 2 + (4*8) = 90 bytes
struct_fmt_90 = '<IHBBBBBBfffffffffffBBffffffff'
struct_len_90 = struct.calcsize(struct_fmt_90)

# Format legacy (58 bytes):
struct_fmt_58 = '<IHBBBBBBfffffffffffBB'
struct_len_58 = struct.calcsize(struct_fmt_58)

def is_valid_header(buf, offset, struct_len):
    """
    Memvalidasi apakah 12 byte pertama dari sebuah record memiliki header yang masuk akal:
    - year: 1980 (default GPS sebelum lock) atau antara 2020 dan 2035
    - month: 1..12
    - day: 1..31
    - hour: 0..23, min: 0..59, sec: 0..59
    - time_valid: 0 atau 1
    """
    if offset + 12 > len(buf):
        return False
    ts, yr, mo, day, hr, mn, sc, tv = struct.unpack('<IH6B', buf[offset:offset+12])
    if yr != 1980 and not (2020 <= yr <= 2035):
        return False
    if not (1 <= mo <= 12):
        return False
    if not (1 <= day <= 31):
        return False
    if not (0 <= hr <= 23):
        return False
    if not (0 <= mn <= 59):
        return False
    if not (0 <= sc <= 59):
        return False
    if tv not in (0, 1):
        return False
    return True

def convert_bin_to_csv(input_bin, output_csv):
    if not os.path.exists(input_bin):
        print(f"Error: File '{input_bin}' tidak ditemukan!")
        return
        
    file_size = os.path.getsize(input_bin)
    
    with open(input_bin, 'rb') as f_in:
        buffer = f_in.read()
        
    # Deteksi format otomatis dengan memeriksa timestamp dan header pada offset 58 vs 90
    is_90_byte = False
    if len(buffer) >= 116:
        ts0 = struct.unpack('<I', buffer[0:4])[0]
        ts58 = struct.unpack('<I', buffer[58:62])[0]
        ts90 = struct.unpack('<I', buffer[90:94])[0] if len(buffer) >= 180 else 0xFFFFFFFF
        
        is58_valid = (ts58 >= ts0 and (ts58 - ts0) < 10000) and is_valid_header(buffer, 58, struct_len_58)
        is90_valid = (ts90 >= ts0 and (ts90 - ts0) < 10000) and is_valid_header(buffer, 90, struct_len_90)
        
        if is90_valid and not is58_valid:
            is_90_byte = True
        elif is58_valid and not is90_valid:
            is_90_byte = False
        elif file_size % struct_len_90 == 0 and file_size % struct_len_58 != 0:
            is_90_byte = True
        elif file_size % struct_len_58 == 0 and file_size % struct_len_90 != 0:
            is_90_byte = False
        else:
            is_90_byte = (file_size % struct_len_90 == 0)

    struct_fmt = struct_fmt_90 if is_90_byte else struct_fmt_58
    struct_len = struct_len_90 if is_90_byte else struct_len_58

    print(f"Membaca file binary: {input_bin} ({'Format 90-byte Lengkap' if is_90_byte else 'Format 58-byte Legacy'}) ...")
    
    with open(output_csv, 'w', newline='') as f_out:
        writer = csv.writer(f_out)
        
        # Tulis Header untuk Excel/Web
        if is_90_byte:
            header = [
                'Timestamp_ms', 'GPS_Year', 'GPS_Month', 'GPS_Day', 
                'GPS_Hour', 'GPS_Min', 'GPS_Sec', 'Time_Valid', 
                'Accel_X(G)', 'Accel_Y(G)', 'Accel_Z(G)', 
                'Gyro_X(deg/s)', 'Gyro_Y(deg/s)', 'Gyro_Z(deg/s)', 
                'Baro_Altitude(m)', 'Latitude', 'Longitude',
                'GPS_Altitude(m)', 'PDOP', 'Fix_Type', 'Satellites',
                'SpeedLeft', 'SpeedRight', 'Vbus', 'Iq',
                'RPM_TIM2CH1', 'RPM_TIM5CH2', 'Temp1', 'Temp2'
            ]
        else:
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
        resync_count = 0
        offset = 0
        buf_len = len(buffer)
        
        while offset + struct_len <= buf_len:
            if is_valid_header(buffer, offset, struct_len):
                data = buffer[offset:offset+struct_len]
                unpacked = struct.unpack(struct_fmt, data)
                
                # Validasi nilai float agar tidak memasukkan nilai denormal / corrupt akibat sektor terpotong
                is_sane = True
                for val in unpacked:
                    if isinstance(val, float):
                        if math.isnan(val) or math.isinf(val) or abs(val) > 1e12 or (0 < abs(val) < 1e-15):
                            is_sane = False
                            break
                
                if is_sane:
                    writer.writerow(unpacked)
                    row_count += 1
                offset += struct_len
            else:
                # Frame Sync: lakukan pencarian byte maju untuk mengunci kembali ke header valid berikutnya
                resync_count += 1
                offset += 1
                while offset + struct_len <= buf_len:
                    if is_valid_header(buffer, offset, struct_len):
                        break
                    offset += 1
            
    print(f"Selesai! Berhasil mengonversi {row_count} baris data (resync events: {resync_count}).")
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
