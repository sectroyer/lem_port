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
- [ ] Replays (record/playback of player actions)
- [x] Terrain/steel/object maps, trigger grid
- [x] Lemming states: walk, fall, climb, float, splat, block, build, bash,
      mine, dig, bomb, exit, drown, burn, traps
- [x] Release rate, nuke, time limit, win/lose
- [ ] Regression tests with recorded solutions

## Phase 3 – Desktop frontend (SDL3)
- [x] Data folder selection (command line, config file, first-run prompt)
- [x] Level view with scrolling, skill panel, minimap, cursor
- [x] Level info and result screens
- [ ] Main menu, passwords
- [ ] Sound effects and music (recreated where needed)
- [x] Scaling, fullscreen, pause, fast forward

## Phase 4 – Other platforms
- [ ] WebAssembly build (Emscripten), data supplied by file picker
- [ ] iOS/iPadOS build, touch controls
