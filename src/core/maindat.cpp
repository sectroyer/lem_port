// SPDX-License-Identifier: GPL-3.0-or-later
#include "maindat.h"

#include <stdexcept>

#include "graphicset.h"

namespace lem {

namespace {

const AnimSpec kAnims[] = {
    {"walk-r",       0x0000,  8, 16, 10, 2},
    {"jump-r",       0x0140,  1, 16, 10, 2},
    {"walk-l",       0x0168,  8, 16, 10, 2},
    {"jump-l",       0x02A8,  1, 16, 10, 2},
    {"dig",          0x02D0, 16, 16, 14, 3},
    {"climb-r",      0x0810,  8, 16, 12, 2},
    {"climb-l",      0x0990,  8, 16, 12, 2},
    {"drown",        0x0B10, 16, 16, 10, 2},
    {"postclimb-r",  0x0D90,  8, 16, 12, 2},
    {"postclimb-l",  0x0F10,  8, 16, 12, 2},
    {"build-r",      0x1090, 16, 16, 13, 3},
    {"build-l",      0x1570, 16, 16, 13, 3},
    {"bash-r",       0x1A50, 32, 16, 10, 3},
    {"bash-l",       0x21D0, 32, 16, 10, 3},
    {"mine-r",       0x2950, 24, 16, 13, 3},
    {"mine-l",       0x30A0, 24, 16, 13, 3},
    {"fall-r",       0x37F0,  4, 16, 10, 2},
    {"fall-l",       0x3890,  4, 16, 10, 2},
    {"prefloat-r",   0x3930,  4, 16, 16, 3},
    {"float-r",      0x3AB0,  4, 16, 16, 3},
    {"prefloat-l",   0x3C30,  4, 16, 16, 3},
    {"float-l",      0x3DB0,  4, 16, 16, 3},
    {"splat",        0x3F30, 16, 16, 10, 2},
    {"exit",         0x41B0,  8, 16, 13, 2},
    {"burn",         0x4350, 14, 16, 14, 4},
    {"block",        0x4970, 16, 16, 10, 2},
    {"shrug-r",      0x4BF0,  8, 16, 10, 2},
    {"shrug-l",      0x4D30,  8, 16, 10, 2},
    {"ohno",         0x4E70, 16, 16, 10, 2},
    {"explosion",    0x50F0,  1, 32, 32, 3},
};
static_assert(sizeof(kAnims) / sizeof(kAnims[0]) == size_t(Anim::Count));

const MaskSpec kMasks[] = {
    {"bash-r",    0x0000, 4, 16, 10},
    {"bash-l",    0x0050, 4, 16, 10},
    {"mine-r",    0x00A0, 2, 16, 13},
    {"mine-l",    0x00D4, 2, 16, 13},
    {"explosion", 0x0108, 1, 16, 22},
};
static_assert(sizeof(kMasks) / sizeof(kMasks[0]) == size_t(MaskId::Count));

size_t planarSize(int w, int h, int bpp) { return size_t(w) * h / 8 * bpp; }

Bitmap transparentZero(Bitmap b) {
    zeroToTransparent(b);
    return b;
}

} // namespace

const AnimSpec& animSpec(Anim a) { return kAnims[int(a)]; }
const MaskSpec& maskSpec(MaskId m) { return kMasks[int(m)]; }

Palette menuPalette() {
    static const uint8_t c[16][3] = {
        {0, 0, 0},       {128, 64, 32},   {96, 48, 32},    {48, 0, 16},
        {32, 8, 124},    {64, 44, 144},   {104, 88, 164},  {152, 140, 188},
        {0, 80, 0},      {0, 96, 16},     {0, 112, 32},    {0, 128, 64},
        {208, 208, 208}, {176, 176, 0},   {64, 80, 176},   {224, 128, 144}};
    Palette p{};
    for (int i = 0; i < 16; ++i) p[i] = {c[i][0], c[i][1], c[i][2]};
    return p;
}

MainData loadMainData(const std::string& dataDir) {
    auto secs = decompressDat(readFile(joinPath(dataDir, "main.dat")));
    if (secs.size() != 7) throw std::runtime_error("main.dat: expected 7 sections");
    MainData m;

    const Bytes& s0 = secs[0].data;
    for (int a = 0; a < int(Anim::Count); ++a) {
        const AnimSpec& sp = kAnims[a];
        for (int f = 0; f < sp.frames; ++f)
            m.anims[a].push_back(transparentZero(decodePlanar(
                s0, sp.offset + f * planarSize(sp.w, sp.h, sp.bpp), sp.w, sp.h, sp.bpp)));
    }

    const Bytes& s1 = secs[1].data;
    for (int k = 0; k < int(MaskId::Count); ++k) {
        const MaskSpec& sp = kMasks[k];
        for (int f = 0; f < sp.frames; ++f)
            m.masks[k].push_back(transparentZero(
                decodePlanar(s1, sp.offset + f * planarSize(sp.w, sp.h, 1), sp.w, sp.h, 1)));
    }
    for (int d = 0; d < 10; ++d)
        m.countdown[d] = transparentZero(decodePlanar(s1, 0x134 + (9 - d) * 8, 8, 8, 1));

    const Bytes& s2 = secs[2].data;
    for (int d = 0; d < 10; ++d) {
        m.skillDigitR[d] = transparentZero(decodePlanar(s2, 0x1900 + d * 16, 8, 8, 1));
        m.skillDigitL[d] = transparentZero(decodePlanar(s2, 0x1908 + d * 16, 8, 8, 1));
    }

    const Bytes& s3 = secs[3].data;
    m.menuBackground = decodePlanar(s3, 0x0000, 320, 104, 2);
    m.logo = transparentZero(decodePlanar(s3, 0x2080, 632, 94, 4));
    const int signOffs[6] = {0x9488, 0xA2D4, 0xB120, 0xBF6C, 0xCDB8, 0xDC04};
    for (int i = 0; i < 6; ++i) m.signs[i] = transparentZero(decodePlanar(s3, signOffs[i], 120, 61, 4));
    m.iconMusic = transparentZero(decodePlanar(s3, 0xEA50, 64, 31, 4));
    m.iconFx = transparentZero(decodePlanar(s3, 0xEE30, 64, 31, 4));

    const Bytes& s4 = secs[4].data;
    for (int b = 0; b < 7; ++b)
        for (int f = 0; f < 8; ++f)
            m.blinks[b].push_back(transparentZero(decodePlanar(s4, 0x600 * b + f * 0xC0, 32, 12, 4)));
    for (int f = 0; f < 16; ++f) {
        m.scrollerLeft.push_back(transparentZero(decodePlanar(s4, 0x2A00 + f * 0x180, 48, 16, 4)));
        m.scrollerRight.push_back(transparentZero(decodePlanar(s4, 0x4200 + f * 0x180, 48, 16, 4)));
    }
    m.reel = transparentZero(decodePlanar(s4, 0x5A00, 16, 16, 4));
    for (int i = 0; i < 4; ++i)
        m.ratingSigns[i] = transparentZero(decodePlanar(s4, 0x5A80 + i * 0x3CC, 72, 27, 4));
    for (int c = 0x21; c <= 0x7E; ++c) {
        size_t off = 0x69B0 + size_t(c - 0x21) * 0x60;
        if (off + 0x60 > s4.size()) break;
        m.menuFont.push_back(transparentZero(decodePlanar(s4, off, 16, 16, 3)));
    }

    const Bytes& s6 = secs[6].data;
    m.panel = decodePlanar(s6, 0x0000, 320, 40, 4);
    for (int i = 0; kPanelFontChars[i]; ++i)
        m.panelFont.push_back(transparentZero(decodePlanar(s6, 0x1900 + i * 0x30, 8, 16, 3)));
    return m;
}

} // namespace lem
