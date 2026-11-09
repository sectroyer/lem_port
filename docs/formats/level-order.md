# Level order and `oddtable.dat`

The game has 120 levels in four ratings of 30 (Fun, Tricky, Taxing,
Mayhem), but only 80 level records (`level000.dat`–`level009.dat`, 8 each).
The other 40 levels reuse the terrain of an existing record with different
settings and a different name, taken from `oddtable.dat`.

Implementation: `src/core/levelpack.cpp`.

## Record numbering

Record *r* (0..79) is section `r % 8` of `level{r / 8:03}.dat`.

## `oddtable.dat`

Not compressed. 80 entries × 56 bytes; entry *i* belongs to record *i*.

| Offset | Size | Content |
|---:|---:|---|
| 0 | 24 | 12 big-endian words, same layout as level header 0x00–0x17: release rate, lemmings, rescue, minutes, 8 skill counts |
| 24 | 32 | level name, space padded |

Unused entries have every word set to 99 and the name
"This is a non-used duplicate".

When a variation is selected, its 24 header bytes and 32 name bytes replace
those of the level record; everything else (graphics set, objects, terrain,
steel, start position) stays as in the record.

## Order table

The game's order is a list of 120 one-byte codes (30 per rating):

* odd code *v*: play record `(v − 1) / 2` unchanged
* even code *v*: play record `i = v / 2 − 1` with `oddtable.dat` entry *i*

```
Fun:    147 155 157 149 151 153 159  14  22  54  70  16  29  32  38
         42  48  72  84 104 138  23  68  96  98 116  78 100 108 134
Tricky:   1  30  36  50  52  56  58  80 102 120 128 130 136   5 148
        152 154 156 160   7  11  13  15  17  19  21  25  27  33  31
Taxing:  37  39  41  43  45  47  49  51  53  55  57  59  61  63   3
         65  67  69  71  73  75  77  79  81  83  85  87  89  35 111
Mayhem:  91  93  95  97  99 101 103 105 107 109 112 113 115 117 119
        121 123 125 127 150 129   9 131 133 135 137 139 141 143 145
```

Example: Fun 1 = code 147 = record 73 ("Just dig!"); Fun 8 = code 14 =
record 6 with variation 6 ("Not as complicated as it looks").
