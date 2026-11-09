// SPDX-License-Identifier: GPL-3.0-or-later
#include "level.h"

#include <stdexcept>

namespace lem {

namespace {
int be16(const uint8_t* p) { return p[0] << 8 | p[1]; }
int sbe16(const uint8_t* p) { return int16_t(uint16_t(be16(p))); }
} // namespace

Level parseLevel(const Bytes& rec) {
    if (rec.size() != kLevelRecordSize) throw std::runtime_error("level: bad record size");
    const uint8_t* d = rec.data();
    Level L;
    L.releaseRate = be16(d + 0);
    L.lemmings = be16(d + 2);
    L.rescue = be16(d + 4);
    L.timeMinutes = be16(d + 6);
    for (int i = 0; i < SkillCount; ++i) L.skills[i] = be16(d + 8 + 2 * i);
    L.startX = be16(d + 0x18);
    L.graphicSet = be16(d + 0x1A);
    L.specialSet = be16(d + 0x1C);

    for (int i = 0; i < 32; ++i) {
        const uint8_t* o = d + 0x20 + 8 * i;
        bool blank = true;
        for (int k = 0; k < 8; ++k) blank &= o[k] == 0;
        if (blank) continue;
        LevelObject obj;
        obj.slot = i;
        obj.x = (sbe16(o) & ~7) - 16;
        obj.y = sbe16(o + 2);
        obj.id = o[5] & 15;
        obj.noOverwrite = o[6] & 0x80;
        obj.onlyOnTerrain = o[6] & 0x40;
        obj.upsideDown = o[7] & 0x80;
        L.objects.push_back(obj);
    }

    for (int i = 0; i < 400; ++i) {
        const uint8_t* t = d + 0x120 + 4 * i;
        if (t[0] == 0xFF && t[1] == 0xFF && t[2] == 0xFF && t[3] == 0xFF) continue;
        LevelTerrain ter;
        int flags = t[0] >> 4;
        ter.noOverwrite = flags & 8;
        ter.upsideDown = flags & 4;
        ter.erase = flags & 2;
        ter.x = ((t[0] & 0x0F) << 8 | t[1]) - 16;
        int y = t[2] << 1 | t[3] >> 7;
        if (y >= 256) y -= 512;
        ter.y = y - 4;
        ter.id = t[3] & 0x3F;
        L.terrain.push_back(ter);
    }

    for (int i = 0; i < 32; ++i) {
        const uint8_t* s = d + 0x760 + 4 * i;
        if (s[0] == 0 && s[1] == 0 && s[2] == 0 && s[3] == 0) continue;
        LevelSteel st;
        st.x = (s[0] << 1 | s[1] >> 7) * 4 - 16;
        st.y = (s[1] & 0x7F) * 4;
        st.w = (s[2] >> 4) * 4 + 4;
        st.h = (s[2] & 0x0F) * 4 + 4;
        L.steel.push_back(st);
    }

    L.name.assign(reinterpret_cast<const char*>(d + 0x7E0), 32);
    while (!L.name.empty() && (L.name.back() == ' ' || L.name.back() == 0)) L.name.pop_back();
    size_t first = L.name.find_first_not_of(' ');
    L.name = first == std::string::npos ? "" : L.name.substr(first);
    return L;
}

} // namespace lem
