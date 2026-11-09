// SPDX-License-Identifier: GPL-3.0-or-later
// Deterministic level simulation. One call to tick() advances the game by one
// frame of the original (60 ms). See docs/mechanics.md.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "bitmap.h"
#include "graphicset.h"
#include "level.h"
#include "maindat.h"

namespace lem {

enum class Action : uint8_t {
    Walking, Jumping, Digging, Climbing, Drowning, Hoisting, Building, Bashing,
    Mining, Falling, Floating, Splatting, Exiting, Vaporizing, Blocking,
    Shrugging, Ohnoing, Exploding,
    Count
};

const char* actionName(Action a);

// Sound effect identifiers (same numbering as the trap sound field).
enum Sfx : int {
    SfxNone = 0, SfxSkillSelect = 1, SfxEntrance = 2, SfxLetsGo = 3, SfxAssign = 4,
    SfxOhNo = 5, SfxElectric = 6, SfxCrush = 7, SfxSplat = 8, SfxRope = 9,
    SfxSteel = 0x0A, SfxUnknown = 0x0B, SfxExplode = 0x0C, SfxFire = 0x0D,
    SfxTenTon = 0x0E, SfxBearTrap = 0x0F, SfxYippee = 0x10, SfxDrown = 0x11,
    SfxBuilderWarning = 0x12,
};

struct LemmingAnim {
    std::vector<const Bitmap*> frames;
    int footX = 8, footY = 10;
    bool loop = true;
};

// Lemming animations indexed by action and direction.
class AnimSet {
public:
    explicit AnimSet(const MainData& m);
    const LemmingAnim& get(Action a, bool leftward) const {
        return anims_[size_t(a)][leftward ? 1 : 0];
    }
private:
    std::array<std::array<LemmingAnim, 2>, size_t(Action::Count)> anims_;
};

struct Lemming {
    int x = 0, y = 0;         // foot position
    int dx = 1;               // +1 right, -1 left
    Action action = Action::Falling;
    int frame = 0;
    bool endOfAnim = false;
    int fallen = 0;
    int explosionTimer = 0;   // 79..1 while counting down, 0 = none
    int floatIndex = 0;
    int bricksLeft = 0;
    bool climber = false, floater = false;
    bool blocking = false;
    bool newDigger = false;
    bool removed = false;
    bool exploded = false;
    int explodeAge = 0;       // ticks since the explosion (record stays until 52)
    int particleFrame = 0;
    uint16_t keptBits = 0;    // action bits the original keeps after some transitions
    uint8_t objBelow = 0, objInFront = 0;
    std::array<uint8_t, 9> savedMap{};
    const LemmingAnim* anim = nullptr;
};

struct ObjectState {
    LevelObject def;
    const ObjectInfo* info = nullptr;
    int frame = 0;
    bool triggered = false;
};

// One recorded player input. `frame` is the number of frames played before it.
struct ReplayEvent {
    int frame = 0;
    char type = 0;   // 'C' cursor (a=x, b=y, c=right button), 'A' assign (a=skill),
                     // 'R' release rate change (a=dir), 'N' nuke, 'P' paused frame
    int a = 0, b = 0, c = 0;
};

struct Replay {
    int rating = 0, number = 0;   // level in game order
    int frames = 0;               // total frames played
    std::vector<ReplayEvent> events;

    std::string save() const;
    static bool parse(const std::string& text, Replay& out);
};

struct GameResult {
    int total = 0;       // lemmings counted for the percentage
    int rescued = 0;
    int needed = 0;      // percentage required
    int achieved = 0;    // percentage reached
    bool success = false;
    bool timeUp = false;
};

class Game {
public:
    static constexpr int kTicksPerSecond = 17;   // game clock ticks per displayed second
    // One game frame = 5 interrupts of the PC timer programmed with divisor 0x4000.
    static constexpr int kFrameMicros = int(5.0 * 16384.0 * 1000000.0 / 1193182.0 + 0.5);

    Game(const Level& lvl, const GraphicSet& gs, const SpecialSet* spec, const AnimSet& anims,
         const MainData& md);

    // Advances the simulation by one frame.
    void tick();

    // Player input (apply between ticks).
    void setCursor(int x, int y, bool rightButton = false);
    bool assignSkill(Skill s);          // at the current cursor
    void setReleaseRateChange(int dir); // +1 faster, -1 slower, 0 none (button held)
    void nuke();
    // Called once per frame while the game is paused: only the opening sequence
    // and (keyboard) release rate changes continue, as in the original.
    void tickPaused();

    // Every input is recorded; `replay` drives a fresh game through it.
    Replay recording(int rating, int number) const;
    void play(const Replay& replay);
    int frameCount() const { return frameCount_; }

