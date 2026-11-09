# Special sets: `vgaspecN.dat`

Four special levels use a single full-terrain bitmap instead of terrain
pieces. Each `vgaspecN.dat` has one compressed section (see
[dat-compression.md](dat-compression.md)) which contains:

| Offset | Size | Content |
|---:|---:|---|
| 0 | 24 | VGA palette, colours 8..15 |
| 24 | 16 | EGA palette data |
| 40 | … | run-length encoded bitmap |

## Run-length layer

Read forward, byte by byte:

| Byte `c` | Meaning |
|---|---|
| 0x00–0x7F | copy the next c+1 bytes |
| 0x81–0xFF | repeat the next byte 257−c times |
| 0x80 | end of the current part |

There are 4 parts of 14400 bytes each. Every part is a 960 × 40,
3-plane planar bitmap (rows 0–39, 40–79, 80–119, 120–159).

Pixel value 0 is empty; values 1..7 map to palette colours 9..15.

The level record still names a normal graphics set; it supplies the objects
(entrance, exit, …). The terrain pieces list is empty. The bitmap is placed
at x = 304 in the 1600-pixel-wide level.

Implementation: `loadSpecialSet()` in `src/core/graphicset.cpp`.
