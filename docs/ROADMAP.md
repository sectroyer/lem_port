# Roadmap

Goal: a native, self-contained engine for DOS Lemmings that reads the
original data files from a folder chosen by the player. No emulator involved.

Targets: Linux and macOS first, then iOS and iPadOS, the browser
(WebAssembly) and Windows. The engine core has no platform dependencies;
SDL3 provides the window, input and sound on every target.

## Phase 1 – Data files
- [x] `.DAT` decompression (+ stored-mode writer)
- [x] Level records
- [x] Graphics sets (terrain, objects, palettes)
- [x] Special sets
- [x] `main.dat`: lemming animations, masks, panel, fonts, menu art
- [x] Level order and `oddtable.dat` variations (120 levels in 4 ratings)
- [ ] Format documentation for each of the above (`docs/formats/`)

## Phase 2 – Simulation
- [x] Fixed-rate tick loop, deterministic
- [x] Replays (record/playback of player actions)
- [x] Terrain/steel/object maps, trigger grid
- [x] Lemming states: walk, fall, climb, float, splat, block, build, bash,
      mine, dig, bomb, exit, drown, burn, traps
- [x] Release rate, nuke, time limit, win/lose
- [x] Regression tests: solution replays, replay determinism and golden
      results for all 120 levels

## Phase 3 – Desktop frontend (SDL3)
- [x] Data folder selection (command line, config file, first-run prompt)
- [x] Level view with scrolling, skill panel, minimap, cursor
- [x] Level info and result screens
- [x] Access codes
- [x] Main menu, code screen, original result messages
- [ ] Menu animations (blinking eyes, credits scroller)
- [x] Sound effects and music (original AdLib driver + OPL2 emulation)
- [x] Scaling, fullscreen, pause, fast forward

## Phase 4 – Other platforms
- [ ] WebAssembly build (Emscripten), data supplied by file picker
- [ ] iOS/iPadOS build, touch controls