    // State.
    bool finished() const { return finished_; }
    GameResult result() const;
    int iteration() const { return iteration_; }
    int minutes() const { return minutes_; }
    int seconds() const { return seconds_; }
    int releaseRate() const { return releaseRate_; }
    int minReleaseRate() const { return level_.releaseRate; }
    int skillCount(Skill s) const { return skills_[s]; }
    int lemmingsOut() const { return lemmingsOut_; }
    int lemmingsIn() const { return lemmingsIn_; }
    int lemmingsTotal() const { return maxLemmings_; }
    bool nuking() const { return nuking_; }
    const Level& level() const { return level_; }
    const Bitmap& terrain() const { return world_; }
    const std::vector<Lemming>& lemmings() const { return lemmings_; }
    const std::vector<ObjectState>& objects() const { return objects_; }
    // Lemming under the cursor (prioritised as for assignment), -1 if none.
    int hoveredLemming() const { return hovered_; }
    int hoverCount() const { return hoverCount_; }

    // Sound effects triggered since the last call.
    std::vector<int> takeSounds();

    // Renders terrain, objects, lemmings and particles into a 1600x160 frame.
    void render(Bitmap& out) const;

    // Particle offsets for frame f (0..51) of an explosion; (-128,-128) = gone.
    static const std::array<std::array<std::array<int8_t, 2>, 80>, 52>& particles();

private:
    // Object map (4x4 pixel cells).
    uint8_t readMap(int x, int y) const;
    void writeMap(int x, int y, uint8_t v);
    void initObjectMap();
    void saveMap(Lemming& l);
    void restoreMap(Lemming& l);
    void setBlockerField(Lemming& l);
    bool overlapsBlockerField(const Lemming& l) const;

    bool hasPixel(int x, int y) const;
    bool hasPixelClipY(int x, int y, int minY) const;
    void removePixel(int x, int y);
    void applyMask(const Bitmap& mask, int x, int y);

    void setAnim(Lemming& l);
    void transition(Lemming& l, Action a, bool turn = false);
    void turnAround(Lemming& l);
    bool checkTopBoundary(Lemming& l);
    void removeLemming(Lemming& l);
    void cue(int sfx);

    void updateReleaseRate();
    void advanceIntro();
    void releaseLemming();
    void updateLemmings();
    void updateNuke();
    void updateObjects();
    void checkFinished();
    void hitTest(int& first, int& second, int& count);
    void interactWithObjects(Lemming& l);
    bool updateExplosionTimer(Lemming& l);
    void layBrick(Lemming& l);
    bool digRow(Lemming& l, int y);

    bool handle(Lemming& l);
    bool handleWalking(Lemming& l);
    bool handleJumping(Lemming& l);
    bool handleDigging(Lemming& l);
    bool handleClimbing(Lemming& l);
    bool handleDrowning(Lemming& l);
    bool handleHoisting(Lemming& l);
    bool handleBuilding(Lemming& l);
    bool handleBashing(Lemming& l);
    bool handleMining(Lemming& l);
    bool handleFalling(Lemming& l);
    bool handleFloating(Lemming& l);
    bool handleBlocking(Lemming& l);
    bool handleOhNoing(Lemming& l);
    bool handleExploding(Lemming& l);
    bool handleEndOfAnim(Lemming& l);

    bool assignTo(Lemming* l1, Lemming* l2, Skill s);

    Level level_;
    const GraphicSet& gs_;
    const AnimSet& anims_;
    const MainData& md_;
    Bitmap world_;
    std::vector<uint8_t> objMap_;
    std::vector<ObjectState> objects_;
    std::array<int, 4> entryTable_{-1, -1, -1, -1};
    std::vector<Lemming> lemmings_;
    std::vector<int> sounds_;

    std::array<int, SkillCount> skills_{};
    int releaseRate_ = 0;
    int rrChange_ = 0;
    int maxLemmings_ = 0;
    int lemmingsReleased_ = 0, lemmingsOut_ = 0, lemmingsIn_ = 0, lemmingsRemoved_ = 0;
    int nextRelease_ = 20;
    int iteration_ = 0;
    int clockFrame_ = 0;
    int minutes_ = 0, seconds_ = 0;
    bool entriesOpen_ = false;
    bool nuking_ = false, nukeInProgress_ = false;
    int nukeIndex_ = 0;
    int rrPhase_ = 0;
    int timeUpFrames_ = -1;       // frames still simulated after the clock hit 0:00
    bool finished_ = false, timeUp_ = false;

    int cursorX_ = -1000, cursorY_ = -1000;
    bool rightButton_ = false;
    int hovered_ = -1, hoverCount_ = 0;
    int lastPrimary_ = -1;

    std::vector<ReplayEvent> events_;
    int frameCount_ = 0;
    void record(char type, int a = 0, int b = 0, int c = 0);
};

} // namespace lem
