# Level record

Every section of `levelNNN.dat` decompresses to a 2048-byte level record.
Ten files × 8 sections = 80 records. All words are **big-endian**.

Implementation: `src/core/level.cpp`.

## Header (0x000–0x01F)

The game reads only the low byte of each header word.

| Offset | Field |
|-------:|-------|
| 0x00 | release rate (1..99) |
| 0x02 | number of lemmings |
| 0x04 | number to rescue |
| 0x06 | time limit in minutes |
| 0x08 | skill counts, one word each: climber, floater, bomber, blocker, builder, basher, miner, digger |
| 0x18 | initial screen x position (pixels) |
| 0x1A | graphics set (0..4 in this data set) |
| 0x1C | special set: 0 = none, N = `vgaspec(N-1).dat` |
| 0x1E | unused |

## Objects (0x020–0x11F) – 32 × 8 bytes

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | x + 16 (signed) |
| 2 | 2 | y (signed) |
| 4 | 2 | object id (index into graphics set object table) |
| 6 | 1 | flags: 0x80 = draw behind terrain, 0x40 = only visible over terrain |
| 7 | 1 | 0x8F = upside down, 0x0F = normal |

An all-zero slot is empty. A slot whose x field is 0 is ignored by the game
(the entrance scan still sees it). The x position is not rounded.

## Terrain (0x120–0x75F) – 400 × 4 bytes

```
byte0: FFFF XXXX    F = flags, X = x bits 11..8
byte1: XXXXXXXX     x bits 7..0
byte2: YYYYYYYY     y bits 8..1
byte3: YTTTTTTT     y bit 0, T = terrain id (low 6 bits used)
```

* x = value − 16
* y = 9-bit two's complement value − 4
* flags: 8 = do not overwrite existing terrain, 4 = upside down,
  2 = erase (remove terrain where the piece is solid)
* `FF FF FF FF` = empty slot

## Steel areas (0x760–0x7DF) – 32 × 4 bytes

```
byte0: XXXXXXXX   x bits 8..1
byte1: XYYYYYYY   x bit 0, y (7 bits)
byte2: WWWWHHHH
byte3: 0
```

* x = value × 4 − 16, y = value × 4
* width = (W + 1) × 4, height = (H + 1) × 4
* all-zero slot = empty

## Name (0x7E0–0x7FF)

32 ASCII characters, padded with spaces.

## Level geometry

The playfield is 1600 × 160 pixels. Special levels place their 960 × 160
bitmap at x = 304 (verified visually: terrain pieces are absent and the
rendered image matches the original layout).
