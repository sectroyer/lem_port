# `adlib.dat` – AdLib music and sound driver

`adlib.dat` is a [compressed](dat-compression.md) file with one section of
22125 bytes: a complete real-mode x86 sound driver for the AdLib (OPL2) card,
including all music and sound effect data. The game loads it into memory
and calls it as an interrupt handler.

lem_port does the same: it loads the section into an emulated 8086 address
space (`src/core/x86.cpp`), calls the driver exactly as the game does, and
sends the driver's writes to the FM chip to an OPL2 emulator (ymfm).
Implementation: `src/core/adlib.cpp`.

## Calling convention

The driver is entered at offset 0 with a function number in AH; it returns
with IRET.

| AH | AL | Function |
|---:|---|---|
| 0 | – | Tick: advance music and effects by one step |
| 1 | – | Detect the card (uses the chip timers); returns AL = 1 if present |
| 2 | – | Silence all channels |
| 3 | track | Select a music track (0 = none); starts on the next tick |
| 4 | effect | Start a sound effect (1..18) |
| 5 | value | Set an internal play state byte |

FM chip access goes through port 388h (register select) and 389h (data).

## Timing

The game calls the tick from its timer interrupt. With the default machine
type the PC timer runs with divisor 0x4000, i.e. 1193182 / 16384 ≈ 72.8
ticks per second. One game frame lasts 5 of these interrupts.

## Start-up

The game initialises the driver with: track 1, tick, track 0, silence, tick.
A level starts its music with "select track" followed by one tick.

## Music tracks

| # | Name | # | Name |
|---:|---|---:|---|
| 1 | Awesome | 12 | Tim1 |
| 2 | BeastI | 13 | Tim2 |
| 3 | BeastII | 14 | Tim3 |
| 4 | CanCan | 15 | Tim4 |
| 5 | Doggie | 16 | Tim5 |
| 6 | Lemming1 | 17 | Tim6 |
| 7 | Lemming2 | 18 | Tim7 |
| 8 | Lemming3 | 19 | Tim8 |
| 9 | Menace | 20 | Tim9 |
| 10 | Mountain | 21 | Tim10 |
| 11 | Ten Lemmings | | |

Special levels use tracks 1, 9, 3, 2 (for `vgaspec0`..`vgaspec3`). Other
levels take the next entry of this list each time a new level is started
(retrying a level keeps the tune):

`6 7 8 10 11 4 12 13 14 15 5 16 17 18 19 20 21`

## Sound effects

Numbers are the same as the trap sound field of the graphics sets (see
[graphics-set.md](graphics-set.md#trap-sounds)): 1 skill select, 2 hatch,
3 "let's go", 4 assign, 5 "oh no", 6 electric trap, 7 crushing trap,
8 splat, 9 rope/slicer, 10 steel, 12 explosion, 13 fire, 14 ten-ton,
15 bear trap, 16 exit, 17 drowning, 18 last bricks / refused skill.
