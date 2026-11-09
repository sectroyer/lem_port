# lemport

A native engine for the 1991 DOS release of *Lemmings*, written in C++20.

The game's data files are **not** included. You need your own copy of the
original DOS version; point the engine at the folder that contains
`main.dat`, `level000.dat`, `vgagr0.dat`, … (any letter case).

## About

The goal is to play the original DOS game exactly as it was – same levels,
same rules, same timing, so original solutions keep working – on modern
systems, without an emulator.

This project follows the path of [Lemmix](https://github.com/ericlangedijk/Lemmix),
the faithful recreation of DOS Lemmings by Eric Langedijk. Lemmix is written
in Delphi and runs on Windows only. lemport aims for the same accuracy in a
portable, modern language (C++20 with SDL3), so the game can run on:

* Linux
* macOS
* iOS / iPadOS
* and later the browser (WebAssembly) and Windows

The engine core has no platform dependencies; each platform only provides a
thin frontend for windowing, input and audio.

## Building

Requirements: CMake ≥ 3.20, a C++20 compiler, SDL3 (frontend only).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

On macOS with MacPorts add `-DCMAKE_PREFIX_PATH=/opt/local`.

## Tools

`lemtool` inspects and exports original data:

```sh
lemtool dat    <file.dat> [outprefix]   # list / extract sections
lemtool gfx    <datadir> <outdir>       # graphics sets as PNG sheets
lemtool levels <datadir> <outdir>       # every level as PNG + summary
lemtool main   <datadir> <outdir>       # sprites, panel, fonts, menu art
lemtool list   <datadir> [outdir]       # 120 levels in game order (+ PNGs)
```

## Documentation

* [docs/ROADMAP.md](docs/ROADMAP.md) – plan and progress
* [docs/formats/](docs/formats/README.md) – data file formats

## Acknowledgements

* *Lemmings* was created by DMA Design and published by Psygnosis (1991).
  All game data remains the property of its copyright holders.
* [Lemmix](https://github.com/ericlangedijk/Lemmix) by Eric Langedijk – the
  reference for DOS-accurate behaviour, and the inspiration for this project.
* [Lemmix Player](https://github.com/AaronKelley/LemmixPlayer), with
  accuracy improvements by ccexplore and namida, maintained by Aaron Kelley.
* The Lemmings community file format documentation by ccexplore, rt,
  Mindless and Simon, which made reading the original data files possible.

No code from these projects is included in this repository.

## License

Copyright (C) 2026 Michał Majchrowicz

This program is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option)
any later version. See [LICENSE](LICENSE).

The original game data is not part of this project and is not covered by
this license.
