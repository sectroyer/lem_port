# Replay files (`.lrp`)

A replay records every player input of one attempt so that the attempt can
be played back exactly; the simulation is fully deterministic. lem_port
writes the last attempt to `last.lrp` in its settings folder, `lemtool sim`
writes `replay.lrp`, and `lemtool replay` plays one back.

Plain text, one item per line. Lines starting with `#` are comments.

```
lem_port replay 1
level <rating 0-3> <level 0-29>
<frame> C <x> <y> <right button 0|1>   cursor moved (level coordinates)
<frame> A <skill 0-7>                  assign skill at the cursor
<frame> R <-1|0|1>                     release rate button held / released
<frame> N                              nuke
<frame> P                              this frame was played paused
end <frames>
```

`frame` is the number of game frames played before the input. Skills are
numbered climber, floater, bomber, blocker, builder, basher, miner, digger.

Example (Fun 1, one digger):

```
lem_port replay 1
level 0 0
199 C 813 75 0
199 A 7
end 804
```
