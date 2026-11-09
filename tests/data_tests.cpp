// SPDX-License-Identifier: GPL-3.0-or-later
// Regression tests that need the original game data. Set LEM_DATA to the
// data folder; without it the tests are skipped.
//
//   data_tests <replay dir> <golden file> [--update]
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include "game.h"
#include "levelpack.h"

using namespace lem;

namespace {

int failures = 0;

struct Data {
    std::string dir;
    LevelPack pack;
    MainData md;
    AnimSet anims;
    std::map<int, GraphicSet> sets;
    std::map<int, SpecialSet> specs;
    explicit Data(const std::string& d) : dir(d), pack(d), md(loadMainData(d)), anims(md) {}

    std::unique_ptr<Game> start(int rating, int number) {
        Level L = pack.level(rating, number);
        if (!sets.count(L.graphicSet)) sets.emplace(L.graphicSet, loadGraphicSet(dir, L.graphicSet));
        const SpecialSet* spec = nullptr;
        if (L.specialSet) {
            if (!specs.count(L.specialSet)) specs.emplace(L.specialSet, loadSpecialSet(dir, L.specialSet - 1));
            spec = &specs.at(L.specialSet);
        }
        return std::make_unique<Game>(L, sets.at(L.graphicSet), spec, anims, md);
    }
};

// Hash of everything that matters: terrain and every lemming's state.
uint64_t stateHash(const Game& g) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    for (uint8_t p : g.terrain().px) mix(p);
    for (const Lemming& l : g.lemmings()) {
        mix(uint64_t(l.x) & 0xFFFF); mix(uint64_t(l.y) & 0xFFFF); mix(uint64_t(l.dx + 2));
        mix(uint64_t(l.action)); mix(uint64_t(l.frame)); mix(uint64_t(l.removed));
        mix(uint64_t(l.explosionTimer)); mix(uint64_t(l.bricksLeft)); mix(l.keptBits);
    }
    mix(uint64_t(g.lemmingsIn())); mix(uint64_t(g.lemmingsOut())); mix(uint64_t(g.frameCount()));
    return h;
}

// Plays a level with a fixed pattern of inputs that exercises every skill.
void scriptedRun(Game& g) {
    for (int f = 0; f < 2500 && !g.finished(); ++f) {
        if (f >= 60 && f % 37 == 0 && !g.lemmings().empty()) {
            const auto& ls = g.lemmings();
            const Lemming& l = ls[size_t(f / 37) % ls.size()];
            g.setCursor(l.x - 2, l.y - 5, (f / 37) % 5 == 0);
            g.assignSkill(Skill((f / 37) % SkillCount));
        }
        if (f == 300) g.setReleaseRateChange(1);
        if (f == 330) g.setReleaseRateChange(0);
        if (f == 1800) g.nuke();
        if (f == 100 || f == 101) g.tickPaused();
        else g.tick();
    }
}

void testReplays(Data& data, const std::string& dir) {
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.path().extension() != ".lrp") continue;
        Bytes b = readFile(e.path().string());
        std::string text(b.begin(), b.end());
        Replay rp;
        if (!Replay::parse(text, rp)) { std::printf("FAIL %s: cannot parse\n", e.path().c_str()); ++failures; continue; }
        int wantRescued = -1, wantFrames = -1;
        if (auto p = text.find("# expect"); p != std::string::npos)
            std::sscanf(text.c_str() + p, "# expect rescued %d frames %d", &wantRescued, &wantFrames);
        auto g = data.start(rp.rating, rp.number);
        g->play(rp);
        while (!g->finished() && g->frameCount() < rp.frames + 100000) g->tick();
        int rescued = g->result().rescued, frames = g->frameCount();
        bool ok = rescued == wantRescued && frames == wantFrames;
        std::printf("%s %s: rescued %d frames %d (expected %d / %d)\n", ok ? "ok  " : "FAIL",
                    e.path().filename().c_str(), rescued, frames, wantRescued, wantFrames);
        if (!ok) ++failures;
    }
}

void testDeterminismAndGolden(Data& data, const std::string& golden, bool update) {
    std::map<std::string, std::string> expected;
    if (!update) {
        if (FILE* f = std::fopen(golden.c_str(), "r")) {
            char key[32], val[128];
            while (std::fscanf(f, "%31s %127[^\n]", key, val) == 2) expected[key] = val;
            std::fclose(f);
        }
    }
    std::string out;
    for (int r = 0; r < kRatings; ++r)
        for (int n = 0; n < kLevelsPerRating; ++n) {
            auto g = data.start(r, n);
            scriptedRun(*g);
            Replay rp = g->recording(r, n);
            auto g2 = data.start(r, n);
            g2->play(rp);
            if (stateHash(*g) != stateHash(*g2)) {
                std::printf("FAIL %s %d: replay does not reproduce the game\n", ratingName(r), n + 1);
                ++failures;
            }
            char key[32], val[128];
            std::snprintf(key, sizeof key, "%s-%02d", ratingName(r), n + 1);
            std::snprintf(val, sizeof val, "frames=%d in=%d hash=%016llx", g->frameCount(), g->lemmingsIn(),
                          static_cast<unsigned long long>(stateHash(*g)));
            out += std::string(key) + " " + val + "\n";
            if (!update && expected.count(key) && expected[key] != val) {
                std::printf("FAIL %s: %s (expected %s)\n", key, val, expected[key].c_str());
                ++failures;
            }
        }
    if (update) {
        writeFile(golden, Bytes(out.begin(), out.end()));
        std::printf("golden file written: %s\n", golden.c_str());
    } else if (expected.empty()) {
        std::printf("FAIL no golden data in %s\n", golden.c_str());
        ++failures;
    }
}

} // namespace

int main(int argc, char** argv) {
    const char* dir = std::getenv("LEM_DATA");
    if (!dir || !*dir) {
        std::printf("LEM_DATA not set, skipping data tests\n");
        return 77;
    }
    if (argc < 3) { std::fprintf(stderr, "usage: data_tests <replay dir> <golden file> [--update]\n"); return 1; }
    Data data(dir);
    testReplays(data, argv[1]);
    testDeterminismAndGolden(data, argv[2], argc > 3 && std::string(argv[3]) == "--update");
    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("all data tests passed\n");
    return 0;
}
