// SPDX-License-Identifier: GPL-3.0-or-later
#include "screens.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace lem {

namespace {

constexpr int kPlayW = 320, kPlayH = 200, kViewH = 160;
constexpr int kMenuW = 640, kMenuH = 350, kMenuY = (kScreenH - kMenuH) / 2;
constexpr int kScrollStep = 8;
constexpr uint8_t kWhite = 3;

// Panel buttons, left to right.
enum Button { BtnSlower, BtnFaster, BtnSkill0, BtnPause = BtnSkill0 + SkillCount, BtnNuke, BtnCount };

uint32_t rgb(const Rgb& c) { return uint32_t(c.r) << 16 | uint32_t(c.g) << 8 | c.b; }

std::string upper(std::string s) {
    for (char& c : s) c = char(std::toupper(uint8_t(c)));
    return s;
}

} // namespace

Frontend::Frontend(const std::string& dataDir, int rating, int level)
    : dataDir_(dataDir), pack_(dataDir), md_(loadMainData(dataDir)), anims_(md_),
      play_(kPlayW, kPlayH, 0), menu_(kMenuW, kMenuH, 0) {
    menuPal_ = menuPalette();
    audioOk_ = audio_.open(dataDir);
    loadLevel(rating, level);
}

Frontend::~Frontend() { audio_.stop(); }

// ---------------------------------------------------------------------------
// Flow

void Frontend::loadLevel(int rating, int level) {
    rating_ = std::clamp(rating, 0, kRatings - 1);
    level_ = std::clamp(level, 0, kLevelsPerRating - 1);
    levelDef_ = pack_.level(rating_, level_);
    gs_ = std::make_unique<GraphicSet>(loadGraphicSet(dataDir_, levelDef_.graphicSet));
    spec_.reset();
    if (levelDef_.specialSet)
        spec_ = std::make_unique<SpecialSet>(loadSpecialSet(dataDir_, levelDef_.specialSet - 1));
    pal_ = levelPalette(spec_ ? spec_->vga : gs_->vgaCustom);
    game_.reset();
    mode_ = Mode::Preview;
}

void Frontend::startPlaying() {
    game_ = std::make_unique<Game>(levelDef_, *gs_, spec_.get(), anims_, md_);
    scrollX_ = std::clamp(levelDef_.startX, 0, kLevelWidth - kPlayW);
    selected_ = Climber;
    paused_ = fast_ = false;
    rrHeld_ = 0;
    mode_ = Mode::Playing;
    // Special levels have their own tune; the others cycle through a list.
    static const int kSpecialMusic[4] = {1, 9, 3, 2};
    static const int kLevelMusic[17] = {6, 7, 8, 10, 11, 4, 12, 13, 14, 15, 5, 16, 17, 18, 19, 20, 21};
    int track = levelDef_.specialSet >= 1 && levelDef_.specialSet <= 4
                    ? kSpecialMusic[levelDef_.specialSet - 1]
                    : kLevelMusic[musicCounter_ % 17];
    audio_.music(track);
}

void Frontend::finishLevel() {
    result_ = game_->result();
    mode_ = Mode::Result;
    audio_.stop();
}

void Frontend::update() {
    ++frameCounter_;
    if (mode_ != Mode::Playing) return;
    if (scrollDir_ == 0 && mouseY_ / 2 < kViewH) {
        if (mouseX_ <= 1) scrollX_ -= kScrollStep;
        else if (mouseX_ >= kScreenW - 2) scrollX_ += kScrollStep;
    }
    scrollX_ = std::clamp(scrollX_ + scrollDir_ * kScrollStep, 0, kLevelWidth - kPlayW);
    if (paused_) {
        // While paused only the opening sequence runs on, and the release
        // rate can still be changed from the keyboard.
        game_->setReleaseRateChange(rrFromKeyboard_ ? rrHeld_ : 0);
        game_->tickPaused();
        for (int id : game_->takeSounds()) audio_.sfx(id);
        return;
    }
    game_->setReleaseRateChange(rrHeld_);
    for (int i = 0, n = fast_ ? 4 : 1; i < n && !game_->finished(); ++i) {
        game_->setCursor(cursorGameX(), cursorGameY(), rightDown_);
        game_->tick();
    }
    for (int id : game_->takeSounds()) audio_.sfx(id);
    if (game_->finished()) finishLevel();
}

// ---------------------------------------------------------------------------
// Input

