# lem_port

**lem_port** brings the 1991 DOS release of *Lemmings* to modern systems –
natively, without an emulator.

It is not a remake and not a reinterpretation. The aim is to play the
original game exactly as it was: the same 120 levels, the same rules and the
same timing, so that every solution that worked in 1991 still works today.

## Why another engine?

The best-known faithful recreation of DOS Lemmings is
[Lemmix](https://github.com/ericlangedijk/Lemmix) by Eric Langedijk – a
remarkable piece of work that captures the original down to its quirks.
Lemmix, however, is written in Delphi and runs only on Windows.

lem_port follows the same path with the same goal of accuracy, written in
portable C++20 on top of SDL3, so the game can be enjoyed on:

* **Linux**
* **macOS**
* **iOS and iPadOS**
* and later in the **browser** (WebAssembly) and on **Windows**

The engine core is free of platform code; each platform only provides a thin
layer for the window, input and sound.

## Game data

The original game files are **not** included and never will be. You need
your own copy of the DOS version of *Lemmings*. Point lem_port at the folder
that contains `main.dat`, `level000.dat`, `vgagr0.dat` and the other data
files – upper or lower case names both work.

## Building

You need CMake 3.20 or newer, a C++20 compiler and, for the game itself,
SDL3.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

On macOS with MacPorts, add `-DCMAKE_PREFIX_PATH=/opt/local` to the first
command.

## Playing

```sh
lem_port --data /path/to/lemmings      # remembered for next time
```

On first start without `--data`, a folder picker asks for the data folder.

| Input | Action |
|---|---|
| Left click on a lemming | assign the selected skill |
| Click a panel button, F3–F10, 1–8 | select a skill |
| F1 / F2, − / + | release rate down / up |
| F11, P | pause |
| Double-click nuke, F12 | nuke |
| F | fast forward |
| Arrow keys, mouse at screen edge, minimap | scroll |
| Hold right button | prefer walking lemmings when assigning |
| Alt+Enter | fullscreen |
| Esc | end the level |

On the level preview screen the arrow keys and Page Up/Down choose a level,
and C opens the access code entry. After a successful level the result
screen shows the access code for the next one.

## Tools

`lemtool` lets you look inside the original data files:

```sh
lemtool dat    <file.dat> [outprefix]   # list or extract sections
lemtool gfx    <datadir> <outdir>       # graphics sets as PNG sheets
lemtool levels <datadir> <outdir>       # every level record as PNG + summary
lemtool main   <datadir> <outdir>       # sprites, panel, fonts, menu art
lemtool list   <datadir> [outdir]       # the 120 levels in game order (+ PNGs)
lemtool sim    <datadir> <rating> <n> <outdir> [ticks] [every] [script]
                                        # play a level headless, save snapshots
```

## Documentation

* [Roadmap](docs/ROADMAP.md) – the plan and current progress
* [File formats](docs/formats/README.md) – how the original data is stored
* [Game mechanics](docs/mechanics.md) – how the simulation behaves

## Acknowledgements

lem_port stands on the shoulders of others:

* **DMA Design** created *Lemmings*, published by **Psygnosis** in 1991.
  The game and its data remain the property of their copyright holders.
* **Eric Langedijk** wrote [Lemmix](https://github.com/ericlangedijk/Lemmix),
  the inspiration for this project and the yardstick for faithful behaviour.
* **ccexplore** and **namida** refined Lemmix to match the DOS game even more
  closely; **Aaron Kelley** maintains that work as
  [Lemmix Player](https://github.com/AaronKelley/LemmixPlayer).
* **ccexplore**, **rt**, **Mindless** and **Simon** documented the original
  file formats and shared that knowledge with the Lemmings community.

Thank you all. No code from these projects is included here.

## License

Copyright © 2026 Michał Majchrowicz

lem_port is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [LICENSE](LICENSE) for details.

The original game data is not part of this project and is not covered by
this license.
