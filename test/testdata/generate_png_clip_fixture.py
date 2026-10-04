"""Regenerate the PNG clipping fixture with dependent rows and a long IDAT."""
from pathlib import Path
import struct
import zlib


def chunk(kind, data):
    return (struct.pack('>I', len(data)) + kind + data +
            struct.pack('>I', zlib.crc32(kind + data)))


width, height = 32, 128
filtered = bytearray()
previous = bytes(width * 4)
for y in range(height):
    row = bytes(value for x in range(width) for value in
                ((x * 31 + y * 17) & 255, (x * 13 + y * 29) & 255,
                 (x * 7 + y * 11) & 255, (0, 128, 255)[(x + y) % 3]))
    filtered.append(0 if y == 0 else 2)
    filtered.extend(row if y == 0 else
                    bytes((a - b) & 255 for a, b in zip(row, previous)))
    previous = row
# Stored deflate blocks keep the IDAT larger than the decoder's read buffer.
data = (b'\x89PNG\r\n\x1a\n' +
        chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) +
        chunk(b'IDAT', zlib.compress(bytes(filtered), level=0)) + chunk(b'IEND', b''))
Path(__file__).with_name('rgba_filtered_32x128.png').write_bytes(data)
