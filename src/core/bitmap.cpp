// SPDX-License-Identifier: GPL-3.0-or-later
#include "bitmap.h"

#include <stdexcept>

namespace lem {

Rgb vgaToRgb(uint8_t r, uint8_t g, uint8_t b) {
    auto c = [](uint8_t v) { return uint8_t((v & 0x3F) * 255 / 63); };
    return {c(r), c(g), c(b)};
}

Bitmap decodePlanar(const Bytes& src, size_t offset, int w, int h, int bpp) {
    size_t planeBytes = size_t(w) * h / 8;
    if (offset + planeBytes * bpp > src.size())
        throw std::runtime_error("planar bitmap out of range");
    Bitmap b(w, h, 0);
    for (int p = 0; p < bpp; ++p) {
        const uint8_t* plane = src.data() + offset + planeBytes * p;
        for (size_t i = 0; i < size_t(w) * h; ++i)
            if (plane[i >> 3] & (0x80 >> (i & 7))) b.px[i] |= uint8_t(1 << p);
    }
    return b;
}

Bitmap decodePlanarMasked(const Bytes& src, size_t imgOffset, size_t maskOffset,
                          int w, int h, int bpp) {
    Bitmap b = decodePlanar(src, imgOffset, w, h, bpp);
    Bitmap m = decodePlanar(src, maskOffset, w, h, 1);
    for (size_t i = 0; i < b.px.size(); ++i)
        if (!m.px[i]) b.px[i] = kTransparent;
    return b;
}

void zeroToTransparent(Bitmap& bmp) {
    for (auto& p : bmp.px)
        if (p == 0) p = kTransparent;
}

void remap(Bitmap& bmp, const std::array<uint8_t, 16>& map) {
    for (auto& p : bmp.px)
        if (p != kTransparent) p = map[p & 15];
}

Bitmap flipVertical(const Bitmap& b) {
    Bitmap r(b.w, b.h);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) r.at(x, b.h - 1 - y) = b.at(x, y);
    return r;
}

} // namespace lem
