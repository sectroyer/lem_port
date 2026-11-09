# Compressed `.DAT` container

Used by `levelNNN.dat`, `vgagrN.dat`, `vgaspecN.dat`, `main.dat`, `cgamain.dat`,
`cgagrN.dat`, `cgaspecN.dat`, `tgamain.dat`, `adlib.dat`, `tandysnd.dat`.

Not used by `groundNo.dat`, `oddtable.dat`, `russell.dat` (plain files).

Implementation: `src/core/dat.cpp`.

## Layout

A file is a sequence of sections laid back to back. Each section is a 10-byte
header followed by the compressed body. All header words are **big-endian**.

| Offset | Size | Field |
|-------:|-----:|-------|
| 0 | 1 | `bits_in_first` – number of valid bits in the last body byte (1..8) |
| 1 | 1 | checksum – XOR of all body bytes |
| 2 | 2 | unused (0) |
| 4 | 2 | decompressed size |
| 6 | 2 | unused (0) |
| 8 | 2 | section size = 10 + body length |

## Bit stream

The body is read **backwards**: start at the last byte, take its bits from the
least significant upwards. Only `bits_in_first` bits are taken from that last
byte; every earlier byte contributes all 8 bits. Multi-bit fields are read
most significant bit first (each new bit is shifted in from the right).

## Output

Output is also produced backwards, from the last byte towards the first,
until `decompressed size` bytes are written. Tokens:

| Prefix | Fields | Meaning |
|--------|--------|---------|
| `00`  | `n:3`, then n+1 × `byte:8` | literal bytes |
| `01`  | `off:8` | copy 2 bytes from distance off+1 |
| `100` | `off:9` | copy 3 bytes from distance off+1 |
| `101` | `off:10` | copy 4 bytes from distance off+1 |
| `110` | `len:8`, `off:12` | copy len+1 bytes from distance off+1 |
| `111` | `n:8`, then n+9 × `byte:8` | literal bytes |

Each literal is written at the current (moving backwards) position, so the
first literal read lands at the highest address.

A copy writes byte by byte: `out[p] = out[p + distance]` while `p` decreases.
Distances smaller than the length are legal and produce runs.

## Verified section counts (this data set)

| File | Sections | Sizes |
|------|---------:|-------|
| `levelNNN.dat` | 8 | 2048 each (one level per section) |
| `vgagrN.dat` | 2 | terrain bitmaps, object bitmaps |
| `vgaspecN.dat` | 1 | palette + second-stage RLE image |
| `main.dat` | 7 | 21104, 388, 8384, 61968, 36080, 758, 8224 |
| `adlib.dat` | 1 | 22125 (sound driver + music) |

## Writing

`compressSectionStored()` writes a valid section using only literal tokens
(no compression). Good enough for tools; the reader accepts any valid stream.
