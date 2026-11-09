#include "levelrender.h"

#include <algorithm>

namespace lem {

Bitmap buildTerrain(const Level& lvl, const GraphicSet& gs, const SpecialSet* spec) {
    Bitmap t(kLevelWidth, kLevelHeight);
    if (spec) {
        for (int y = 0; y < spec->terrain.h; ++y)
            for (int x = 0; x < spec->terrain.w; ++x)
                t.at(x + kSpecialOffsetX, y) = spec->terrain.at(x, y);
        return t;
    }
    for (const LevelTerrain& p : lvl.terrain) {
        if (p.id < 0 || p.id >= 64 || !gs.terrain[p.id].used) continue;
        const Bitmap& src = gs.terrain[p.id].bmp;
        for (int sy = 0; sy < src.h; ++sy) {
            int y = p.y + (p.upsideDown ? src.h - 1 - sy : sy);
            if (y < 0 || y >= kLevelHeight) continue;
            for (int sx = 0; sx < src.w; ++sx) {
                int x = p.x + sx;
                if (x < 0 || x >= kLevelWidth) continue;
                uint8_t c = src.at(sx, sy);
                if (c == kTransparent) continue;
                uint8_t& d = t.at(x, y);
                if (p.erase) d = kTransparent;
                else if (p.noOverwrite) { if (d == kTransparent) d = c; }
                else d = c;
            }
        }
    }
    return t;
}

void blitObject(Bitmap& dst, const Bitmap& terrain, const Bitmap& src, int x, int y,
                bool noOverwrite, bool onlyOnTerrain, bool upsideDown) {
    for (int sy = 0; sy < src.h; ++sy) {
        int dy = y + (upsideDown ? src.h - 1 - sy : sy);
        if (dy < 0 || dy >= dst.h) continue;
        for (int sx = 0; sx < src.w; ++sx) {
            int dx = x + sx;
            if (dx < 0 || dx >= dst.w) continue;
            uint8_t c = src.at(sx, sy);
            if (c == kTransparent) continue;
            bool solid = terrain.at(dx, dy) != kTransparent;
            if (noOverwrite && solid) continue;
            if (onlyOnTerrain && !solid) continue;
            dst.at(dx, dy) = c;
        }
    }
}

Bitmap renderLevelPreview(const Level& lvl, const GraphicSet& gs, const SpecialSet* spec) {
    Bitmap terrain = buildTerrain(lvl, gs, spec);
    Bitmap out(kLevelWidth, kLevelHeight, 0);
    // Objects flagged noOverwrite go behind terrain, others in front.
    for (const LevelObject& o : lvl.objects) {
        if (o.id < 0 || o.id >= 16 || !gs.objects[o.id].used || !o.noOverwrite) continue;
        const ObjectInfo& oi = gs.objects[o.id];
        int f = oi.previewFrameIndex();
        blitObject(out, terrain, oi.frames[f], o.x, o.y, false, o.onlyOnTerrain, o.upsideDown);
    }
    for (size_t i = 0; i < out.px.size(); ++i)
        if (terrain.px[i] != kTransparent) out.px[i] = terrain.px[i];
    for (const LevelObject& o : lvl.objects) {
        if (o.id < 0 || o.id >= 16 || !gs.objects[o.id].used || o.noOverwrite) continue;
        const ObjectInfo& oi = gs.objects[o.id];
        int f = oi.previewFrameIndex();
        blitObject(out, terrain, oi.frames[f], o.x, o.y, false, o.onlyOnTerrain, o.upsideDown);
    }
    return out;
}

} // namespace lem
