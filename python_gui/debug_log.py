import struct

def analyze(filename):
    with open(filename, 'rb') as f:
        d = f.read()

    print(f"\n=== {filename} (len: {len(d)}) ===")
    prev_offset = None
    prev_ts = None
    jumps = []
    
    for i in range(len(d) - 10):
        # Match either 1980 (0x07bc) or 2026 (0x07ea)
        if (d[i+4:i+6] == b'\xbc\x07' or d[i+4:i+6] == b'\xea\x07'):
            ts = struct.unpack('<I', d[i:i+4])[0]
            delta_off = (i - prev_offset) if prev_offset is not None else 0
            delta_ts = (ts - prev_ts) if prev_ts is not None else 0
            if delta_off != 90 and prev_offset is not None:
                print(f"JUMP #{len(jumps)}: offset {prev_offset} (ts={prev_ts}) -> offset {i} (ts={ts}). Delta offset={delta_off}, Delta ts={delta_ts}. i % 512 = {i % 512}")
                jumps.append((prev_offset, i, delta_off, delta_ts))
            prev_offset = i
            prev_ts = ts
    print(f"Total jumps: {len(jumps)}, last_ts: {prev_ts}, last_offset: {prev_offset}")

analyze('python_gui/board_telemetry/log80.bin')
analyze('python_gui/board_telemetry/log81.bin')
