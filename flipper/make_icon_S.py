#!/usr/bin/env python3
import struct, zlib

# 10x10 1-bit bold "S"
# Verified pipeline: Flipper SDK does ImageOps.invert before XBM-encoding.
# So a BLACK PNG pixel ends up as a drawn pixel on the Flipper.
# Encode S shape (p=1) as PNG bit 0 (black).
pixels = [
    [0,1,1,1,1,1,1,1,1,0],
    [1,1,1,1,1,1,1,1,1,1],
    [1,1,0,0,0,0,0,0,0,0],
    [1,1,0,0,0,0,0,0,0,0],
    [0,1,1,1,1,1,1,1,1,0],
    [1,1,1,1,1,1,1,1,1,1],
    [0,0,0,0,0,0,0,0,1,1],
    [0,0,0,0,0,0,0,0,1,1],
    [1,1,1,1,1,1,1,1,1,1],
    [0,1,1,1,1,1,1,1,1,0],
]

def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)

W, H = 10, 10
ihdr = struct.pack(">IIBBBBB", W, H, 1, 0, 0, 0, 0)

raw = b""
for row in pixels:
    byte0, byte1 = 0, 0
    for i, p in enumerate(row):
        bit = (1 - p) & 1
        if i < 8:
            byte0 |= bit << (7 - i)
        else:
            byte1 |= bit << (7 - (i - 8))
    raw += b"\x00" + bytes([byte0, byte1])

idat = zlib.compress(raw, 9)
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")

with open("/Users/xyconix/repos/SwizFlockHunter/icon.png", "wb") as f:
    f.write(png)
print("wrote icon.png:", len(png), "bytes")
