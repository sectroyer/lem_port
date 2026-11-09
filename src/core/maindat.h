// Shared game graphics from main.dat. See docs/formats/main-dat.md.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "bitmap.h"

namespace lem {

enum class Anim : int {
    WalkR, JumpR, WalkL, JumpL, Dig, ClimbR, ClimbL, Drown, PostClimbR, PostClimbL,
    BuildR, BuildL, BashR, BashL, MineR, MineL, FallR, FallL, PreFloatR, FloatR,
    PreFloatL, FloatL, Splat, Exit, Burn, Block, ShrugR, ShrugL, OhNo, Explosion,
    Count
};

struct AnimSpec {
    const char* name;
    int offset, frames, w, h, bpp;
};

const AnimSpec& animSpec(Anim a);

enum class MaskId : int { BashR, BashL, MineR, MineL, Explosion, Count };

struct MaskSpec {
    const char* name;
    int offset, frames, w, h;
};

const MaskSpec& maskSpec(MaskId m);

struct MainData {
    // Lemming animations; pixel 0 = transparent.
    std::array<std::vector<Bitmap>, size_t(Anim::Count)> anims;
    // Terrain removal masks: 1 = remove, kTransparent = keep.
    std::array<std::vector<Bitmap>, size_t(MaskId::Count)> masks;
    std::array<Bitmap, 10> countdown;      // 8x8 digits 0..9 (value 1 = white)
    Bitmap panel;                          // 320x40 skill panel, 4 bpp
    std::array<Bitmap, 10> skillDigitL;    // 8x8, digit drawn in left half
    std::array<Bitmap, 10> skillDigitR;    // 8x8, digit drawn in right half
    std::vector<Bitmap> panelFont;         // 8x16, "%0123456789-A..Z"
    // Menu graphics (main menu palette).
    Bitmap menuBackground;                 // 320x104, 2 bpp
    Bitmap logo;                           // 632x94
    std::array<Bitmap, 6> signs;           // F1, F2, F3, rating, exit, F4
    Bitmap iconMusic, iconFx;
    std::array<std::vector<Bitmap>, 7> blinks;
    std::vector<Bitmap> scrollerLeft, scrollerRight;
    Bitmap reel;
    std::array<Bitmap, 4> ratingSigns;     // mayhem, taxing, tricky, fun
    std::vector<Bitmap> menuFont;          // 16x16, ASCII 0x21..0x7E
};

// Panel font characters in storage order.
constexpr const char* kPanelFontChars = "%0123456789-ABCDEFGHIJKLMNOPQRSTUVWXYZ";

MainData loadMainData(const std::string& dataDir);

Palette menuPalette();

} // namespace lem
