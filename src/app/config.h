// SPDX-License-Identifier: GPL-3.0-or-later
// Persistent user settings (data folder, window scale, progress).
#pragma once

#include <string>

namespace lem {

struct Config {
    std::string dataDir;
    int scale = 2;
    int lastRating = 0;
    int lastLevel = 0;
    bool music = true;
    bool sfx = true;
};

Config loadConfig();
void saveConfig(const Config& c);

} // namespace lem
