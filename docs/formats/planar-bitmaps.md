# Planar bitmaps

All game graphics use planar storage, matching EGA/VGA 16-colour hardware.

A `w × h` bitmap with `bpp` bit planes takes `w*h/8 * bpp` bytes:

```
plane 0 : w*h/8 bytes   -> adds 1 to the pixel value
plane 1 : w*h/8 bytes   -> adds 2
plane 2 : ...           -> adds 4
plane 3 : ...           -> adds 8
```

Inside a plane pixels run left to right, top to bottom; the most significant
bit of each byte is the leftmost pixel.

A mask is a single plane (1 = opaque).

Implementation: `decodePlanar()` / `decodePlanarMasked()` in
`src/core/bitmap.cpp`.

## Palettes

VGA palette entries are 3 bytes (R, G, B), 6 bits each (0..63). Convert to
8 bits with `v * 255 / 63`.

EGA palette entries are 1 byte: `00 RH GH BH RL GL BL`.

### In-level palette

| Index | VGA (R,G,B) | Use |
|---:|---|---|
| 0 | 00,00,00 | black / empty |
| 1 | 10,10,38 | lemming body |
| 2 | 00,2C,00 | lemming hair |
| 3 | 3C,34,34 | lemming skin |
| 4 | 2C,2C,00 | panel |
| 5 | 3C,08,08 | panel / nuke |
| 6 | 20,20,20 | panel |
| 7 | = colour 8 | builder bricks, minimap |
| 8–15 | from graphics set / special set | terrain and objects |
