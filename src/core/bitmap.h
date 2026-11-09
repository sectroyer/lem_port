// SPDX-License-Identifier: GPL-3.0-or-later
// Indexed-colour bitmaps and planar bitmap decoding.
// See docs/formats/planar-bitmaps.md.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "dat.h"

namespace lem {

constexpr uint8_t kTransparent = 0xFF;

// 8-bit indexed image. Pixel value kTransparent means "no pixel".
struct Bitmap {
    int w = 0, h = 0;
    std::vector<uint8_t> px;

    Bitmap() = default;
    Bitmap(int w_, int h_, uint8_t fill = kTransparent)
        : w(w_), h(h_), px(size_t(w_) * h_, fill) {}
    uint8_t at(int x, int y) const { return px[size_t(y) * w + x]; }
    uint8_t& at(int x, int y) { return px[size_t(y) * w + x]; }
    bool empty() const { return w == 0 || h == 0; }
};

struct Rgb { uint8_t r, g, b; };
using Palette = std::array<Rgb, 16>;

// Converts a 6-bit-per-channel VGA DAC triplet to 8-bit RGB.
Rgb vgaToRgb(uint8_t r, uint8_t g, uint8_t b);

// Decodes a planar bitmap: `bpp` consecutive bit planes, each w*h/8 bytes,
// MSB = leftmost pixel, plane k contributes value (1 << k).
// Pixels with value 0 stay 0 (callers decide on transparency).
Bitmap decodePlanar(const Bytes& src, size_t offset, int w, int h, int bpp);

// Decodes a planar bitmap plus a separate 1-bit mask; unmasked pixels become
// kTransparent.
Bitmap decodePlanarMasked(const Bytes& src, size_t imgOffset, size_t maskOffset,
                          int w, int h, int bpp);

// Treats colour 0 as transparent.
void zeroToTransparent(Bitmap& bmp);

// Applies a value mapping to every non-transparent pixel.
void remap(Bitmap& bmp, const std::array<uint8_t, 16>& map);

Bitmap flipVertical(const Bitmap& b);

} // namespace lem
