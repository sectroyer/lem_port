# lemport

A native engine for the 1991 DOS release of *Lemmings*, written in C++20.

The game's data files are **not** included. You need your own copy of the
original DOS version; point the engine at the folder that contains
`main.dat`, `level000.dat`, `vgagr0.dat`, … (any letter case).

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
