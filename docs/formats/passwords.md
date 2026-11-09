# Level access codes

After each level the game shows a ten-letter access code. Entering it on
the code screen starts that level. A code holds:

* `L` – level index 0..119 (`rating × 30 + number − 1`)
* `P` – percentage rescued on the previous level (7 bits)
* `F` – 4 flag bits carried unchanged from the code that was entered last
  (0 on a fresh start)

Because `P` is part of the code, the same level has many valid codes;
published code lists therefore differ between sources.

Implementation: `src/core/password.cpp`.

## Encoding

Each position has a base letter; data bits are OR-ed into bits that are
zero in that letter (positions 1–7) or added to it (positions 8–10).

| Pos | Base | Content |
|---:|:---:|---|
| 1 | A | bit 3 = L0, bit 2 = P0, bit 1 = F0 |
| 2 | J | bit 2 = L1, bit 0 = P1 |
| 3 | H | bit 2 = L2, bit 1 = P2, bit 0 = F1 |
| 4 | L | bit 1 = P3, bit 0 = L3 |
| 5 | D | bit 3 = P4, bit 1 = L4, bit 0 = F2 |
| 6 | H | bit 2 = P5, bit 1 = F3, bit 0 = L5 |
| 7 | B | bits 3–2 = L7–L6, bit 0 = P6 |
| 8 | B | + (L & 15) |
| 9 | C | + (L >> 4) |
| 10 | J | + (sum of letters 1–9) & 15 |

Finally letters 1–7 are rotated right (each letter moves one position to
the right, the last wraps to the front) `8 − (L & 7)` times.

## Decoding

1. Check letter 10 against the checksum of letters 1–9.
2. Read `L` from letters 8 and 9.
3. Rotate letters 1–7 left `8 − (L & 7)` times.
4. Rebuild `L` from the bits of letters 1–7; it must equal the value from
   step 2. `F` is read back and kept for the next code; `P` is ignored.

Example: `IJJLDNCCCN` → level index 1 (Fun 2), P = 100, F = 8.

The code `BILLANDTED` switches on the original game's cheat mode.

## Verification

Decoding was checked against publicly available code lists: every listed
code either decodes to the level it is listed for, or is a one-letter
transcription error (except one code with several errors).