bool Frontend::handleEvent(const SDL_Event& ev) {
    switch (ev.type) {
    case SDL_EVENT_MOUSE_MOTION:
        mouseX_ = std::clamp(int(ev.motion.x), 0, kScreenW - 1);
        mouseY_ = std::clamp(int(ev.motion.y), 0, kScreenH - 1);
        if (game_) game_->setCursor(cursorGameX(), cursorGameY(), rightDown_);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        bool right = ev.button.button == SDL_BUTTON_RIGHT;
        if (right) rightDown_ = true;
        mouseX_ = std::clamp(int(ev.button.x), 0, kScreenW - 1);
        mouseY_ = std::clamp(int(ev.button.y), 0, kScreenH - 1);
        if (mode_ == Mode::Preview && !codeEntry_) { codeMessage_.clear(); startPlaying(); }
        else if (mode_ == Mode::Result) {
            if (result_.success) {
                ++musicCounter_;
                int n = level_ + 1, r = rating_;
                if (n >= kLevelsPerRating) { n = 0; r = std::min(r + 1, kRatings - 1); }
                loadLevel(r, n);
            } else {
                loadLevel(rating_, level_);
            }
        } else {
            playClick(mouseX_ / 2, mouseY_ / 2, right, ev.button.clicks >= 2);
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev.button.button == SDL_BUTTON_RIGHT) rightDown_ = false;
        rrHeld_ = 0;
        break;
    case SDL_EVENT_KEY_DOWN: {
        SDL_Keycode k = ev.key.key;
        if (mode_ == Mode::Preview && codeEntry_) {
            if (k == SDLK_ESCAPE) { codeEntry_ = false; codeMessage_.clear(); }
            else if (k == SDLK_BACKSPACE && !codeInput_.empty()) codeInput_.pop_back();
            else if (k >= SDLK_A && k <= SDLK_Z && codeInput_.size() < 10) codeInput_ += char('A' + (k - SDLK_A));
            else if (k == SDLK_RETURN && codeInput_.size() == 10) {
                auto c = decodeAccessCode(codeInput_);
                if (c && c->level < kRatings * kLevelsPerRating) {
                    codeFlags_ = c->flags;
                    codeEntry_ = false;
                    loadLevel(c->level / kLevelsPerRating, c->level % kLevelsPerRating);
                    codeMessage_ = "Code for Level " + std::to_string(level_ + 1) + " " + upper(ratingName(rating_));
                } else {
                    codeMessage_ = "Incorrect Code";
                    codeInput_.clear();
                }
            }
            break;
        }
        if (mode_ == Mode::Preview) {
            if (k == SDLK_ESCAPE) return false;
            if (k == SDLK_C || k == SDLK_TAB) {
                codeEntry_ = true;
                codeInput_.clear();
                codeMessage_.clear();
                break;
            }
            if (k == SDLK_RIGHT || k == SDLK_DOWN) {
                int n = level_ + 1, r = rating_;
                if (n >= kLevelsPerRating) { n = 0; r = (r + 1) % kRatings; }
                loadLevel(r, n);
            } else if (k == SDLK_LEFT || k == SDLK_UP) {
                int n = level_ - 1, r = rating_;
                if (n < 0) { n = kLevelsPerRating - 1; r = (r + kRatings - 1) % kRatings; }
                loadLevel(r, n);
            } else if (k == SDLK_PAGEDOWN) {
                loadLevel((rating_ + 1) % kRatings, 0);
            } else if (k == SDLK_PAGEUP) {
                loadLevel((rating_ + kRatings - 1) % kRatings, 0);
            } else if (k == SDLK_RETURN || k == SDLK_SPACE) {
                startPlaying();
            }
            break;
        }
        if (mode_ == Mode::Result) {
            if (k == SDLK_ESCAPE) loadLevel(rating_, level_);
            break;
        }
        if (ev.key.repeat && k != SDLK_LEFT && k != SDLK_RIGHT) break;
        if (k >= SDLK_F3 && k <= SDLK_F10) selectSkill(BtnSkill0 + int(k - SDLK_F3));
        else if (k >= SDLK_1 && k <= SDLK_8) selectSkill(BtnSkill0 + int(k - SDLK_1));
        else if (k == SDLK_F1 || k == SDLK_MINUS) { rrHeld_ = -1; rrFromKeyboard_ = true; }
        else if (k == SDLK_F2 || k == SDLK_EQUALS || k == SDLK_PLUS) { rrHeld_ = 1; rrFromKeyboard_ = true; }
        else if (k == SDLK_F11 || k == SDLK_P || k == SDLK_PAUSE) paused_ = !paused_;
        else if (k == SDLK_F12) pressNuke();
        else if (k == SDLK_F) fast_ = !fast_;
        else if (k == SDLK_M) {
            audio_.musicOn = !audio_.musicOn;
            if (!audio_.musicOn) audio_.stop();
        }
        else if (k == SDLK_X) audio_.sfxOn = !audio_.sfxOn;
        else if (k == SDLK_LEFT) scrollDir_ = -1;
        else if (k == SDLK_RIGHT) scrollDir_ = 1;
        else if (k == SDLK_ESCAPE) finishLevel();
        break;
    }
    case SDL_EVENT_KEY_UP: {
        SDL_Keycode k = ev.key.key;
        if (k == SDLK_F1 || k == SDLK_F2 || k == SDLK_MINUS || k == SDLK_EQUALS || k == SDLK_PLUS)
            rrHeld_ = 0;
        if ((k == SDLK_LEFT && scrollDir_ < 0) || (k == SDLK_RIGHT && scrollDir_ > 0)) scrollDir_ = 0;
        break;
    }
    default:
        break;
    }
    return true;
}

