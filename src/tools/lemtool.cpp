// SPDX-License-Identifier: GPL-3.0-or-later
// Command line utility for inspecting and exporting original game data.
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <cstring>
#include <filesystem>
#include <string>

#include "dat.h"
#include "graphicset.h"
#include "level.h"
#include "levelrender.h"
#include "maindat.h"
#include "levelpack.h"
#include "game.h"
#include "adlib.h"
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

// Runs a level without player input and writes snapshots.
int cmdSim(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "usage: lemtool sim <datadir> <rating 0-3> <level 1-30> <outdir> [ticks] [every] [script]\n"
                             "script: tick:skill:lemming,... (skill = climber..digger, nuke, rr+N, rr-N)\n");
        return 1;
    }
    std::string dir = argv[2], out = argv[5];
    int rating = std::atoi(argv[3]), number = std::atoi(argv[4]) - 1;
    int ticks = argc > 6 ? std::atoi(argv[6]) : 2000;
    int every = argc > 7 ? std::atoi(argv[7]) : 100;
    std::filesystem::create_directories(out);
    LevelPack pack(dir);
    Level L = pack.level(rating, number);
    GraphicSet gs = loadGraphicSet(dir, L.graphicSet);
    SpecialSet spec;
    if (L.specialSet) spec = loadSpecialSet(dir, L.specialSet - 1);
    MainData md = loadMainData(dir);
    AnimSet anims(md);
    Game game(L, gs, L.specialSet ? &spec : nullptr, anims, md);
    Palette pal = levelPalette(L.specialSet ? spec.vga : gs.vgaCustom);
    struct Cmd { int tick; std::string what; int lem; };
    std::vector<Cmd> script;
    if (argc > 8) {
        std::string sc = argv[8];
        size_t p = 0;
        while (p < sc.size()) {
            size_t e = sc.find(',', p);
            std::string item = sc.substr(p, e == std::string::npos ? std::string::npos : e - p);
            size_t a = item.find(':'), b = item.find(':', a + 1);
            script.push_back({std::atoi(item.substr(0, a).c_str()), item.substr(a + 1, b - a - 1),
                              b == std::string::npos ? 0 : std::atoi(item.substr(b + 1).c_str())});
            if (e == std::string::npos) break;
            p = e + 1;
        }
    }
    static const char* skillNames[] = {"climber", "floater", "bomber", "blocker", "builder", "basher", "miner", "digger"};
    Bitmap frame;
    for (int t = 1; t <= ticks && !game.finished(); ++t) {
        for (const Cmd& c : script) {
            if (c.tick != t) continue;
            if (c.what == "nuke") { game.nuke(); continue; }
            if (c.what.rfind("rr", 0) == 0) { game.setReleaseRateChange(c.what[2] == '+' ? 1 : -1); continue; }
            if (c.what == "rr0") { game.setReleaseRateChange(0); continue; }
            for (int k = 0; k < SkillCount; ++k) {
                if (c.what != skillNames[k]) continue;
                if (c.lem >= int(game.lemmings().size())) { std::printf("t%d: no lemming %d\n", t, c.lem); break; }
                const Lemming& l = game.lemmings()[c.lem];
                game.setCursor(l.x - 2, l.y - 5);
                bool ok = game.assignSkill(Skill(k));
                std::printf("t%5d assign %s to #%d (%s at %d,%d): %s\n", t, c.what.c_str(), c.lem,
                            actionName(l.action), l.x, l.y, ok ? "ok" : "refused");
            }
        }
        game.tick();
        if (t % every == 0) {
            game.render(frame);
            char name[64];
            std::snprintf(name, sizeof name, "/t%05d.png", t);
            savePng(out + name, frame, pal);
            int counts[int(Action::Count)] = {};
            for (const Lemming& l : game.lemmings())
                if (!l.removed) ++counts[int(l.action)];
            std::printf("t%5d %d:%02d out %2d in %2d |", t, game.minutes(), game.seconds(),
                        game.lemmingsOut(), game.lemmingsIn());
            for (int a = 0; a < int(Action::Count); ++a)
                if (counts[a]) std::printf(" %s=%d", actionName(Action(a)), counts[a]);
            std::printf("\n");
        }
    }
    std::string rp = game.recording(rating, number).save();
    writeFile(out + "/replay.lrp", Bytes(rp.begin(), rp.end()));
    GameResult r = game.result();
    std::printf("finished=%d iteration=%d rescued %d/%d (%d%% of %d%% needed) %s\n", game.finished(),
                game.iteration(), r.rescued, r.total, r.achieved, r.needed, r.success ? "SUCCESS" : "fail");
    return 0;
}

