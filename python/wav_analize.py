# python3 - "/mnt/c/Users/Bartek/Downloads/SHOT.WAV" <<'PY'
import sys
import struct
from pathlib import Path

p = Path(sys.argv[1])
with p.open("rb") as f:
    header = f.read(12)
    print("Plik:", p.name)
    print("Rozmiar:", p.stat().st_size, "bajtów")
    print("Nagłówek:", repr(header))

    if len(header) != 12 or header[:4] != b"RIFF" or header[8:] != b"WAVE":
        raise SystemExit("To nie jest standardowy plik RIFF/WAVE.")

    while True:
        pos = f.tell()
        chunk = f.read(8)
        if len(chunk) != 8:
            break
        name, size = struct.unpack("<4sI", chunk)
        print(f"Chunk {name!r}: nagłówek={pos}, dane={pos + 8}, rozmiar={size}")

        if name == b"fmt " and size >= 16:
            raw = f.read(16)
            if len(raw) != 16:
                raise SystemExit("Niepełny chunk fmt.")
            fmt, ch, rate, byte_rate, align, bits = struct.unpack("<HHIIHH", raw)
            print(f"  format={fmt}, kanały={ch}, Hz={rate}, bity={bits}, "
                  f"block_align={align}, byte_rate={byte_rate}")

        f.seek(pos + 8 + size + (size & 1))