void Frontend::selectSkill(int button) {
    if (button >= BtnSkill0 && button < BtnSkill0 + SkillCount) {
        if (selected_ != button - BtnSkill0) audio_.sfx(SfxSkillSelect);
        selected_ = button - BtnSkill0;
    }
}

void Frontend::pressNuke() {
    if (paused_) return;
    if (frameCounter_ - lastNukeFrame_ <= 5) {
        game_->nuke();
        lastNukeFrame_ = -100;
    } else {
        lastNukeFrame_ = frameCounter_;
    }
}

void Frontend::playClick(int x, int y, bool right, bool dbl) {
    if (y < kViewH) {
        if (paused_ || right) return;
        game_->setCursor(cursorGameX(), cursorGameY(), rightDown_);
        game_->assignSkill(Skill(selected_));
        return;
    }
    int py = y - kViewH;
    // Minimap: jump the view.
    if (x >= 208 && x < 308 && py >= 18 && py < 38) {
        scrollX_ = std::clamp((x - 208) * 16 - kPlayW / 2, 0, kLevelWidth - kPlayW);
        return;
    }
    if (py < 16 || x >= BtnCount * 16) return;
    int b = x / 16;
    switch (b) {
    case BtnSlower: if (!paused_) { rrHeld_ = -1; rrFromKeyboard_ = false; } break;
    case BtnFaster: if (!paused_) { rrHeld_ = 1; rrFromKeyboard_ = false; } break;
    case BtnPause: paused_ = !paused_; break;
    case BtnNuke: pressNuke(); break;
    default: selectSkill(b); break;
    }
}

// ---------------------------------------------------------------------------
// Drawing

void Frontend::draw(uint32_t* pixels, int pitch) {
    if (mode_ == Mode::Playing) {
        drawPlay();
        for (int y = 0; y < kScreenH; ++y) {
            const uint8_t* src = &play_.px[size_t(y / 2) * kPlayW];
            uint32_t* dst = pixels + size_t(y) * pitch;
            for (int x = 0; x < kScreenW; ++x) dst[x] = rgb(pal_[src[x / 2] & 15]);
        }
        return;
    }
    // Menu-style screens are composed directly in RGB.
    std::vector<uint32_t> rgbBuf(size_t(kMenuW) * kMenuH, 0);
    menu_ = Bitmap(kMenuW, kMenuH, 0);
    drawMenuBackground();
    if (mode_ == Mode::Preview) drawPreview();
    else drawResult();
    for (size_t i = 0; i < rgbBuf.size(); ++i) rgbBuf[i] = rgb(menuPal_[menu_.px[i] & 15]);
    if (mode_ == Mode::Preview) {
        // Level overview in the level's own colours, 640x80 (1600x160 / 2.5).
        Bitmap lvl = renderLevelPreview(levelDef_, *gs_, spec_.get());
        Palette pp = levelPalette(spec_ ? spec_->vga : gs_->vgaPreview);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < kMenuW; ++x) {
                uint8_t c = lvl.at(x * kLevelWidth / kMenuW, y * kLevelHeight / 64);
                rgbBuf[size_t(y + 16) * kMenuW + x] = rgb(pp[c & 15]);
            }
    }
    for (int y = 0; y < kScreenH; ++y) {
        uint32_t* dst = pixels + size_t(y) * pitch;
        int sy = y - kMenuY;
        for (int x = 0; x < kScreenW; ++x)
            dst[x] = (sy >= 0 && sy < kMenuH) ? rgbBuf[size_t(sy) * kMenuW + x] : 0;
    }
}