// Plays a recorded replay and prints the outcome.
int cmdReplay(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: lemtool replay <datadir> <file.lrp>\n"); return 1; }
    std::string dir = argv[2];
    Bytes text = readFile(argv[3]);
    Replay rp;
    if (!Replay::parse(std::string(text.begin(), text.end()), rp)) { std::fprintf(stderr, "bad replay\n"); return 1; }
    LevelPack pack(dir);
    Level L = pack.level(rp.rating, rp.number);
    GraphicSet gs = loadGraphicSet(dir, L.graphicSet);
    SpecialSet spec;
    if (L.specialSet) spec = loadSpecialSet(dir, L.specialSet - 1);
    MainData md = loadMainData(dir);
    AnimSet anims(md);
    Game game(L, gs, L.specialSet ? &spec : nullptr, anims, md);
    game.play(rp);
    while (!game.finished() && game.frameCount() < rp.frames + 100000) game.tick();
    GameResult r = game.result();
    std::printf("%s %d \"%s\": rescued %d/%d (%d%%, needed %d%%) frames %d %s\n", ratingName(rp.rating),
                rp.number + 1, L.name.c_str(), r.rescued, r.total, r.achieved, r.needed, game.frameCount(),
                r.success ? "SUCCESS" : "fail");
    return r.success ? 0 : 2;
}

// Renders a music track or a sound effect of the AdLib driver to a WAV file.
int cmdAudio(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "usage: lemtool audio <datadir> <music|sfx> <number> <out.wav> [seconds]\n");
        return 1;
    }
    std::unique_ptr<AdlibDriver> drv(AdlibDriver::load(argv[2]));
    if (!drv) { std::fprintf(stderr, "cannot load adlib.dat\n"); return 1; }
    int n = std::atoi(argv[4]);
    double seconds = argc > 6 ? std::atof(argv[6]) : 10.0;
    if (!std::strcmp(argv[3], "music")) drv->playMusic(n);
    else drv->playSfx(n);
    int rate = drv->sampleRate(), frames = int(seconds * rate);
    std::vector<int16_t> pcm(static_cast<size_t>(frames));
    drv->generate(pcm.data(), frames);
    if (!drv->ok()) { std::fprintf(stderr, "%s\n", drv->error().c_str()); return 1; }
    double sum = 0;
    int peak = 0;
    for (int16_t v : pcm) { sum += double(v) * v; peak = std::max(peak, std::abs(int(v))); }
    std::printf("%d samples at %d Hz, rms %.1f, peak %d\n", frames, rate, std::sqrt(sum / frames), peak);
    Bytes wav;
    auto put32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) wav.push_back(uint8_t(v >> (8 * i))); };
    auto put16 = [&](uint16_t v) { wav.push_back(uint8_t(v)); wav.push_back(uint8_t(v >> 8)); };
    auto tag = [&](const char* t) { wav.insert(wav.end(), t, t + 4); };
    tag("RIFF"); put32(uint32_t(36 + frames * 2)); tag("WAVE");
    tag("fmt "); put32(16); put16(1); put16(1); put32(uint32_t(rate)); put32(uint32_t(rate * 2)); put16(2); put16(16);
    tag("data"); put32(uint32_t(frames * 2));
    for (int16_t v : pcm) put16(uint16_t(v));
    writeFile(argv[5], wav);
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
        if (!std::strcmp(argv[1], "sim")) return cmdSim(argc, argv);
        if (!std::strcmp(argv[1], "audio")) return cmdAudio(argc, argv);
        if (!std::strcmp(argv[1], "replay")) return cmdReplay(argc, argv);
        std::fprintf(stderr, "unknown command %s\n", argv[1]);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
    }
    return 1;
}
