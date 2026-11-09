// SPDX-License-Identifier: GPL-3.0-or-later
// Level definition (2048-byte record stored in levelXXX.dat sections).
// See docs/formats/level.md.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "dat.h"

namespace lem {

enum Skill : int {
    Climber = 0, Floater, Bomber, Blocker, Builder, Basher, Miner, Digger,
    SkillCount
};

struct LevelObject {
    int x = 0, y = 0;     // pixel position of the top-left corner
    int id = 0;           // index into the graphics set object table
    bool noOverwrite = false;  // drawn behind existing terrain
    bool onlyOnTerrain = false;// visible only where terrain exists
    bool upsideDown = false;
};

struct LevelTerrain {
    int x = 0, y = 0;
    int id = 0;
    bool noOverwrite = false;
    bool upsideDown = false;
    bool erase = false;
};

struct LevelSteel {
    int x = 0, y = 0, w = 0, h = 0;
};

struct Level {
    int releaseRate = 0;
    int lemmings = 0;
    int rescue = 0;
    int timeMinutes = 0;
    std::array<int, SkillCount> skills{};
    int startX = 0;
    int graphicSet = 0;
    int specialSet = 0;   // 0 = none, otherwise vgaspec(N-1)
    std::vector<LevelObject> objects;
    std::vector<LevelTerrain> terrain;
    std::vector<LevelSteel> steel;
    std::string name;
};

constexpr size_t kLevelRecordSize = 2048;

Level parseLevel(const Bytes& rec);

} // namespace lem
