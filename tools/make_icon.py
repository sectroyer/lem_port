#!/usr/bin/env python3
"""Generates the application icon (original pixel art) as a 1024x1024 PNG.

usage: make_icon.py out.png [--square]
--square: opaque square without rounded corners (iOS rounds icons itself)
"""
import struct
import sys
import zlib

# 16x16 pixel art: a little walker with green hair and a blue robe.
ART = [
    "................",
    "......GGG.......",
    ".....GGGGG......",
    "....GGGGGGG.....",
    "....GGSSSGG.....",
    ".....SSKSS......",
    ".....SSSSS......",
    "......BBB.......",
    ".....BBBBB......",
    "....SBBBBBS.....",
    "....SBBBBB......",
    ".....BBBBB......",
    ".....BB.BB......",
    ".....BB..BB.....",
    "....SS....SS....",
    "................",
]
COLORS = {
    "G": (40, 200, 40),
    "S": (245, 215, 200),
    "K": (20, 20, 40),
    "B": (70, 80, 230),
}
BG_TOP, BG_BOTTOM = (60, 30, 20), (120, 70, 40)
SIZE, CELL, MARGIN = 1024, 56, 64


SQUARE = "--square" in sys.argv


def pixel(x, y):
    # Rounded-square background with a vertical gradient.
    r = 0 if SQUARE else 180
    cx = min(max(x, r), SIZE - 1 - r)
    cy = min(max(y, r), SIZE - 1 - r)
    if r and (x - cx) ** 2 + (y - cy) ** 2 > r * r:
        return (0, 0, 0, 0)
    t = y / (SIZE - 1)
    bg = tuple(int(a + (b - a) * t) for a, b in zip(BG_TOP, BG_BOTTOM))
    ox = (SIZE - 16 * CELL) // 2
    oy = (SIZE - 16 * CELL) // 2 + 20
    gx, gy = (x - ox) // CELL, (y - oy) // CELL
    if 0 <= gx < 16 and 0 <= gy < 16:
        c = ART[gy][gx]
        if c in COLORS:
            return COLORS[c] + (255,)
    # Grass strip under the feet.
    if oy + 15 * CELL <= y < oy + 16 * CELL - 16:
        return (30, 140, 30, 255)
    return bg + (255,)


def main():
    raw = bytearray()
    for y in range(SIZE):
        raw.append(0)
        for x in range(SIZE):
            p = pixel(x, y)
            raw.extend(p[:3] if SQUARE else p)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2 if SQUARE else 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(sys.argv[1], "wb") as f:
        f.write(png)


if __name__ == "__main__":
    main()
