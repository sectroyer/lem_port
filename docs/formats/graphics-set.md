# Graphics sets: `groundNo.dat` + `vgagrN.dat`

Five sets (0..4) ship with the game. `groundNo.dat` (1056 bytes, not
compressed) describes the pieces; `vgagrN.dat` (2 compressed sections) holds
the bitmaps: section 0 = terrain, section 1 = objects.

Words in `groundNo.dat` are **little-endian**.

Implementation: `src/core/graphicset.cpp`.

## Layout of `groundNo.dat`

| Offset | Size | Content |
|---:|---:|---|
| 0x000 | 16 × 28 | object descriptors |
| 0x1C0 | 64 × 8 | terrain descriptors |
| 0x3C0 | 8 | EGA palette (in level) |
| 0x3C8 | 8 | EGA palette (standard, unused) |
| 0x3D0 | 8 | EGA palette (preview) |
| 0x3D8 | 24 | VGA palette colours 8..15 (in level) |
| 0x3F0 | 24 | VGA palette (standard, unused) |
| 0x408 | 24 | VGA palette colours 8..15 (preview) |

## Object descriptor (28 bytes)

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | flags: bit 0 = animates only when triggered; bit 1 = continuous loop |
| 2 | 1 | start frame |
| 3 | 1 | frame count |
| 4 | 1 | width |
| 5 | 1 | height |
| 6 | 2 | bytes per frame (stride) |
| 8 | 2 | mask offset from frame start |
| 10 | 2 | (copy of mask offset) |
| 12 | 2 | (half of mask offset) |
| 14 | 2 | trigger left / 4 |
| 16 | 2 | (trigger top + 4) / 4 |
| 18 | 1 | trigger width / 4 |
| 19 | 1 | trigger height / 4 |
| 20 | 1 | trigger effect |
| 21 | 2 | offset of frame 0 in section 1 |
| 23 | 2 | preview image offset (does not index the object frames in this data set; frame 0 is used) |
| 25 | 2 | unknown |
| 27 | 1 | sound played when a trap fires |

Frames are 4-bit planar bitmaps followed (at `mask offset`) by a 1-bit mask.
Frame *k* starts at `base + k * stride`.

Object 0 is always the exit, object 1 the entrance hatch (flags = 3, frames
1..10: frame 0 is the closed hatch).

### Trigger effects

| Value | Effect |
|---:|---|
| 0 | none (decoration) |
| 1 | exit |
| 4 | trap – kills one lemming, plays its animation, ignores others while running |
| 5 | water – drowning |
| 6 | fire / disintegration – kills on contact, any number at once |
| 7 | one-way wall pointing left |
| 8 | one-way wall pointing right |
| 9 | steel (unused by objects) |

Trigger coordinates are relative to the object position, with the object
position rounded down to a multiple of 4 (triggers live on a 4-pixel grid).

### Trap sounds

0 none, 1 skill select, 2 hatch opening, 3 "let's go", 4 assign skill,
5 "oh no", 6 electric trap, 7 crushing trap, 8 splat, 9 rope/slicer,
0x0A steel hit, 0x0B unknown, 0x0C explosion, 0x0D fire, 0x0E 10-ton trap,
0x0F bear trap, 0x10 exit, 0x11 drowning, 0x12 last bricks.

## Terrain descriptor (8 bytes)

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | width |
| 1 | 1 | height |
| 2 | 2 | image offset in section 0 |
| 4 | 2 | mask offset in section 0 |
| 6 | 2 | unknown |

Terrain bitmaps are 4-bit planar and use colours 8..15 only; the mask plane
coincides with plane 3. Unused slots are all zero.

## Verified object tables

| Set | Theme | Objects (id: description) |
|---:|---|---|
| 0 | dirt | 0 exit, 1 entrance, 2 green flag, 3/4 one-way, 5 water, 6 bear trap, 7 exit flames, 8 rock crusher, 9 blue flag, 10 ten-ton |
| 1 | fire | 0 exit, 1 entrance, 2 green flag, 3/4 one-way, 5 lava, 6 exit flames, 7 fire pit, 8 flamethrower (L), 9 blue flag, 10 flamethrower (R) |
| 2 | marble | 0 exit, 1 entrance, 2 green flag, 3/4 one-way, 5 acid, 6 exit flames, 7 blue flag, 8 pillar crusher, 9 spinner |
| 3 | pillar | 0 exit, 1 entrance, 2 green flag, 3/4 one-way, 5 water, 6 exit flames, 7 blue flag, 8 rope trap, 9/10 spikes |
| 4 | crystal | 0 exit, 1 entrance, 2 green flag, 3 blue flag, 4/5 one-way, 6 water, 7 slicer, 8 exit flames, 9 electrode, 10 zapper |
