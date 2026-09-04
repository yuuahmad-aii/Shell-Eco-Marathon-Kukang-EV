import streamlit as st
import struct
import pandas as pd
import folium
from streamlit_folium import st_folium
import plotly.graph_objects as go
from plotly.subplots import make_subplots

# Konfigurasi Halaman Streamlit
st.set_page_config(page_title="Kukang EV Telemetry", page_icon="🏎️", layout="wide")

# Definisi format struct binary data logger
struct_fmt = '<IHBBBBBBfffffffffffBB'
struct_len = struct.calcsize(struct_fmt)

@st.cache_data
def parse_binary_log(file_bytes):
    """Mengekstrak file binary menjadi Pandas DataFrame"""
    data_list = []
    offset = 0
    
    while offset + struct_len <= len(file_bytes):
        chunk = file_bytes[offset:offset+struct_len]
        unpacked = struct.unpack(struct_fmt, chunk)
        
        timestamp_s = unpacked[0] / 1000.0
        
        # Validasi GPS (Abaikan kordinat jika 0.0)
        lat = unpacked[15]
        lon = unpacked[16]
        if lat == 0.0 or lon == 0.0:
            lat = None
            lon = None
            
        data_list.append({
            "Time (s)": timestamp_s,
            "Accel X": unpacked[8],
            "Accel Y": unpacked[9],
            "Accel Z": unpacked[10],
            "Gyro X": unpacked[11],
            "Gyro Y": unpacked[12],
            "Gyro Z": unpacked[13],
            "Baro Altitude": unpacked[14],
            "GPS Altitude": unpacked[17],
            "Latitude": lat,
            "Longitude": lon,
            "PDOP": unpacked[18],
            "Satellites": unpacked[20]
        })
        offset += struct_len
        
    return pd.DataFrame(data_list)

# --- UI DASHBOARD ---
st.title("🏎️ Kukang EV Telemetry Dashboard")

# Sidebar
st.sidebar.header("Unggah Data Telemetri")
uploaded_file = st.sidebar.file_uploader("Pilih file log.bin", type=['bin'])

st.sidebar.markdown("---")
st.sidebar.markdown("""
**Panduan Fitur:**
- 🖱️ **Zoom In**: Klik dan seret kotak pada grafik.
- 🖐️ **Pan**: Tahan Shift + klik seret, atau pilih mode Pan di pojok kanan atas grafik.
- ↕️ **Batas Sumbu Y**: Arahkan kursor ke angka sumbu Y, klik lalu tarik ke atas/bawah.
- 🌗 **Tema**: Tema Web Gelap/Terang mengikuti pengaturan Windows/Browser Anda.
""")

if uploaded_file is not None:
    # Membaca bytes file
    file_bytes = uploaded_file.read()
    
    # Parsing data
    with st.spinner('Mengekstrak data biner...'):
        df = parse_binary_log(file_bytes)
    
    if df.empty:
        st.error("File log kosong atau rusak.")
    else:
        st.sidebar.success(f"Berhasil memuat {len(df)} baris data!")
        
        # Membagi layar menjadi 2 kolom (Rasio 1 : 2)
        col1, col2 = st.columns([1, 2])
        
        # --- KOLOM 1: PETA FOLIUM ---
        with col1:
            st.subheader("🗺️ GPS Track Map")
            
            # Hapus data yang tidak memiliki sinyal GPS
            valid_gps = df.dropna(subset=['Latitude', 'Longitude'])
            
            if not valid_gps.empty:
                center_lat = valid_gps['Latitude'].mean()
                center_lon = valid_gps['Longitude'].mean()
                
                # Buat Peta Terrain (OpenTopoMap)
                m = folium.Map(location=[center_lat, center_lon], zoom_start=18, tiles='OpenTopoMap')
                
                # Gambar lintasan mobil
                points = list(zip(valid_gps['Latitude'], valid_gps['Longitude']))
                folium.PolyLine(points, color="red", weight=4, opacity=0.8).add_to(m)
                
                # Tandai Start dan End
                folium.Marker(points[0], popup="Start", icon=folium.Icon(color='green')).add_to(m)
                folium.Marker(points[-1], popup="End", icon=folium.Icon(color='red')).add_to(m)
                
                # Render Peta ke Streamlit
                st_folium(m, height=750, use_container_width=True)
            else:
                st.warning("Tidak ada data GPS (No Fix) pada log ini.")
                
        # --- KOLOM 2: GRAFIK PLOTLY ---
        with col2:
            st.subheader("📈 Time-Series Telemetry")
            
            # Buat subplot Plotly yang tersinkronisasi
            fig = make_subplots(
                rows=4, cols=1, 
                shared_xaxes=True, 
                vertical_spacing=0.04,
                subplot_titles=("Acceleration (G)", "Angular Velocity (Gyro)", "Altitude (m)", "GPS Quality")
            )
            
            # 1. Acceleration
            fig.add_trace(go.Scatter(x=df['Time (s)'], y=df['Accel X'], name='Accel X', line=dict(color='#ff3333')), row=1, col=1)
            fig.add_trace(go.Scatter(x=df['Time (s)'], y=df['Accel Y'], name='Accel Y', line=dict(color='#33ff33')), row=1, col=1)
            fig.add_trace(go.Scatter(x=df['Time (s)'], y=df['Accel Z'], name='Accel Z', line=dict(color='#3333ff')), row=1, col=1)
            
            # 2. Gyro
            fig.add_trace(go.Scatter(x=df['Time (s)'], y=df['Gyro X'], name='Gyro X', line=dict(color='#ff9933')), row=2, col=1)
            fig.add_trace(go.Scatter(x=df['Time (s)'], y=df['Gyro Y'], name='Gyro Y', line=dict(color='#cc33ff')), row=2, col=1)
            fig.add_trace(go.Scatter(x=df['Time (s)'], y=df['Gyro Z'], name='Gyro Z', line=dict(color='#33ccff')), row=2, col=1)
            
            # 3. Altitude
            fig.add_trace(go.Scatter(x=df['Time (s)'], y=df['Baro Altitude'], name='Baro Alt', line=dict(color='#ffff33')), row=3, col=1)
            fig.add_trace(go.Scatter(x=df['Time (s)'], y=df['GPS Altitude'], name='GPS Alt', line=dict(color='#00ffcc', dash='dash')), row=3, col=1)
            
            # 4. GPS Quality (Satelit dan PDOP)
            fig.add_trace(go.Scatter(x=df['Time (s)'], y=df['Satellites'], name='Satellites', line=dict(color='#ff66ff')), row=4, col=1)
            fig.add_trace(go.Scatter(x=df['Time (s)'], y=df['PDOP'], name='PDOP (Error)', line=dict(color='white')), row=4, col=1)
            
            # Pengaturan layout grafik
            fig.update_layout(
                height=800,
                hovermode="x unified",     # Hover garis vertikal lintas grafik
                margin=dict(l=0, r=0, t=30, b=0),
                legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
            )
            
            # Set label sumbu Y
            fig.update_yaxes(title_text="G", row=1, col=1)
            fig.update_yaxes(title_text="deg/s", row=2, col=1)
            fig.update_yaxes(title_text="Meters", row=3, col=1)
            fig.update_yaxes(title_text="Count / Error", row=4, col=1)
            
            # Render Grafik ke Streamlit
            st.plotly_chart(fig, use_container_width=True)

else:
    st.info("👈 Silakan unggah file log.bin dari menu samping (Sidebar) untuk memulai analisis.")
