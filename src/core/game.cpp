// SPDX-License-Identifier: GPL-3.0-or-later
#include "game.h"

#include <algorithm>

#include "levelrender.h"

namespace lem {

namespace {

// Object map cell values. 0..127 = index of a triggered trap object.
constexpr uint8_t kMapNone = 128;
constexpr uint8_t kMapExit = 129;
constexpr uint8_t kMapForceLeft = 130;
constexpr uint8_t kMapForceRight = 131;
constexpr uint8_t kMapWater = 133;
constexpr uint8_t kMapFire = 134;
constexpr uint8_t kMapOneWayLeft = 135;
constexpr uint8_t kMapOneWayRight = 136;
constexpr uint8_t kMapSteel = 137;
constexpr uint8_t kMapBlocker = 138;

constexpr int kMapOffset = 16;                   // pixels of margin left/top
constexpr int kMapW = (1647 + kMapOffset) / 4 + 1;
constexpr int kMapH = (175 + kMapOffset) / 4;

constexpr int kHeadMinY = -5;
constexpr int kMinX = 0;
constexpr int kMaxX = 1647;
constexpr int kMaxY = 163;
constexpr int kMaxFall = 60;

constexpr int kEntranceId = 1;

struct FloatStep { int dy, frame; };
// Umbrella animation: frames 0..3 open the umbrella, 4..7 float. After the
// last entry the sequence continues from entry 8.
constexpr FloatStep kFloat[16] = {
    {3, 1}, {3, 2}, {3, 3}, {3, 5}, {-1, 5}, {0, 5}, {1, 5}, {1, 5},
    {2, 5}, {2, 6}, {2, 7}, {2, 7}, {2, 6}, {2, 5}, {2, 4}, {2, 4},
};

constexpr uint8_t kParticleColors[16] = {4, 15, 14, 13, 12, 11, 10, 9, 8, 11, 10, 9, 8, 7, 6, 2};

int mapCell(int v) { return (v & ~3) / 4; }

// Action word bits of the original game. Some transitions OR a new bit into
// the word instead of replacing it, and the old bit keeps affecting the
// cursor priority and skill checks.
uint16_t actionBit(Action a) {
    switch (a) {
    case Action::Splatting: return 0x0001;
    case Action::Exploding: return 0x0002;
    case Action::Falling: return 0x0004;
    case Action::Jumping: return 0x0008;
    case Action::Digging: return 0x0010;
    case Action::Climbing: return 0x0020;
    case Action::Hoisting: return 0x0040;
    case Action::Building: return 0x0080;
    case Action::Blocking: return 0x0100;
    case Action::Bashing: return 0x0200;
    case Action::Floating: return 0x0400;
    case Action::Mining: return 0x0800;
    case Action::Drowning: return 0x1000;
    case Action::Exiting: return 0x2000;
    case Action::Vaporizing: return 0x4000;
    case Action::Ohnoing: return 0x8000;
    default: return 0;   // walker; a shrugger is a walker with a flag
    }
}

constexpr uint16_t kPriorityBits = 0x8B90;     // oh-no, miner, basher, blocker, builder, digger
constexpr uint16_t kNoClimbFloatBits = 0x0103; // blocker, exploder, splatter
constexpr int kExplodeFrames = 52;

bool isPriority(Action a) {
    switch (a) {
    case Action::Blocking: case Action::Building: case Action::Shrugging:
    case Action::Bashing: case Action::Mining: case Action::Digging: case Action::Ohnoing:
        return true;
    default:
        return false;
    }
}

} // namespace

const char* actionName(Action a) {
    static const char* names[] = {
        "WALKER", "JUMPER", "DIGGER", "CLIMBER", "DROWNING", "CLIMBER", "BUILDER",
        "BASHER", "MINER", "FALLER", "FLOATER", "SPLATTING", "EXITING", "FRYING",
        "BLOCKER", "SHRUGGING", "BOMBER", "BOMBER"};
    return names[size_t(a)];
}

AnimSet::AnimSet(const MainData& m) {
    auto set = [&](Action a, std::initializer_list<Anim> r, std::initializer_list<Anim> l, bool loop) {
        for (int dir = 0; dir < 2; ++dir) {
            LemmingAnim& la = anims_[size_t(a)][dir];
            la.frames.clear();
            for (Anim src : dir == 0 ? r : l)
                for (const Bitmap& f : m.anims[size_t(src)]) la.frames.push_back(&f);
            const AnimSpec& sp = animSpec(*(dir == 0 ? r : l).begin());
            la.footX = sp.w / 2;
            la.footY = sp.h;
            la.loop = loop;
        }
    };
    set(Action::Walking, {Anim::WalkR}, {Anim::WalkL}, true);
    set(Action::Jumping, {Anim::JumpR}, {Anim::JumpL}, false);
    set(Action::Digging, {Anim::Dig}, {Anim::Dig}, true);
    set(Action::Climbing, {Anim::ClimbR}, {Anim::ClimbL}, true);
    set(Action::Drowning, {Anim::Drown}, {Anim::Drown}, false);
    set(Action::Hoisting, {Anim::PostClimbR}, {Anim::PostClimbL}, false);
    set(Action::Building, {Anim::BuildR}, {Anim::BuildL}, true);
    set(Action::Bashing, {Anim::BashR}, {Anim::BashL}, true);
    set(Action::Mining, {Anim::MineR}, {Anim::MineL}, true);
    set(Action::Falling, {Anim::FallR}, {Anim::FallL}, true);
    set(Action::Floating, {Anim::PreFloatR, Anim::FloatR}, {Anim::PreFloatL, Anim::FloatL}, true);
    set(Action::Splatting, {Anim::Splat}, {Anim::Splat}, false);
    set(Action::Exiting, {Anim::Exit}, {Anim::Exit}, false);
    set(Action::Vaporizing, {Anim::Burn}, {Anim::Burn}, false);
    set(Action::Blocking, {Anim::Block}, {Anim::Block}, true);
    set(Action::Shrugging, {Anim::ShrugR}, {Anim::ShrugL}, false);
    set(Action::Ohnoing, {Anim::OhNo}, {Anim::OhNo}, false);
    set(Action::Exploding, {Anim::Explosion}, {Anim::Explosion}, false);
    // The explosion graphic is centred on the lemming, slightly below its feet.
    for (auto& la : anims_[size_t(Action::Exploding)]) la.footY = 25;
    // The digger frame is anchored 12 pixels above the feet (sprite is 14 high).
    for (auto& la : anims_[size_t(Action::Digging)]) la.footY = 12;
}

// ---------------------------------------------------------------------------
// Setup

Game::Game(const Level& lvl, const GraphicSet& gs, const SpecialSet* spec, const AnimSet& anims,
           const MainData& md)
    : level_(lvl), gs_(gs), anims_(anims), md_(md) {
    world_ = buildTerrain(lvl, gs, spec);
    maxLemmings_ = lvl.lemmings;
    releaseRate_ = lvl.releaseRate;
    skills_ = lvl.skills;
    minutes_ = lvl.timeMinutes;
    seconds_ = 0;

    int entries = 0;
    for (const LevelObject& o : lvl.objects) {
        if (o.id < 0 || o.id >= 16 || !gs.objects[o.id].used) continue;
        ObjectState st;
        st.def = o;
        st.info = &gs.objects[o.id];
        st.frame = st.info->startFrame;
        // Continuously animated objects are advanced once before the first frame.
        if (st.info->flags == 2 && !o.inactive)
            st.frame = (st.frame + 1) % std::max<int>(1, int(st.info->frames.size()));
        if (o.id == kEntranceId && entries < 4) entryTable_[entries++] = int(objects_.size());
        objects_.push_back(st);
    }
    // Entrances are used in a fixed rotation of four slots.
    switch (entries) {
    case 1: entryTable_[1] = entryTable_[2] = entryTable_[3] = entryTable_[0]; break;
    case 2: entryTable_[2] = entryTable_[1]; entryTable_[3] = entryTable_[0]; break;  // ABBA
    case 3: entryTable_[3] = entryTable_[1]; break;                                   // ABCB
    default: break;
    }
    initObjectMap();
}

void Game::initObjectMap() {
    objMap_.assign(size_t(kMapW) * kMapH, kMapNone);
    for (const LevelSteel& s : level_.steel)
        for (int y = s.y; y < s.y + s.h; ++y)
            for (int x = s.x; x < s.x + s.w; ++x) writeMap(x, y, kMapSteel);
    for (size_t i = 0; i < objects_.size(); ++i) {
        const ObjectState& o = objects_[i];
        if (o.def.slot >= 16 || o.def.inactive) continue;   // slots 16..31 have no effect
        const ObjectInfo& oi = *o.info;
        uint8_t v = oi.effect == TriggerEffect::Trap ? uint8_t(i) : uint8_t(int(oi.effect) + 128);
        int x0 = (o.def.x & ~3) + oi.trigLeft;
        int y0 = (o.def.y & ~3) + oi.trigTop;
        for (int y = y0; y < y0 + oi.trigH; ++y)
            for (int x = x0; x < x0 + oi.trigW; ++x) writeMap(x, y, v);
    }
}

// ---------------------------------------------------------------------------
// Maps

uint8_t Game::readMap(int x, int y) const {
    int cx = mapCell(x) + kMapOffset / 4, cy = mapCell(y) + kMapOffset / 4;
    if (cx < 0 || cy < 0 || cx >= kMapW || cy >= kMapH) return kMapNone;
    return objMap_[size_t(cy) * kMapW + cx];
}

void Game::writeMap(int x, int y, uint8_t v) {
    int cx = mapCell(x) + kMapOffset / 4, cy = mapCell(y) + kMapOffset / 4;
    if (cx < 0 || cy < 0 || cx >= kMapW || cy >= kMapH) return;
    objMap_[size_t(cy) * kMapW + cx] = v;
}

static const int kFieldDx[9] = {-4, 0, 4, -4, 0, 4, -4, 0, 4};
static const int kFieldDy[9] = {-6, -6, -6, -2, -2, -2, 2, 2, 2};

void Game::saveMap(Lemming& l) {
    for (int i = 0; i < 9; ++i) l.savedMap[i] = readMap(l.x + kFieldDx[i], l.y + kFieldDy[i]);
}

void Game::restoreMap(Lemming& l) {
    for (int i = 0; i < 9; ++i) writeMap(l.x + kFieldDx[i], l.y + kFieldDy[i], l.savedMap[i]);
}

void Game::setBlockerField(Lemming& l) {
    static const uint8_t v[3] = {kMapForceLeft, kMapBlocker, kMapForceRight};
    for (int i = 0; i < 9; ++i) writeMap(l.x + kFieldDx[i], l.y + kFieldDy[i], v[i % 3]);
}

bool Game::overlapsBlockerField(const Lemming& l) const {
    for (int i = 0; i < 9; ++i) {
        uint8_t v = readMap(l.x + kFieldDx[i], l.y + kFieldDy[i]);
        if (v == kMapForceLeft || v == kMapBlocker || v == kMapForceRight) return true;
    }
    return false;
}

bool Game::hasPixel(int x, int y) const {
    return x >= 0 && y >= 0 && x < world_.w && y < world_.h && world_.at(x, y) != kTransparent;
}

bool Game::hasPixelClipY(int x, int y, int minY) const {
    return hasPixel(x, y >= minY ? y : minY);
}

void Game::removePixel(int x, int y) {
    if (x >= 0 && y >= 0 && x < world_.w && y < world_.h) world_.at(x, y) = kTransparent;
}

void Game::applyMask(const Bitmap& mask, int x, int y) {
    for (int my = 0; my < mask.h; ++my)
        for (int mx = 0; mx < mask.w; ++mx)
            if (mask.at(mx, my) != kTransparent) removePixel(x + mx, y + my);
}

// ---------------------------------------------------------------------------
// Lemming helpers

void Game::setAnim(Lemming& l) { l.anim = &anims_.get(l.action, l.dx < 0); }

void Game::transition(Lemming& l, Action a, bool turn) {
    if (l.action == a && !turn) return;
    if (turn) l.dx = -l.dx;
    Action old = l.action;
    l.action = a;
    setAnim(l);
    if (old == a) return;
    if (a == Action::Ohnoing || a == Action::Exploding)
        l.keptBits |= actionBit(old);
    else if (a == Action::Falling && (old == Action::Digging || old == Action::Bashing ||
                                      old == Action::Mining || old == Action::Climbing))
        l.keptBits |= actionBit(old);
    else
        l.keptBits = 0;
    l.frame = 0;
    l.endOfAnim = false;
    l.fallen = 0;
    l.bricksLeft = 0;
    switch (a) {
    case Action::Splatting:
        l.explosionTimer = 0;
        l.dx = 0;
        cue(SfxSplat);
        break;
    case Action::Blocking:
        l.blocking = true;
        saveMap(l);
        setBlockerField(l);
        break;
    case Action::Exiting: cue(SfxYippee); break;
    case Action::Digging: l.newDigger = true; break;
    case Action::Falling: l.fallen = 3; break;
    case Action::Building: l.bricksLeft = 12; break;
    case Action::Ohnoing: cue(SfxOhNo); break;
    case Action::Exploding: cue(SfxExplode); break;
    case Action::Floating: l.floatIndex = 0; break;
    case Action::Mining: ++l.y; break;
    default: break;
    }
}

void Game::turnAround(Lemming& l) {
    l.dx = -l.dx;
    setAnim(l);
}

bool Game::checkTopBoundary(Lemming& l) {
    int top = -l.anim->footY;
    if (l.y + top >= kHeadMinY) return false;
    l.y = kHeadMinY - 2 - top;
    turnAround(l);
    if (l.action == Action::Jumping) transition(l, Action::Walking);
    return true;
}

void Game::removeLemming(Lemming& l) {
    l.removed = true;
    --lemmingsOut_;
    ++lemmingsRemoved_;
}

void Game::cue(int sfx) {
    if (sfx > 0) sounds_.push_back(sfx);
}

std::vector<int> Game::takeSounds() {
    std::vector<int> s;
    s.swap(sounds_);
    return s;
}

// ---------------------------------------------------------------------------
// Main loop

void Game::updateReleaseRate() {
    // While a button is held the rate moves 5 steps (one per timer interrupt
    // of the frame), then the panel redraw blocks it for two frames.
    if (rrPhase_ > 0) {
        rrPhase_ = (rrPhase_ + 1) % 3;
        return;
    }
    if (rrChange_ == 0) return;
    int n = std::clamp(releaseRate_ + 5 * rrChange_, level_.releaseRate, 99);
    if (n != releaseRate_) {
        releaseRate_ = n;
        rrPhase_ = 1;
    }
}

void Game::advanceIntro() {
    ++iteration_;
    switch (iteration_) {
    case 16: cue(SfxLetsGo); break;
    case 36:
        cue(SfxEntrance);
        entriesOpen_ = true;
        for (ObjectState& o : objects_)
            if (o.def.id == kEntranceId) { o.triggered = true; o.frame = 1; }
        break;
    default: break;
    }
}

void Game::tickPaused() {
    if (finished_) return;
    updateReleaseRate();
    if (!entriesOpen_) advanceIntro();
}

void Game::tick() {
    if (finished_) return;
    checkFinished();
    if (finished_) return;

    updateReleaseRate();
    advanceIntro();
    if (timeUpFrames_ < 0 && ++clockFrame_ == kTicksPerSecond) {
        clockFrame_ = 0;
        if (--seconds_ < 0) {
            --minutes_;
            seconds_ = 59;
        }
    }

    releaseLemming();
    updateLemmings();
    updateNuke();
    updateObjects();

    int a, b;
    hitTest(a, b, hoverCount_);
    hovered_ = a;

    // When the clock reaches 0:00 the original still runs two more frames.
    if (timeUpFrames_ < 0 && minutes_ <= 0 && seconds_ <= 0) timeUpFrames_ = 2;
}

void Game::checkFinished() {
    if (timeUpFrames_ >= 0) {
        if (timeUpFrames_-- == 0) finished_ = timeUp_ = true;
        return;
    }
    if (lemmingsIn_ >= maxLemmings_ || lemmingsRemoved_ >= maxLemmings_ ||
        (nuking_ && lemmingsOut_ == 0))
        finished_ = true;
}

GameResult Game::result() const {
    GameResult r;
    r.total = maxLemmings_;
    r.rescued = lemmingsIn_;
    r.needed = level_.lemmings ? level_.rescue * 100 / level_.lemmings : 0;
    r.achieved = r.total ? r.rescued * 100 / r.total : 0;
    r.success = r.achieved >= r.needed;
    r.timeUp = timeUp_;
    return r;
}

void Game::releaseLemming() {
    if (!entriesOpen_ || nuking_ || timeUpFrames_ >= 0) return;
    if (--nextRelease_ != 0) return;
    int rr = 99 - releaseRate_;
    if (rr < 0) rr += 256;
    nextRelease_ = rr / 2 + 4;
    if (lemmingsReleased_ >= maxLemmings_) return;
    int ix = entryTable_[lemmingsReleased_ % 4];
    if (ix < 0) return;
    Lemming l;
    l.action = Action::Walking;   // so that the transition below takes effect
    l.dx = 1;
    transition(l, Action::Falling);
    l.x = objects_[ix].def.x + 24;
    l.y = objects_[ix].def.y + 14;
    l.objBelow = l.objInFront = kMapNone;
    lemmings_.push_back(l);
    ++lemmingsReleased_;
    ++lemmingsOut_;
}

void Game::updateLemmings() {
    for (Lemming& l : lemmings_) {
        if (l.removed) continue;
        if (l.explosionTimer != 0 && updateExplosionTimer(l)) continue;
        if (handle(l)) interactWithObjects(l);
    }
}

void Game::updateNuke() {
    if (!nuking_ || !nukeInProgress_) return;
    int n = int(lemmings_.size());
    // Removed lemmings are skipped, unless they still carry a bomb timer.
    while (nukeIndex_ < n && lemmings_[nukeIndex_].removed && lemmings_[nukeIndex_].explosionTimer == 0)
        ++nukeIndex_;
    if (nukeIndex_ > lemmingsReleased_ - 1) {
        nukeInProgress_ = false;
        return;
    }
    Lemming& l = lemmings_[nukeIndex_];
    if (!l.removed && l.explosionTimer == 0 && l.action != Action::Splatting &&
        l.action != Action::Exploding)
        l.explosionTimer = 79;
    ++nukeIndex_;
}

void Game::updateObjects() {
    for (auto it = objects_.rbegin(); it != objects_.rend(); ++it) {
        ObjectState& o = *it;
        int count = int(o.info->frames.size());
        if (o.def.inactive) continue;
        if (o.triggered || o.info->flags == 2) ++o.frame;
        if (o.frame >= count) {
            o.frame = 0;
            o.triggered = false;
        }
    }
}

bool Game::updateExplosionTimer(Lemming& l) {
    if (--l.explosionTimer > 0) return false;
    switch (l.action) {
    case Action::Vaporizing: case Action::Drowning: case Action::Floating: case Action::Falling:
        transition(l, Action::Exploding);
        break;
    default:
        transition(l, Action::Ohnoing);
        break;
    }
    return true;
}

void Game::interactWithObjects(Lemming& l) {
    if (l.y >= 160) return;   // no object checks below the visible level
    l.objBelow = readMap(l.x, l.y);
    l.objInFront = readMap(l.x + 8 * l.dx, l.y - 8);
    uint8_t v = l.objBelow;
    if (v < 128) {
        ObjectState& o = objects_[v];
        if (!o.triggered) {
            o.triggered = true;
            removeLemming(l);
            cue(o.info->sound);
        }
        return;
    }
    switch (v) {
    case kMapExit:
        if (l.action != Action::Falling) transition(l, Action::Exiting);
        break;
    case kMapForceLeft:
        if (l.dx > 0) turnAround(l);
        break;
    case kMapForceRight:
        if (l.dx < 0) turnAround(l);
        break;
    case kMapWater:
        transition(l, Action::Drowning);
        cue(SfxDrown);
        break;
    case kMapFire:
        transition(l, Action::Vaporizing);
        cue(SfxFire);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Actions

bool Game::handle(Lemming& l) {
    if (l.action != Action::Floating && l.action != Action::Digging) {
        int maxFrame = int(l.anim->frames.size()) - 1;
        if (l.frame < maxFrame) {
            l.endOfAnim = false;
            ++l.frame;
        } else {
            l.endOfAnim = true;
            if (l.anim->loop) l.frame = 0;
        }
    }
    switch (l.action) {
    case Action::Walking: return handleWalking(l);
    case Action::Jumping: return handleJumping(l);
    case Action::Digging: return handleDigging(l);
    case Action::Climbing: return handleClimbing(l);
    case Action::Drowning: return handleDrowning(l);
    case Action::Hoisting: return handleHoisting(l);
    case Action::Building: return handleBuilding(l);
    case Action::Bashing: return handleBashing(l);
    case Action::Mining: return handleMining(l);
    case Action::Falling: return handleFalling(l);
    case Action::Floating: return handleFloating(l);
    case Action::Blocking: return handleBlocking(l);
    case Action::Ohnoing: return handleOhNoing(l);
    case Action::Exploding: return handleExploding(l);
    case Action::Shrugging:
        if (l.endOfAnim) { transition(l, Action::Walking); return true; }
        return false;
    case Action::Exiting:
        if (l.endOfAnim) { removeLemming(l); ++lemmingsIn_; }
        return false;
    case Action::Splatting:
    case Action::Vaporizing:
        if (l.endOfAnim) removeLemming(l);
        return false;
    default:
        return false;
    }
}

bool Game::handleWalking(Lemming& l) {
    l.x += l.dx;
    if (l.x < kMinX || l.x > kMaxX) {
        turnAround(l);
        return true;
    }
    if (hasPixelClipY(l.x, l.y, 0)) {
        // Step up, jump, climb or turn around.
        int dy = 0, ny = l.y;
        while (dy <= 6 && hasPixelClipY(l.x, ny - 1, -dy - 1)) { ++dy; --ny; }
        if (dy > 6) {
            if (l.climber) transition(l, Action::Climbing);
            else turnAround(l);
            return true;
        }
        if (dy >= 3) {
            transition(l, Action::Jumping);
            ny = l.y - 2;
        }
        l.y = ny;
        checkTopBoundary(l);
        return true;
    }
    // Step down or start falling.
    int dy = 1;
    while (dy <= 3) {
        ++l.y;
        if (hasPixelClipY(l.x, l.y, dy)) break;
        ++dy;
    }
    if (dy > 3) {
        ++l.y;
        transition(l, Action::Falling);
    }
    if (l.y > kMaxY) {
        removeLemming(l);
        return false;
    }
    return true;
}

bool Game::handleJumping(Lemming& l) {
    int dy = 0;
    while (dy < 2 && hasPixelClipY(l.x, l.y - 1, -dy - 1)) { ++dy; --l.y; }
    if (dy < 2) transition(l, Action::Walking);
    checkTopBoundary(l);
    return true;
}

bool Game::digRow(Lemming& l, int yy) {
    bool any = false;
    for (int x = l.x - 4; x < l.x + 5; ++x)
        if (hasPixel(x, yy)) { removePixel(x, yy); any = true; }
    return any;
}

bool Game::handleDigging(Lemming& l) {
    if (l.newDigger) {
        // The first row is clamped to the top of the level, the second follows it.
        int r = std::max(l.y - 2, 0);
        digRow(l, r);
        digRow(l, r + 1);
        l.newDigger = false;
    } else if (++l.frame >= 16) {
        l.frame -= 16;
    }
    if (l.frame != 0 && l.frame != 8) return false;
    int y = l.y;
    ++l.y;
    if (l.y > kMaxY) {
        removeLemming(l);
        return false;
    }
    if (!digRow(l, std::max(y, 0))) {
        transition(l, Action::Falling);
    } else if (readMap(l.x, l.y) == kMapSteel) {
        cue(SfxSteel);
        transition(l, Action::Walking);
    }
    return true;
}

bool Game::handleClimbing(Lemming& l) {
    if (l.frame <= 3) {
        if (!hasPixelClipY(l.x, l.y - 7 - l.frame, 0)) {
            l.y = l.y - l.frame + 2;
            transition(l, Action::Hoisting);
            checkTopBoundary(l);
        }
        return true;
    }
    --l.y;
    if (l.y - l.anim->footY < kHeadMinY || hasPixelClipY(l.x - l.dx, l.y - 8, -8)) {
        transition(l, Action::Falling, true);
        l.x += l.dx * 2;
    }
    return true;
}

bool Game::handleDrowning(Lemming& l) {
    if (l.endOfAnim) removeLemming(l);
    else if (!hasPixel(l.x + 8 * l.dx, l.y)) l.x += l.dx;
    return false;
}

bool Game::handleHoisting(Lemming& l) {
    if (l.frame <= 4) {
        l.y -= 2;
        checkTopBoundary(l);
        return true;
    }
    if (l.endOfAnim) {
        transition(l, Action::Walking);
        checkTopBoundary(l);
        return true;
    }
    return false;
}

void Game::layBrick(Lemming& l) {
    int x = l.dx == 1 ? l.x : l.x - 4;
    for (int i = 0; i < 6; ++i, ++x)
        if (x >= 0 && x < world_.w && l.y - 1 >= 0 && l.y - 1 < world_.h &&
            world_.at(x, l.y - 1) == kTransparent)
            world_.at(x, l.y - 1) = 8;
}

bool Game::handleBuilding(Lemming& l) {
    if (l.frame == 10 && l.bricksLeft <= 3) cue(SfxBuilderWarning);
    if (l.frame == 9 || (l.frame == 10 && l.bricksLeft == 9)) {
        layBrick(l);
        return false;
    }
    if (l.frame != 0) return true;

    l.x += l.dx;
    --l.y;
    if (l.x <= kMinX || l.x > kMaxX || hasPixelClipY(l.x, l.y - 1, -1)) {
        transition(l, Action::Walking, true);
        checkTopBoundary(l);
        return true;
    }
    l.x += l.dx;
    if (hasPixelClipY(l.x, l.y - 1, -1)) {
        transition(l, Action::Walking, true);
        checkTopBoundary(l);
        return true;
    }
    if (--l.bricksLeft == 0) {
        transition(l, Action::Shrugging);
        checkTopBoundary(l);
        return true;
    }
    if (hasPixelClipY(l.x + 2 * l.dx, l.y - 9, -9) || l.x <= kMinX || l.x > kMaxX) {
        transition(l, Action::Walking, true);
        checkTopBoundary(l);
        return true;
    }
    // Too high: becomes a walker without turning around.
    if (l.y - l.anim->footY < kHeadMinY) {
        transition(l, Action::Walking);
        checkTopBoundary(l);
    }
    return true;
}

bool Game::handleBashing(Lemming& l) {
    int index = l.frame >= 16 ? l.frame - 16 : l.frame;
    if (index >= 11 && index <= 15) {
        l.x += l.dx;
        if (l.x < kMinX || l.x > kMaxX) {
            transition(l, Action::Walking, true);
            return true;
        }
        int dy = 0;
        while (dy < 3 && !hasPixelClipY(l.x, l.y, dy)) { ++dy; ++l.y; }
        if (dy == 3) {
            transition(l, Action::Falling);
            return true;
        }
        uint8_t front = readMap(l.x + l.dx * 8, l.y - 8);
        if (front == kMapSteel) cue(SfxSteel);
        if (front == kMapSteel || (front == kMapOneWayLeft && l.dx != -1) ||
            (front == kMapOneWayRight && l.dx != 1))
            transition(l, Action::Walking, true);
        return true;
    }
    if (index >= 2 && index <= 5) {
        const Bitmap& mask = md_.masks[size_t(l.dx < 0 ? MaskId::BashL : MaskId::BashR)][index - 2];
        applyMask(mask, l.x - l.anim->footX, l.y - l.anim->footY);
        // On the first stroke, stop if there is nothing left to bash.
        if (l.frame == 5) {
            int n = 0, x = l.x + l.dx * 8, y = l.y - 6;
            while (n < 4 && !hasPixel(x, y)) { ++n; x += l.dx; }
            if (n == 4) transition(l, Action::Walking);
        }
    }
    return false;
}

bool Game::handleMining(Lemming& l) {
    const auto& masks = md_.masks[size_t(l.dx < 0 ? MaskId::MineL : MaskId::MineR)];
    if (l.frame == 1) {
        applyMask(masks[0], l.x - l.anim->footX, l.y - l.anim->footY);
        return false;
    }
    if (l.frame == 2) {
        applyMask(masks[1], l.x + l.dx - l.anim->footX, l.y + 1 - l.anim->footY);
        return false;
    }
    if (l.frame == 3 || l.frame == 15) {
        for (int step = 0; step < 2; ++step) {
            l.x += l.dx;
            if (l.x < kMinX || l.x > kMaxX) {
                transition(l, Action::Walking, true);
                return true;
            }
        }
        if (l.frame == 3 && ++l.y > kMaxY) {
            removeLemming(l);
            return false;
        }
        if (!hasPixelClipY(l.x, l.y, 0)) {
            transition(l, Action::Falling);
            return true;
        }
        uint8_t below = readMap(l.x, l.y);
        if (below == kMapSteel) cue(SfxSteel);
        // A right-pointing one-way wall stops miners in both directions,
        // exactly as in the original.
        if (below == kMapSteel || (below == kMapOneWayLeft && l.dx != -1) ||
            below == kMapOneWayRight)
            transition(l, Action::Walking, true);
        return true;
    }
    if (l.frame == 0) {
        if (++l.y > kMaxY) {
            removeLemming(l);
            return false;
        }
        return true;
    }
    return false;
}

bool Game::handleFalling(Lemming& l) {
    if (l.fallen > 16 && l.floater) {
        transition(l, Action::Floating);
        return true;
    }
    int dy = 0;
    while (dy < 3 && !hasPixelClipY(l.x, l.y, dy)) {
        ++dy;
        if (++l.y > kMaxY) {
            removeLemming(l);
            return false;
        }
    }
    if (dy == 3) {
        l.fallen += 3;
        return true;
    }
    // Returning true lets the exit check run on the same frame, which is how
    // a lemming can drop straight into an exit.
    transition(l, l.fallen > kMaxFall ? Action::Splatting : Action::Walking);
    return true;
}

bool Game::handleFloating(Lemming& l) {
    const FloatStep& st = kFloat[l.floatIndex];
    l.frame = st.frame;
    int dy = st.dy;
    if (++l.floatIndex >= 16) l.floatIndex = 8;
    if (dy <= 0) {
        l.y += dy;
    } else {
        for (int minY = 0; dy > 0; --dy, ++minY) {
            if (hasPixelClipY(l.x, l.y, minY)) {
                transition(l, Action::Walking);
                return true;
            }
            ++l.y;
        }
    }
    if (l.y > kMaxY) {
        removeLemming(l);
        return false;
    }
    return true;
}

bool Game::handleBlocking(Lemming& l) {
    if (!hasPixelClipY(l.x, l.y, 0)) {
        transition(l, Action::Walking);
        l.blocking = false;
        restoreMap(l);
    }
    return false;
}

bool Game::handleOhNoing(Lemming& l) {
    if (l.endOfAnim) {
        transition(l, Action::Exploding);
        return false;
    }
    int dy = 0;
    while (dy < 3 && !hasPixelClipY(l.x, l.y, dy)) { ++dy; ++l.y; }
    if (l.y > kMaxY) {
        removeLemming(l);
        return false;
    }
    return true;
}

bool Game::handleExploding(Lemming& l) {
    if (!l.endOfAnim) return false;
    if (l.exploded) {
        // The lemming stays in the game (counted as out, visible to the
        // cursor) while its debris flies, and is removed after 52 frames.
        ++l.particleFrame;
        if (++l.explodeAge >= kExplodeFrames) removeLemming(l);
        return false;
    }
    if (l.blocking) {
        l.blocking = false;
        restoreMap(l);
    }
    uint8_t m = readMap(l.x, l.y);
    if (m != kMapSteel && m != kMapWater)
        applyMask(md_.masks[size_t(MaskId::Explosion)][0], l.x - 8, l.y - 14);
    l.exploded = true;
    l.explodeAge = 1;
    l.particleFrame = 0;
    return false;
}

// ---------------------------------------------------------------------------
// Input

void Game::setCursor(int x, int y, bool rightButton) {
    cursorX_ = x;
    cursorY_ = y;
    rightButton_ = rightButton;
    int a, b;
    hitTest(a, b, hoverCount_);
    hovered_ = a;
}

void Game::setReleaseRateChange(int dir) { rrChange_ = dir; }

void Game::nuke() {
    if (nuking_) return;
    nuking_ = true;
    // As in the original, only released lemmings count towards the result.
    maxLemmings_ = lemmingsReleased_;
    nukeInProgress_ = true;
}

void Game::hitTest(int& first, int& second, int& count) {
    int prio = -1, nonPrio = -1;
    count = 0;
    for (size_t i = 0; i < lemmings_.size(); ++i) {
        const Lemming& l = lemmings_[i];
        if (l.removed) continue;
        int x = std::max(l.x - l.anim->footX, 0), y = std::max(l.y - l.anim->footY, 0);
        int bottom = std::min(y + 12, 160);
        if (x <= cursorX_ && cursorX_ <= x + 12 && y <= cursorY_ && cursorY_ <= bottom) {
            ++count;
            if (isPriority(l.action) || (l.keptBits & kPriorityBits)) prio = int(i);
            else nonPrio = int(i);
        }
    }
    first = (rightButton_ || prio < 0) ? nonPrio : prio;
    second = nonPrio;
    // The original remembers the last chosen lemming; with the right button
    // held and only busy lemmings under the cursor it falls back to it.
    if (first >= 0) lastPrimary_ = first;
}

bool Game::assignSkill(Skill s) {
    int a, b, n;
    hitTest(a, b, n);
    if (n == 0) return false;
    if (a < 0) a = lastPrimary_;
    if (a < 0) return false;
    Lemming* l1 = &lemmings_[a];
    Lemming* l2 = b >= 0 ? &lemmings_[b] : nullptr;
    bool ok = assignTo(l1, l2, s);
    if (ok) cue(SfxAssign);
    return ok;
}

bool Game::assignTo(Lemming* l1, Lemming* l2, Skill s) {
    auto inSet = [](Action a, std::initializer_list<Action> set) {
        return std::find(set.begin(), set.end(), a) != set.end();
    };
    auto pick = [&](std::initializer_list<Action> set) -> Lemming* {
        if (inSet(l1->action, set)) return l1;
        if (l2 && inSet(l2->action, set)) return l2;
        return nullptr;
    };
    if (skills_[s] <= 0) return false;
    switch (s) {
    case Climber:
        if (l1->climber || (l1->keptBits & kNoClimbFloatBits) ||
            inSet(l1->action, {Action::Blocking, Action::Splatting, Action::Exploding}))
            return false;
        l1->climber = true;
        // Original quirk: a shrugging builder is switched straight back to walking.
        if (l1->action == Action::Shrugging) l1->action = Action::Walking;
        break;
    case Floater:
        if (l1->floater || (l1->keptBits & kNoClimbFloatBits) ||
            inSet(l1->action, {Action::Blocking, Action::Splatting, Action::Exploding}))
            return false;
        l1->floater = true;
        break;
    case Bomber:
        if (l1->explosionTimer != 0 ||
            inSet(l1->action, {Action::Ohnoing, Action::Exploding, Action::Vaporizing, Action::Splatting}))
            return false;
        l1->explosionTimer = 79;
        break;
    case Blocker:
        if (!inSet(l1->action, {Action::Walking, Action::Shrugging, Action::Building, Action::Bashing,
                                Action::Mining, Action::Digging}) ||
            overlapsBlockerField(*l1))
            return false;
        transition(*l1, Action::Blocking);
        break;
    case Builder: {
        if (l1->y - l1->anim->footY < kHeadMinY) return false;
        Lemming* t = pick({Action::Walking, Action::Shrugging, Action::Bashing, Action::Mining,
                           Action::Digging});
        if (!t) return false;
        transition(*t, Action::Building);
        break;
    }
    case Basher: {
        Lemming* t = pick({Action::Walking, Action::Shrugging, Action::Building, Action::Mining,
                           Action::Digging});
        if (!t) return false;
        if (t->objInFront == kMapSteel) { cue(SfxBuilderWarning); return false; }
        if ((t->objInFront == kMapOneWayLeft && t->dx != -1) ||
            (t->objInFront == kMapOneWayRight && t->dx != 1))
            return false;
        transition(*t, Action::Bashing);
        break;
    }
    case Miner: {
        Lemming* t = pick({Action::Walking, Action::Shrugging, Action::Building, Action::Bashing,
                           Action::Digging});
        if (!t) return false;
        if (t->objBelow == kMapSteel) return false;
        if (t->objInFront == kMapSteel) { cue(SfxBuilderWarning); return false; }
        if ((t->objInFront == kMapOneWayLeft && t->dx != -1) ||
            (t->objInFront == kMapOneWayRight && t->dx != 1))
            return false;
        transition(*t, Action::Mining);
        break;
    }
    case Digger: {
        if (l1->objBelow == kMapSteel) return false;
        Lemming* t = pick({Action::Walking, Action::Shrugging, Action::Building, Action::Bashing,
                           Action::Mining});
        if (!t) return false;
        transition(*t, Action::Digging);
        break;
    }
    default:
        return false;
    }
    --skills_[s];
    return true;
}

// ---------------------------------------------------------------------------
// Rendering

const std::array<std::array<std::array<int8_t, 2>, 80>, 52>& Game::particles() {
    // Debris thrown up and outwards, then pulled down by gravity. Generated
    // once from a fixed seed so every explosion looks the same.
    static const auto table = [] {
        std::array<std::array<std::array<int8_t, 2>, 80>, 52> t{};
        uint32_t seed = 0x1234567u;
        auto rnd = [&](int lo, int hi) {
            seed = seed * 1103515245u + 12345u;
            return lo + int((seed >> 16) % uint32_t(hi - lo + 1));
        };
        for (int p = 0; p < 80; ++p) {
            int x = 0, y = -8 * 16;                        // 1/16 pixel units
            int vx = rnd(-40, 40), vy = rnd(-72, -8);
            bool gone = false;
            for (int f = 0; f < 52; ++f) {
                x += vx;
                y += vy;
                vy += 4;
                int px = x / 16, py = y / 16;
                if (gone || px < -120 || px > 120 || py < -120 || py > 120) {
                    gone = true;
                    t[f][p] = {-128, -128};
                } else {
                    t[f][p] = {int8_t(px), int8_t(py)};
                }
            }
        }
        return t;
    }();
    return table;
}

void Game::render(Bitmap& out) const {
    out = Bitmap(world_.w, world_.h, 0);
    for (size_t i = 0; i < out.px.size(); ++i)
        if (world_.px[i] != kTransparent) out.px[i] = world_.px[i];

    // Objects restricted to terrain first, then all others.
    for (int pass = 0; pass < 2; ++pass)
        for (const ObjectState& o : objects_) {
            if (o.def.inactive || o.def.onlyOnTerrain != (pass == 0)) continue;
            if (o.frame < 0 || o.frame >= int(o.info->frames.size())) continue;
            blitObject(out, world_, o.info->frames[o.frame], o.def.x, o.def.y, o.def.noOverwrite,
                       o.def.onlyOnTerrain, o.def.upsideDown);
        }

    auto plot = [&](const Bitmap& b, int x0, int y0, uint8_t color = 0) {
        for (int y = 0; y < b.h; ++y)
            for (int x = 0; x < b.w; ++x) {
                uint8_t c = b.at(x, y);
                int dx = x0 + x, dy = y0 + y;
                if (c == kTransparent || dx < 0 || dy < 0 || dx >= out.w || dy >= out.h) continue;
                out.at(dx, dy) = color ? color : c;
            }
    };

    for (const Lemming& l : lemmings_) {
        if (l.removed) continue;
        if (l.exploded) {
            if (l.particleFrame >= kExplodeFrames) continue;
            const auto& pf = particles()[l.particleFrame];
            for (int i = 0; i < 80; ++i) {
                if (pf[i][0] == -128 && pf[i][1] == -128) continue;
                int x = l.x + pf[i][0], y = l.y + pf[i][1];
                if (x >= 0 && y >= 0 && x < out.w && y < out.h) out.at(x, y) = kParticleColors[i % 16];
            }
            continue;
        }
        {
            int f = std::clamp(l.frame, 0, int(l.anim->frames.size()) - 1);
            plot(*l.anim->frames[f], l.x - l.anim->footX, l.y - l.anim->footY);
            if (l.explosionTimer > 0) {
                int digit = (l.explosionTimer >> 4) + 1;
                plot(md_.countdown[digit], l.x - 1, l.y - l.anim->footY - 12, 3);
            }
        }
    }
}

} // namespace lem
