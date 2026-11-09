// SPDX-License-Identifier: GPL-3.0-or-later
// Command line utility for inspecting and exporting original game data.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "dat.h"
#include "graphicset.h"
#include "level.h"
#include "levelrender.h"
#include "maindat.h"
#include "levelpack.h"
#include "png.h"

using namespace lem;

namespace {

void savePng(const std::string& path, const Bitmap& b, const Palette& pal, bool transparentZero = false) {
    std::vector<uint8_t> rgba(size_t(b.w) * b.h * 4);
    for (size_t i = 0; i < b.px.size(); ++i) {
        uint8_t v = b.px[i];
        uint8_t* o = &rgba[i * 4];
        if (v == kTransparent || (transparentZero && v == 0)) { o[0] = o[1] = o[2] = 0; o[3] = 0; continue; }
        Rgb c = pal[v & 15];
        o[0] = c.r; o[1] = c.g; o[2] = c.b; o[3] = 255;
    }
    writePngRgba(path, b.w, b.h, rgba);
}

// Lays out bitmaps in rows on a sheet, separated by 2px gaps.
Bitmap sheet(const std::vector<const Bitmap*>& items, int maxW = 640) {
    int x = 0, y = 0, rowH = 0, W = 0;
    std::vector<std::pair<int, int>> pos;
    for (const Bitmap* b : items) {
        if (x > 0 && x + b->w > maxW) { x = 0; y += rowH + 2; rowH = 0; }
        pos.push_back({x, y});
        x += b->w + 2;
        rowH = std::max(rowH, b->h);
        W = std::max(W, x);
    }
    Bitmap s(std::max(W, 1), y + rowH + 1, kTransparent);
    for (size_t i = 0; i < items.size(); ++i)
        for (int yy = 0; yy < items[i]->h; ++yy)
            for (int xx = 0; xx < items[i]->w; ++xx)
                s.at(pos[i].first + xx, pos[i].second + yy) = items[i]->at(xx, yy);
    return s;
}

int cmdDat(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: lemtool dat <file> [outprefix]\n"); return 1; }
    auto secs = decompressDat(readFile(argv[2]));
    for (size_t i = 0; i < secs.size(); ++i) {
        std::printf("section %zu: %zu bytes\n", i, secs[i].data.size());
        if (argc > 3) writeFile(std::string(argv[3]) + "." + std::to_string(i), secs[i].data);
    }
    return 0;
}

int cmdLevels(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: lemtool levels <datadir> <outdir>\n"); return 1; }
    std::string dir = argv[2], out = argv[3];
    std::filesystem::create_directories(out);
    GraphicSet sets[5];
    bool loaded[5] = {};
    for (int f = 0; f < 10; ++f) {
        char name[32];
        std::snprintf(name, sizeof name, "level%03d.dat", f);
        auto secs = decompressDat(readFile(joinPath(dir, name)));
        for (size_t s = 0; s < secs.size(); ++s) {
            Level L = parseLevel(secs[s].data);
            if (L.graphicSet < 0 || L.graphicSet > 4) {
                std::printf("%s[%zu] unsupported graphic set %d\n", name, s, L.graphicSet);
                continue;
            }
            if (!loaded[L.graphicSet]) { sets[L.graphicSet] = loadGraphicSet(dir, L.graphicSet); loaded[L.graphicSet] = true; }
            const GraphicSet& gs = sets[L.graphicSet];
            SpecialSet spec;
            bool hasSpec = L.specialSet > 0;
            if (hasSpec) spec = loadSpecialSet(dir, L.specialSet - 1);
            Bitmap img = renderLevelPreview(L, gs, hasSpec ? &spec : nullptr);
            Palette pal = levelPalette(hasSpec ? spec.vga : gs.vgaCustom);
            char png[64];
            std::snprintf(png, sizeof png, "/level%03d_%zu.png", f, s);
            savePng(out + png, img, pal);
            std::printf("%s[%zu] g%d s%d rr%3d lem%3d save%3d t%d skills", name, s, L.graphicSet,
                        L.specialSet, L.releaseRate, L.lemmings, L.rescue, L.timeMinutes);
            for (int k = 0; k < SkillCount; ++k) std::printf(" %d", L.skills[k]);
            std::printf(" x%d obj%zu ter%zu steel%zu \"%s\"\n", L.startX, L.objects.size(),
                        L.terrain.size(), L.steel.size(), L.name.c_str());
        }
    }
    return 0;
}

int cmdGfx(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: lemtool gfx <datadir> <outdir>\n"); return 1; }
    std::string dir = argv[2], out = argv[3];
    std::filesystem::create_directories(out);
    for (int n = 0; n < 5; ++n) {
        GraphicSet gs = loadGraphicSet(dir, n);
        Palette pal = levelPalette(gs.vgaCustom);
        std::vector<const Bitmap*> ter, obj;
        for (auto& t : gs.terrain) if (t.used) ter.push_back(&t.bmp);
        for (int i = 0; i < 16; ++i) {
            const ObjectInfo& o = gs.objects[i];
            if (!o.used) continue;
            std::printf("set%d obj%2d %3dx%-3d frames %d..%d flags %d trig(%d,%d %dx%d) eff %d snd %d prev %d\n",
                        n, i, o.w, o.h, o.startFrame, o.endFrame, o.flags, o.trigLeft, o.trigTop,
                        o.trigW, o.trigH, int(o.effect), o.sound, o.previewFrameIndex());
            for (auto& f : o.frames) obj.push_back(&f);
        }
        savePng(out + "/terrain" + std::to_string(n) + ".png", sheet(ter), pal);
        savePng(out + "/objects" + std::to_string(n) + ".png", sheet(obj), pal);
    }
    for (int n = 0; n < 4; ++n) {
        SpecialSet s = loadSpecialSet(dir, n);
        savePng(out + "/special" + std::to_string(n) + ".png", s.terrain, levelPalette(s.vga));
    }
    return 0;
}

int cmdMain(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: lemtool main <datadir> <outdir>\n"); return 1; }
    std::string dir = argv[2], out = argv[3];
    std::filesystem::create_directories(out);
    MainData m = loadMainData(dir);
    GraphicSet gs = loadGraphicSet(dir, 0);
    Palette lvl = levelPalette(gs.vgaCustom);
    Palette menu = menuPalette();

    std::vector<const Bitmap*> lem;
    for (auto& a : m.anims) for (auto& f : a) lem.push_back(&f);
    savePng(out + "/lemmings.png", sheet(lem, 400), lvl);
    std::vector<const Bitmap*> masks;
    for (auto& a : m.masks) for (auto& f : a) masks.push_back(&f);
    for (auto& d : m.countdown) masks.push_back(&d);
    for (int d = 0; d < 10; ++d) masks.push_back(&m.skillDigitL[d]), masks.push_back(&m.skillDigitR[d]);
    savePng(out + "/masks.png", sheet(masks, 200), lvl);
    savePng(out + "/panel.png", m.panel, lvl);
    std::vector<const Bitmap*> pf;
    for (auto& c : m.panelFont) pf.push_back(&c);
    savePng(out + "/panelfont.png", sheet(pf, 400), lvl);

    savePng(out + "/menu_bg.png", m.menuBackground, menu);
    savePng(out + "/logo.png", m.logo, menu);
    std::vector<const Bitmap*> misc;
    for (auto& s : m.signs) misc.push_back(&s);
    misc.push_back(&m.iconMusic); misc.push_back(&m.iconFx);
    for (auto& s : m.ratingSigns) misc.push_back(&s);
    savePng(out + "/signs.png", sheet(misc, 640), menu);
    std::vector<const Bitmap*> anim;
    for (auto& b : m.blinks) for (auto& f : b) anim.push_back(&f);
    for (auto& f : m.scrollerLeft) anim.push_back(&f);
    for (auto& f : m.scrollerRight) anim.push_back(&f);
    anim.push_back(&m.reel);
    savePng(out + "/menu_anims.png", sheet(anim, 400), menu);
    std::vector<const Bitmap*> font;
    for (auto& c : m.menuFont) font.push_back(&c);
    savePng(out + "/menufont.png", sheet(font, 400), menu);
    return 0;
}

int cmdList(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: lemtool list <datadir> [outdir]\n"); return 1; }
    std::string dir = argv[2];
    LevelPack pack(dir);
    GraphicSet sets[5];
    bool loaded[5] = {};
    if (argc > 3) std::filesystem::create_directories(argv[3]);
    for (int r = 0; r < kRatings; ++r)
        for (int n = 0; n < kLevelsPerRating; ++n) {
            Level L = pack.level(r, n);
            auto s = LevelPack::slot(r, n);
            std::printf("%-6s %2d  rec %2d%s  rr %2d  %2d/%2d  %d min  \"%s\"\n", ratingName(r), n + 1,
                        s.record, s.variation ? "*" : " ", L.releaseRate, L.rescue, L.lemmings,
                        L.timeMinutes, L.name.c_str());
            if (argc <= 3) continue;
            int g = L.graphicSet;
            if (!loaded[g]) { sets[g] = loadGraphicSet(dir, g); loaded[g] = true; }
            SpecialSet spec;
            if (L.specialSet) spec = loadSpecialSet(dir, L.specialSet - 1);
            char png[64];
            std::snprintf(png, sizeof png, "/%s_%02d.png", ratingName(r), n + 1);
            savePng(std::string(argv[3]) + png,
                    renderLevelPreview(L, sets[g], L.specialSet ? &spec : nullptr),
                    levelPalette(L.specialSet ? spec.vga : sets[g].vgaCustom));
        }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: lemtool <dat|levels|gfx> ...\n");
        return 1;
    }
    try {
        if (!std::strcmp(argv[1], "dat")) return cmdDat(argc, argv);
        if (!std::strcmp(argv[1], "levels")) return cmdLevels(argc, argv);
        if (!std::strcmp(argv[1], "gfx")) return cmdGfx(argc, argv);
        if (!std::strcmp(argv[1], "main")) return cmdMain(argc, argv);
        if (!std::strcmp(argv[1], "list")) return cmdList(argc, argv);
        std::fprintf(stderr, "unknown command %s\n", argv[1]);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
    }
    return 1;
}
