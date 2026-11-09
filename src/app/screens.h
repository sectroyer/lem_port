// SPDX-License-Identifier: GPL-3.0-or-later
// Game screens: level preview, playing, result.
#pragma once

#include <SDL3/SDL_events.h>

#include <cstdint>
#include <memory>
#include <string>

#include "game.h"
#include "graphicset.h"
#include "levelpack.h"
#include "levelrender.h"
#include "maindat.h"
#include "password.h"
#include "audio.h"

namespace lem {

// Output canvas. The play screen (320x200) is doubled; menu screens use the
// original 640x350 layout, centred vertically.
constexpr int kScreenW = 640;
constexpr int kScreenH = 400;

class Frontend {
public:
    explicit Frontend(const std::string& dataDir, int rating = 0, int level = 0);
    ~Frontend();
    // Returns false when the user wants to quit.
    bool handleEvent(const SDL_Event& ev);
    void update();                         // one 60 ms frame
    void draw(uint32_t* pixels, int pitch);// pitch in pixels
    void startLevel() { startPlaying(); }

    int rating() const { return rating_; }
    int levelNumber() const { return level_; }

private:
    enum class Mode { Preview, Playing, Result };

    void loadLevel(int rating, int level);
    void startPlaying();
    void finishLevel();

    // Play screen helpers (320x200 coordinates).
    void drawPlay();
    void drawPanel();
    void drawPanelText(int col, const std::string& s);
    void playClick(int x, int y, bool right, bool dbl);
    void selectSkill(int button);
    void pressNuke();
    // Game coordinates of the cursor: the original's hot spot sits 3 pixels
    // left of and 2 below the crosshair centre.
    int cursorGameX() const { return scrollX_ + mouseX_ / 2 - 3; }
    int cursorGameY() const { return mouseY_ / 2 + 2; }

    // Menu-style screens (640x350 coordinates).
    void drawMenuBackground();
    void drawMenuText(int x, int y, const std::string& s);
    void drawMenuTextCentered(int y, const std::string& s);
    void drawPreview();
    void drawResult();

    std::string dataDir_;
    LevelPack pack_;
    MainData md_;
    AnimSet anims_;
    std::unique_ptr<GraphicSet> gs_;
    std::unique_ptr<SpecialSet> spec_;
    std::unique_ptr<Game> game_;
    Level levelDef_;
    Palette pal_{};
    Palette menuPal_{};

    Mode mode_ = Mode::Preview;
    int rating_ = 0, level_ = 0;

    // Play state.
    int scrollX_ = 0;
    int scrollDir_ = 0;
    int selected_ = Climber;      // selected skill
    bool paused_ = false;
    bool fast_ = false;
    int rrHeld_ = 0;
    int mouseX_ = 0, mouseY_ = 0; // canvas coordinates
    bool rightDown_ = false;
    int frameCounter_ = 0;
    int musicCounter_ = 0;          // advances through the music list per new level
    Audio audio_;
    bool audioOk_ = false;
    int lastNukeFrame_ = -100;   // the nuke needs two presses within 5 frames
    bool rrFromKeyboard_ = false;
    GameResult result_{};
    int codeFlags_ = 0;           // carried from the last entered code
    bool codeEntry_ = false;
    std::string codeInput_;
    std::string codeMessage_;

    Bitmap levelFrame_;           // 1600x160 rendered level
    Bitmap play_;                 // 320x200 play screen
    Bitmap menu_;                 // 640x350 menu screen
};

} // namespace lem