void Frontend::drawPlay() {
    game_->render(levelFrame_);
    std::fill(play_.px.begin(), play_.px.end(), 0);
    for (int y = 0; y < kViewH; ++y)
        std::copy_n(&levelFrame_.px[size_t(y) * kLevelWidth + scrollX_], kPlayW, &play_.px[size_t(y) * kPlayW]);
    drawPanel();

    // Cursor: a box around a lemming under it, otherwise a crosshair.
    int cx = mouseX_ / 2, cy = mouseY_ / 2;
    auto put = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < kPlayW && y < kPlayH) play_.at(x, y) = kWhite;
    };
    if (cy < kViewH && game_->hoveredLemming() >= 0) {
        for (int i = -7; i <= 7; ++i) {
            if (std::abs(i) >= 4) { put(cx + i, cy - 7); put(cx + i, cy + 7); put(cx - 7, cy + i); put(cx + 7, cy + i); }
        }
    } else {
        for (int i = 2; i <= 6; ++i) { put(cx + i, cy); put(cx - i, cy); put(cx, cy + i); put(cx, cy - i); }
    }
}

void Frontend::drawPanelText(int col, const std::string& s) {
    for (size_t i = 0; i < s.size() && col + int(i) < 40; ++i) {
        char c = s[i];
        int idx = -1;
        if (c == '%') idx = 0;
        else if (c >= '0' && c <= '9') idx = 1 + (c - '0');
        else if (c == '-') idx = 11;
        else if (c >= 'A' && c <= 'Z') idx = 12 + (c - 'A');
        if (idx < 0) continue;
        const Bitmap& g = md_.panelFont[idx];
        int x0 = (col + int(i)) * 8;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 8; ++x)
                if (g.at(x, y) != kTransparent) play_.at(x0 + x, kViewH + y) = g.at(x, y);
    }
}

void Frontend::drawPanel() {
    const Bitmap& p = md_.panel;
    for (int y = 0; y < p.h; ++y)
        for (int x = 0; x < p.w; ++x) play_.at(x, kViewH + y) = p.at(x, y);

    // Status line.
    if (int h = game_->hoveredLemming(); h >= 0) {
        const Lemming& l = game_->lemmings()[h];
        std::string name = actionName(l.action);
        if (l.action == Action::Walking || l.action == Action::Falling) {
            if (l.climber && l.floater) name = "ATHLETE";
            else if (l.climber) name = "CLIMBER";
            else if (l.floater) name = "FLOATER";
        }
        drawPanelText(0, name + " " + std::to_string(game_->hoverCount()));
    }
    drawPanelText(14, "OUT " + std::to_string(game_->lemmingsOut()));
    int total = std::max(1, game_->lemmingsTotal());
    drawPanelText(23, "IN " + std::to_string(game_->lemmingsIn() * 100 / total) + "%");
    char t[16];
    std::snprintf(t, sizeof t, "TIME %d-%02d", std::max(0, game_->minutes()), std::max(0, game_->seconds()));
    drawPanelText(31, t);

    // Button numbers: two 4-pixel digits combined into one 8x8 cell.
    auto number = [&](int button, int n) {
        int x0 = button * 16 + 4, y0 = kViewH + 17;
        if (n <= 0) {
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) play_.at(x0 + x, y0 + y) = 0;
            return;
        }
        n = std::min(n, 99);
        const Bitmap& l = md_.skillDigitL[n / 10];
        const Bitmap& r = md_.skillDigitR[n % 10];
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                const Bitmap& src = x < 4 ? l : r;
                play_.at(x0 + x, y0 + y) = src.at(x, y) != kTransparent ? kWhite : 0;
            }
    };
    number(BtnSlower, game_->minReleaseRate());
    number(BtnFaster, game_->releaseRate());
    for (int s = 0; s < SkillCount; ++s) number(BtnSkill0 + s, game_->skillCount(Skill(s)));

    auto frame = [&](int x0, int y0, int x1, int y1, uint8_t c) {
        for (int x = x0; x <= x1; ++x) { play_.at(x, y0) = c; play_.at(x, y1) = c; }
        for (int y = y0; y <= y1; ++y) { play_.at(x0, y) = c; play_.at(x1, y) = c; }
    };
    int sb = BtnSkill0 + selected_;
    frame(sb * 16, kViewH + 16, sb * 16 + 15, kViewH + 39, kWhite);
    if (paused_) frame(BtnPause * 16, kViewH + 16, BtnPause * 16 + 15, kViewH + 39, kWhite);

    // Minimap: terrain, lemmings, view rectangle.
    const Bitmap& w = game_->terrain();
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 100; ++x)
            play_.at(208 + x, kViewH + 18 + y) = w.at(x * 16, y * 8) != kTransparent ? 7 : 0;
    for (const Lemming& l : game_->lemmings()) {
        if (l.removed) continue;
        int mx = l.x / 16, my = l.y / 8;
        if (mx >= 0 && mx < 100 && my >= 0 && my < 20) play_.at(208 + mx, kViewH + 18 + my) = 2;
    }
    int vx = 208 + scrollX_ / 16;
    frame(vx, kViewH + 18, std::min(vx + 20, 319), kViewH + 37, kWhite);
}

