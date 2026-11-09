# Game mechanics

How the simulation in `src/core/game.cpp` behaves. All values are in pixels
and game frames ("ticks").

## Timing

* One tick (game frame) = 5 interrupts of the PC timer at divisor 0x4000,
  i.e. 68.66 ms (about 14.6 ticks per second).
* The level clock shows minutes:seconds; one displayed second = 17 ticks
  (about 1.17 real seconds).
* Tick 16: "Let's go!" sound. Tick 36: hatch sound, hatches open.
* Release interval: `(99 − RR) / 2 + 4` ticks (integer division). The
  countdown starts at 20 and is first decremented on the opening tick, so the
  first lemming appears on tick 55.
* The release rate can be raised up to 99 and lowered down to the level's
  initial value. While a button is held it moves 5 steps, then pauses for two
  ticks (the panel is redrawn in between). The keyboard keys also work while
  the game is paused; the panel buttons do not.
* When the clock reaches 0:00 two more ticks are simulated (lemmings may
  still finish exiting); the clock and the release are frozen during them.

## Entrances

A new lemming appears at hatch position + (24, 14), falling, facing right.
Hatches are used in a rotation of four slots:

| Hatches | Order |
|---:|---|
| 1 | A A A A |
| 2 | A B B A |
| 3 | A B C B |
| 4 | A B C D |

## Terrain and the object map

* Terrain is a 1600×160 pixel map; any pixel is solid.
* A second, coarse map of 4×4-pixel cells stores steel areas, object trigger
  areas and blocker force fields. It covers x −16…1647 and y −16…175.
* Steel areas are written first, then object triggers in object order.
  Objects stored in record slots 16–31 are drawn but have no trigger.
  A slot whose x field is 0 is ignored completely (not drawn, animated or
  triggered).
* Continuously animated objects start one frame ahead of their first frame.
* No object checks happen for a lemming at y ≥ 160.
* Trigger areas are placed relative to the object position rounded down to a
  multiple of 4.

## Lemming state

Each lemming has a foot position (x, y), a direction (±1), an action,
an animation frame and a few counters. The sprite is drawn with its foot at
(8, height) of the 16-pixel-wide frame.

Every tick, for each lemming in creation order:

1. Advance the bomb countdown if set (79 → 0). When it reaches 0 the lemming
   explodes at once if it is falling, floating, drowning or burning;
   otherwise it starts the "oh no" animation. Nothing else happens this tick.
2. Advance the animation frame (except for floaters and diggers, which
   manage their own frames). One-shot animations stop on their last frame
   and raise an end-of-animation flag.
3. Run the action handler (below).
4. If the handler asks for it, check the object map cell at the feet:
   exit, water, fire, trap, blocker force field.

## Actions

| Action | Behaviour |
|---|---|
| Walker | Moves 1 px. Steps up to 2 px; 3–6 px becomes a jump; 7+ px is a wall (turn, or climb if climber). Steps down up to 3 px, deeper becomes a fall. |
| Jumper | Rises 2 px per tick until clear, then walks. |
| Faller | Falls 3 px per tick. Floaters open the umbrella after 16 px. Landing after more than 60 px is fatal. |
| Floater | Follows a fixed 16-step descent table (umbrella opening, then about 2 px per tick). |
| Climber | Climbs 1 px per tick on frames 4–7; at the top, hoists over; an overhang makes it fall backwards. |
| Blocker | Writes a 3×3 force field around itself, turning other lemmings. Becomes a walker if the ground disappears. |
| Builder | 12 bricks of 6 px, one per 16-frame cycle, moving 2 px across and 1 px up per brick. Stops at walls or the top of the level; shrugs after the last brick. |
| Basher | Removes terrain with a 4-frame mask per stroke and moves forward between strokes. Stops at steel, at a one-way wall pointing the other way, or if nothing is left in front. |
| Miner | Removes terrain diagonally with a 2-frame mask, moving 2 px forward and 1–2 px down per cycle. Stops at steel and at one-way walls (a wall pointing right stops miners in both directions, as in the original). |
| Digger | Removes a 9 px wide row and moves down 1 px twice per cycle. Stops at steel or when nothing is left to dig. |
| Bomber | After the countdown and "oh no", removes a 16×22 area unless standing on steel or water. The mask clears every pixel, steel graphics included; the steel cells of the coarse map stay. The lemming then stays in the game for 52 ticks while its debris flies: it counts as "out", the cursor can see it and the level does not end before it is gone. |

Lemmings leaving the level at the bottom (y > 163) are removed.

## Kept action bits

The original stores the action as a bit field. Most changes replace it, but
falling after digging, bashing, mining or climbing, and starting "oh no" or
exploding, add the new bit and keep the old one until the next replacing
change. The kept bit still counts for the cursor priority (digger, basher,
miner, builder, blocker and "oh no" are busy actions) and a kept blocker bit
refuses climber and floater.

## Skill assignment

The cursor's game point is 3 pixels left of and 2 pixels below the centre of
the crosshair. The cursor box of a lemming is 13×13 pixels starting at its
sprite's top-left corner (clamped to the level). When several lemmings are under the cursor, one doing a "busy" action
(blocker, builder, shrugger, basher, miner, digger, "oh no") is preferred,
unless the right mouse button is held; with the button held and only busy
lemmings under the cursor, the last lemming chosen before is used again.
Builders, bashers, miners and diggers can also be given to a second lemming
under the cursor if the first cannot take the skill.

The nuke needs two presses within 5 ticks and is not accepted while paused.

## Pause

While paused, the opening sequence (sounds, hatches opening) keeps running,
but the clock, the release countdown and all lemmings stop.

## End of level

The level ends when the time runs out, when all lemmings are saved or gone,
or when the nuke has finished. After a nuke only the lemmings that had been
released count towards the result (an original behaviour). The result is
`saved × 100 / total` compared with `required × 100 / lemmings`.

## Explosion debris

Debris is visual only. The engine uses its own generated particle pattern
(80 particles, 52 frames).
