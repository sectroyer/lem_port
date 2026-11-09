# `main.dat`

Shared graphics for EGA/VGA mode. A [compressed](dat-compression.md) file
with 7 sections. All bitmaps are [planar](planar-bitmaps.md); colour 0 is
transparent unless stated otherwise. Frames of an animation are stored back
to back (`w*h/8*bpp` bytes each).

Implementation: `src/core/maindat.cpp`.

| Section | Size | Content |
|---:|---:|---|
| 0 | 21104 | lemming animations |
| 1 | 388 | terrain removal masks, bomber countdown digits |
| 2 | 8384 | alternative skill panel ("high performance" mode), skill digits, alternative panel font |
| 3 | 61968 | main menu: background, logo, signs, sound icons |
| 4 | 36080 | main menu: eye blinks, credit scroller, rating signs, menu font |
| 5 | 758 | not graphics; unused by this engine |
| 6 | 8224 | skill panel and panel font (used in normal mode) |

## Section 0 – lemming animations (in-level palette)

| Offset | Animation | Frames | Size | bpp |
|---:|---|---:|---|---:|
| 0x0000 | walk right | 8 | 16×10 | 2 |
| 0x0140 | jump (step up) right | 1 | 16×10 | 2 |
| 0x0168 | walk left | 8 | 16×10 | 2 |
| 0x02A8 | jump left | 1 | 16×10 | 2 |
| 0x02D0 | dig | 16 | 16×14 | 3 |
| 0x0810 | climb right | 8 | 16×12 | 2 |
| 0x0990 | climb left | 8 | 16×12 | 2 |
| 0x0B10 | drown | 16 | 16×10 | 2 |
| 0x0D90 | climb-over right | 8 | 16×12 | 2 |
| 0x0F10 | climb-over left | 8 | 16×12 | 2 |
| 0x1090 | build right | 16 | 16×13 | 3 |
| 0x1570 | build left | 16 | 16×13 | 3 |
| 0x1A50 | bash right | 32 | 16×10 | 3 |
| 0x21D0 | bash left | 32 | 16×10 | 3 |
| 0x2950 | mine right | 24 | 16×13 | 3 |
| 0x30A0 | mine left | 24 | 16×13 | 3 |
| 0x37F0 | fall right | 4 | 16×10 | 2 |
| 0x3890 | fall left | 4 | 16×10 | 2 |
| 0x3930 | umbrella opening right | 4 | 16×16 | 3 |
| 0x3AB0 | floating right | 4 | 16×16 | 3 |
| 0x3C30 | umbrella opening left | 4 | 16×16 | 3 |
| 0x3DB0 | floating left | 4 | 16×16 | 3 |
| 0x3F30 | splat | 16 | 16×10 | 2 |
| 0x41B0 | exiting | 8 | 16×13 | 2 |
| 0x4350 | burning | 14 | 16×14 | 4 |
| 0x4970 | blocking | 16 | 16×10 | 2 |
| 0x4BF0 | shrug right | 8 | 16×10 | 2 |
| 0x4D30 | shrug left | 8 | 16×10 | 2 |
| 0x4E70 | "oh no" | 16 | 16×10 | 2 |
| 0x50F0 | explosion | 1 | 32×32 | 3 |

Dig/bash/mine frames include the debris. The bash animation contains two
strokes in its 32 frames.

## Section 1 – masks and digits (1 bpp)

| Offset | Content | Frames | Size |
|---:|---|---:|---|
| 0x0000 | bash mask right | 4 | 16×10 |
| 0x0050 | bash mask left | 4 | 16×10 |
| 0x00A0 | mine mask right | 2 | 16×13 |
| 0x00D4 | mine mask left | 2 | 16×13 |
| 0x0108 | explosion mask | 1 | 16×22 |
| 0x0134 | countdown digits 9,8,…,0 | 10 | 8×8 |

Mask bit 1 = remove terrain. Countdown digits draw bit 1 in white.

## Section 2 – skill digits

At 0x1900: twenty 8×8 1-bpp bitmaps, for each digit 0..9 first the
right-half version then the left-half version (digits are 4 pixels wide).
Two-digit skill counts are drawn by combining a left and a right digit.

0x0000 holds a 320×40 4-bpp alternative panel and 0x19A0 an alternative
8×16 font; neither is used.

## Section 3 – main menu (menu palette)

| Offset | Content | Size | bpp |
|---:|---|---|---:|
| 0x0000 | background tile (colour 0 is opaque) | 320×104 | 2 |
| 0x2080 | logo | 632×94 | 4 |
| 0x9488 | sign F1 "1 player" | 120×61 | 4 |
| 0xA2D4 | sign F2 "new level" | 120×61 | 4 |
| 0xB120 | sign F3 (sound, blank) | 120×61 | 4 |
| 0xBF6C | rating sign | 120×61 | 4 |
| 0xCDB8 | sign "exit to DOS" | 120×61 | 4 |
| 0xDC04 | sign F4 (controls) | 120×61 | 4 |
| 0xEA50 | music icon (overlay on F3) | 64×31 | 4 |
| 0xEE30 | FX icon (overlay on F3) | 64×31 | 4 |

## Section 4 – main menu, continued (menu palette, 4 bpp unless noted)

| Offset | Content | Frames | Size |
|---:|---|---:|---|
| 0x0000 + n·0x600 | eye blink n (n = 0..6) | 8 | 32×12 |
| 0x2A00 | scroller, left end | 16 | 48×16 |
| 0x4200 | scroller, right end | 16 | 48×16 |
| 0x5A00 | scroller reel tile | 1 | 16×16 |
| 0x5A80 | rating "mayhem" | 1 | 72×27 |
| 0x5E4C | rating "taxing" | 1 | 72×27 |
| 0x6218 | rating "tricky" | 1 | 72×27 |
| 0x65E4 | rating "fun" | 1 | 72×27 |
| 0x69B0 | menu font, ASCII 0x21..0x7E, 3 bpp | 94 | 16×16 |

Character *c* is at `0x69B0 + (c − 0x21) · 0x60`.

## Section 6 – skill panel (in-level palette)

| Offset | Content | Size | bpp |
|---:|---|---|---:|
| 0x0000 | skill panel | 320×40 | 4 |
| 0x1900 | panel font: `% 0-9 - A-Z` (38 glyphs) | 8×16 | 3 |

Glyph *i* is at `0x1900 + i · 0x30`.

## Menu palette

| Idx | RGB | Idx | RGB |
|---:|---|---:|---|
| 0 | 0,0,0 | 8 | 0,80,0 |
| 1 | 128,64,32 | 9 | 0,96,16 |
| 2 | 96,48,32 | 10 | 0,112,32 |
| 3 | 48,0,16 | 11 | 0,128,64 |
| 4 | 32,8,124 | 12 | 208,208,208 |
| 5 | 64,44,144 | 13 | 176,176,0 |
| 6 | 104,88,164 | 14 | 64,80,176 |
| 7 | 152,140,188 | 15 | 224,128,144 |
