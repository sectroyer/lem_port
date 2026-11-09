// SPDX-License-Identifier: GPL-3.0-or-later
// Graphics sets: groundXo.dat metadata + vgagrX.dat bitmaps, and the
// full-terrain special sets in vgaspecX.dat.
// See docs/formats/graphics-set.md and docs/formats/special-set.md.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "bitmap.h"

namespace lem {

enum class TriggerEffect : int {
    None = 0, Exit = 1, Trap = 4, Drown = 5, Disintegrate = 6,
    OneWayLeft = 7, OneWayRight = 8, Steel = 9,
};

struct ObjectInfo {
    bool used = false;
    int flags = 0;           // bit 0: animation runs only when triggered
    int startFrame = 0, endFrame = 0;
    int w = 0, h = 0;
    int frameSize = 0, maskOffset = 0;
    int trigLeft = 0, trigTop = 0, trigW = 0, trigH = 0;  // pixels, relative
    TriggerEffect effect = TriggerEffect::None;
    int baseLoc = 0;
    int previewLoc = 0;      // byte offset of the frame shown in previews
    int previewFrameIndex() const;
    int sound = 0;
    std::vector<Bitmap> frames;  // decoded frames (count = endFrame)
};

struct TerrainInfo {
    bool used = false;
    int w = 0, h = 0;
    int imageLoc = 0, maskLoc = 0;
    Bitmap bmp;
};

struct GraphicSet {
    std::array<ObjectInfo, 16> objects;
    std::array<TerrainInfo, 64> terrain;
    std::array<Rgb, 8> egaCustom{}, egaStandard{}, egaPreview{};
    std::array<Rgb, 8> vgaCustom{}, vgaStandard{}, vgaPreview{};
};

// Loads graphics set `n` from directory `dataDir` (groundNo.dat + vgagrN.dat).
GraphicSet loadGraphicSet(const std::string& dataDir, int n);

struct SpecialSet {
    std::array<Rgb, 8> vga{};
    Bitmap terrain;   // 960x160, values 0 (empty) or 8..15, transparent = empty
};

// Loads special set `n` from vgaspecN.dat.
SpecialSet loadSpecialSet(const std::string& dataDir, int n);

// Second-stage run-length decoder used by vgaspecX.dat. Returns sub-sections
// separated by 0x80 terminators.
std::vector<Bytes> decodeSpecialRle(const Bytes& src, size_t offset);

// Builds the 16-colour in-level palette: fixed lower half, set colours upper
// half, colour 7 duplicating colour 8.
Palette levelPalette(const std::array<Rgb, 8>& upper);

std::string joinPath(const std::string& dir, const std::string& file);

} // namespace lem
