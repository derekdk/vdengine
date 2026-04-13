#!/usr/bin/env python3
"""Generate small synthetic test PNGs for FlipCompare_test.cpp."""

import os
import struct
import zlib

def write_png(path, width, height, pixels):
    """Write an RGBA PNG file from raw pixel data (list of (R,G,B,A) tuples)."""
    def chunk(chunk_type, data):
        c = chunk_type + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)

    raw = b''
    for y in range(height):
        raw += b'\x00'  # filter byte: None
        for x in range(width):
            idx = y * width + x
            r, g, b, a = pixels[idx]
            raw += struct.pack('BBBB', r, g, b, a)

    signature = b'\x89PNG\r\n\x1a\n'
    ihdr = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)  # 8-bit RGBA
    idat = zlib.compress(raw)

    with open(path, 'wb') as f:
        f.write(signature)
        f.write(chunk(b'IHDR', ihdr))
        f.write(chunk(b'IDAT', idat))
        f.write(chunk(b'IEND', b''))

def main():
    data_dir = os.path.join(os.path.dirname(__file__), 'data')
    os.makedirs(data_dir, exist_ok=True)

    # solid_red_8x8.png
    pixels = [(255, 0, 0, 255)] * 64
    write_png(os.path.join(data_dir, 'solid_red_8x8.png'), 8, 8, pixels)

    # solid_red_8x8_copy.png (identical)
    write_png(os.path.join(data_dir, 'solid_red_8x8_copy.png'), 8, 8, pixels)

    # solid_blue_8x8.png
    pixels = [(0, 0, 255, 255)] * 64
    write_png(os.path.join(data_dir, 'solid_blue_8x8.png'), 8, 8, pixels)

    # solid_red_8x8_noise.png (red with +-3 noise per channel, deterministic)
    import random
    rng = random.Random(42)
    pixels = []
    for _ in range(64):
        r = max(0, min(255, 255 + rng.randint(-3, 3)))
        g = max(0, min(255, 0 + rng.randint(-3, 3)))
        b = max(0, min(255, 0 + rng.randint(-3, 3)))
        pixels.append((r, g, b, 255))
    write_png(os.path.join(data_dir, 'solid_red_8x8_noise.png'), 8, 8, pixels)

    # gradient_32x32.png (horizontal red gradient)
    pixels = []
    for y in range(32):
        for x in range(32):
            r = int(x * 255 / 31)
            pixels.append((r, 0, 0, 255))
    write_png(os.path.join(data_dir, 'gradient_32x32.png'), 32, 32, pixels)

    # gradient_32x32_shifted.png (same gradient shifted 1px right)
    pixels = []
    for y in range(32):
        for x in range(32):
            src_x = (x - 1) % 32
            r = int(src_x * 255 / 31)
            pixels.append((r, 0, 0, 255))
    write_png(os.path.join(data_dir, 'gradient_32x32_shifted.png'), 32, 32, pixels)

    # checkerboard_16x16.png
    pixels = []
    for y in range(16):
        for x in range(16):
            if (x + y) % 2 == 0:
                pixels.append((255, 255, 255, 255))
            else:
                pixels.append((0, 0, 0, 255))
    write_png(os.path.join(data_dir, 'checkerboard_16x16.png'), 16, 16, pixels)

    # blank_16x16.png
    pixels = [(0, 0, 0, 255)] * 256
    write_png(os.path.join(data_dir, 'blank_16x16.png'), 16, 16, pixels)

    print("Generated test images in", data_dir)

if __name__ == '__main__':
    main()