void Frontend::drawMenuBackground() {
    const Bitmap& bg = md_.menuBackground;
    for (int y = 0; y < kMenuH; ++y)
        for (int x = 0; x < kMenuW; ++x) menu_.at(x, y) = bg.at(x % bg.w, y % bg.h);
}

void Frontend::drawMenuText(int x0, int y0, const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        int c = uint8_t(s[i]);
        if (c < 0x21 || c > 0x7E || size_t(c - 0x21) >= md_.menuFont.size()) continue;
        const Bitmap& g = md_.menuFont[c - 0x21];
        int gx = x0 + int(i) * 16;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                int px = gx + x, py = y0 + y;
                if (g.at(x, y) != kTransparent && px >= 0 && px < kMenuW && py >= 0 && py < kMenuH)
                    menu_.at(px, py) = g.at(x, y);
            }
    }
}

void Frontend::drawMenuTextCentered(int y, const std::string& s) {
    std::string t = s.size() > 40 ? s.substr(0, 40) : s;
    drawMenuText((kMenuW - int(t.size()) * 16) / 2, y, t);
}

void Frontend::drawPreview() {
    char buf[64];
    std::snprintf(buf, sizeof buf, "Level %d", level_ + 1);
    drawMenuTextCentered(100, std::string(buf) + "  " + levelDef_.name);
    std::snprintf(buf, sizeof buf, "Number of Lemmings %d", levelDef_.lemmings);
    drawMenuTextCentered(140, buf);
    std::snprintf(buf, sizeof buf, "%d%% To Be Saved",
                  levelDef_.lemmings ? levelDef_.rescue * 100 / levelDef_.lemmings : 0);
    drawMenuTextCentered(160, buf);
    std::snprintf(buf, sizeof buf, "Release Rate %d", levelDef_.releaseRate);
    drawMenuTextCentered(180, buf);
    std::snprintf(buf, sizeof buf, "Time %d Minute%s", levelDef_.timeMinutes, levelDef_.timeMinutes == 1 ? "" : "s");
    drawMenuTextCentered(200, buf);
    drawMenuTextCentered(220, std::string("Rating  ") + upper(ratingName(rating_)));
    if (codeEntry_) {
        drawMenuTextCentered(256, "Enter Code");
        drawMenuTextCentered(280, codeInput_ + std::string(10 - codeInput_.size(), '.'));
        if (!codeMessage_.empty()) drawMenuTextCentered(304, codeMessage_);
        return;
    }
    if (!codeMessage_.empty()) drawMenuTextCentered(240, codeMessage_);
    drawMenuTextCentered(272, "Press mouse button to continue");
    drawMenuTextCentered(304, "Arrows: level  C: code  Esc: quit");
}

void Frontend::drawResult() {
    char buf[64];
    drawMenuTextCentered(48, result_.timeUp ? "Your time is up!" : "All lemmings accounted for.");
    std::snprintf(buf, sizeof buf, "You rescued %d%%", result_.achieved);
    drawMenuTextCentered(96, buf);
    std::snprintf(buf, sizeof buf, "You needed  %d%%", result_.needed);
    drawMenuTextCentered(112, buf);
    if (result_.achieved == 100) drawMenuTextCentered(160, "Superb! You rescued every lemming.");
    else if (result_.success) drawMenuTextCentered(160, "Well done! On to the next level.");
    else if (result_.achieved == 0) drawMenuTextCentered(160, "Oh dear, not even one rescued.");
    else drawMenuTextCentered(160, "So close... have another go.");
    if (result_.success) {
        int next = rating_ * kLevelsPerRating + level_ + 1;
        if (next < kRatings * kLevelsPerRating) {
            std::string code = encodeAccessCode({next, result_.achieved, codeFlags_});
            std::snprintf(buf, sizeof buf, "Your Access Code for Level %d", next % kLevelsPerRating + 1);
            drawMenuTextCentered(208, buf);
            drawMenuTextCentered(228, "is " + code);
        }
    }
    drawMenuTextCentered(272, result_.success ? "Press mouse button for next level"
                                              : "Press mouse button to retry level");
}

} // namespace lem
