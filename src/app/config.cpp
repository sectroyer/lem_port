// SPDX-License-Identifier: GPL-3.0-or-later
#include "config.h"

#include <SDL3/SDL.h>

#include <fstream>

namespace lem {

namespace {

std::string configPath() {
    char* base = SDL_GetPrefPath("lem_port", "lem_port");
    if (!base) return "lem_port.cfg";
    std::string p = std::string(base) + "config.txt";
    SDL_free(base);
    return p;
}

} // namespace

Config loadConfig() {
    Config c;
    std::ifstream f(configPath());
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        try {
            if (k == "data") c.dataDir = v;
            else if (k == "scale") c.scale = std::max(1, std::stoi(v));
            else if (k == "rating") c.lastRating = std::stoi(v);
            else if (k == "level") c.lastLevel = std::stoi(v);
        } catch (...) {
        }
    }
    return c;
}

void saveConfig(const Config& c) {
    std::ofstream f(configPath());
    f << "data=" << c.dataDir << "\n"
      << "scale=" << c.scale << "\n"
      << "rating=" << c.lastRating << "\n"
      << "level=" << c.lastLevel << "\n";
}

} // namespace lem
