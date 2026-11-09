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

### macOS app to share

```sh
tools/mac_package.sh build-mac
```

builds a self-contained universal `Lem_Port.app` (Apple Silicon and Intel,
macOS 11 or newer, SDL3 built in) and packs it as `build-mac/Lem_Port-macos.zip`.
The app is signed ad hoc; on another Mac open it the first time with right
click → Open.

### iOS and iPadOS

Works on iOS 12 and newer (tested on an iPad mini 2 with iOS 12.5). You need
Xcode, an Apple ID added in Xcode → Settings → Accounts (a free one is fine)
and, for installing from the command line, libimobiledevice
(`sudo port install ideviceinstaller` or `brew install ideviceinstaller`).

```sh
# Xcode project; SDL3 is downloaded and built for iOS
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_ARCHITECTURES=arm64

# signed build for devices (TEAM = your team id, see below)
cmake --build build-ios --config Release --target lem_port -- \
      -sdk iphoneos -allowProvisioningUpdates DEVELOPMENT_TEAM=TEAM

# install on the USB-connected device and copy the game data
tools/ios_install.sh build-ios/Release-iphoneos/Lem_Port.app /path/to/lemmings
```

* The team id is the `OU=` value of your signing certificate:
  `security find-certificate -c "Apple Development" -p | openssl x509 -noout -subject`.
* A device must be registered with the team once: open
  `build-ios/lem_port.xcodeproj` in Xcode, choose the device and press Run.
  Afterwards the build and `ios_install.sh` are enough.
* With a free Apple ID an installed app runs for 7 days; build and install
  again to renew it. Updates keep the game data.
* iOS 12–14 devices need Xcode's support files for that version in
  `Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/DeviceSupport`.
* On the first start, trust the developer on the device: Settings →
  General → Device Management.
* The game data can also be copied with Finder (device → Files) or the Files
  app into the Lem_Port folder; the game starts once the files are there.

Touch controls: tap = click (tap a lemming to assign the selected skill),
drag the level to scroll, double tap TIME for fast forward, three-finger tap
ends a level, two-finger tap returns to the menu from the preview and result
screens.

### Tests

The regression tests in `tests/data_tests.cpp` need the original data and run
only when `LEM_DATA` points to it (`LEM_DATA=/path/to/lemmings ctest ...`).
They play the replays in `tests/replays` and check every level against
`tests/golden.txt`.

On macOS with MacPorts, add `-DCMAKE_PREFIX_PATH=/opt/local` to the first
command.

## Playing

```sh
Lem_Port --data /path/to/lemmings      # remembered for next time
```

On first start without `--data`, a folder picker asks for the data folder.

| Input | Action |
|---|---|
| Left click on a lemming | assign the selected skill |
| Click a panel button, F3–F10, 1–8 | select a skill |
| F1 / F2, − / + | release rate down / up |
| F11, P | pause |
| Double-click nuke, F12 | nuke |
| F, double click on TIME | fast forward |
| M / X | music / sound effects on or off |
| Arrow keys, mouse at screen edge, minimap | scroll |
| Hold right button | prefer walking lemmings when assigning |
| Alt+Enter | fullscreen |
| Esc | end the level |

Main menu: F1 (or click the sign) plays, F2 enters an access code, F4 shows
the controls, F3 cycles
music + effects / effects only / silence, the arrow keys or the rating sign
change the rating, Esc quits. On the level preview screen the arrow keys and
Page Up/Down also choose any level. After a successful level the result screen
shows the access code for the next one; the right mouse button returns to the
menu.

## Tools

`lemtool` lets you look inside the original data files:

```sh
lemtool dat    <file.dat> [outprefix]   # list or extract sections
lemtool gfx    <datadir> <outdir>       # graphics sets as PNG sheets
lemtool levels <datadir> <outdir>       # every level record as PNG + summary
lemtool main   <datadir> <outdir>       # sprites, panel, fonts, menu art
lemtool list   <datadir> [outdir]       # the 120 levels in game order (+ PNGs)
lemtool audio  <datadir> <music|sfx> <n> <out.wav> [seconds]
lemtool replay <datadir> <file.lrp>     # play a replay, print the result
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
* **Aaron Giles** wrote [ymfm](https://github.com/aaronsgiles/ymfm)
  (BSD-3-Clause), which emulates the AdLib's FM chip; it is included in
  `third_party/ymfm`.
* **ccexplore**, **rt**, **Mindless** and **Simon** documented the original
  file formats and shared that knowledge with the Lemmings community.

Thank you all. Apart from ymfm, no code from these projects is included here.

## License

Copyright © 2026 Michał Majchrowicz

lem_port is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [LICENSE](LICENSE) for details.

The original game data is not part of this project and is not covered by
this license.
