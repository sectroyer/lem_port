// SPDX-License-Identifier: GPL-3.0-or-later
#include "graphicset.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace lem {

namespace {

int le16(const uint8_t* p) { return p[0] | p[1] << 8; }

Rgb egaToRgb(uint8_t v) {
    auto c = [](int hi, int lo) { return uint8_t((hi * 2 + lo) * 85); };
    return {c(v >> 5 & 1, v >> 2 & 1), c(v >> 4 & 1, v >> 1 & 1), c(v >> 3 & 1, v & 1)};
}

std::array<Rgb, 8> readVga(const uint8_t* p) {
    std::array<Rgb, 8> r{};
    for (int i = 0; i < 8; ++i) r[i] = vgaToRgb(p[3 * i], p[3 * i + 1], p[3 * i + 2]);
    return r;
}

std::array<Rgb, 8> readEga(const uint8_t* p) {
    std::array<Rgb, 8> r{};
    for (int i = 0; i < 8; ++i) r[i] = egaToRgb(p[i]);
    return r;
}

} // namespace

int ObjectInfo::previewFrameIndex() const {
    if (frames.empty() || frameSize == 0 || previewLoc < baseLoc) return 0;
    int i = (previewLoc - baseLoc) / frameSize;
    return i < int(frames.size()) ? i : 0;
}

Palette levelPalette(const std::array<Rgb, 8>& upper) {
    Palette p{};
    static const uint8_t fixed[7][3] = {
        {0x00, 0x00, 0x00}, {0x10, 0x10, 0x38}, {0x00, 0x2C, 0x00}, {0x3C, 0x34, 0x34},
        {0x2C, 0x2C, 0x00}, {0x3C, 0x08, 0x08}, {0x20, 0x20, 0x20}};
    for (int i = 0; i < 7; ++i) p[i] = vgaToRgb(fixed[i][0], fixed[i][1], fixed[i][2]);
    p[7] = upper[0];
    for (int i = 0; i < 8; ++i) p[8 + i] = upper[i];
    return p;
}

GraphicSet loadGraphicSet(const std::string& dataDir, int n) {
    Bytes ground = readFile(joinPath(dataDir, "ground" + std::to_string(n) + "o.dat"));
    if (ground.size() != 1056) throw std::runtime_error("ground: unexpected size");
    auto secs = decompressDat(readFile(joinPath(dataDir, "vgagr" + std::to_string(n) + ".dat")));
    if (secs.size() != 2) throw std::runtime_error("vgagr: expected 2 sections");
    const Bytes& terrainData = secs[0].data;
    const Bytes& objectData = secs[1].data;

    GraphicSet gs;
    for (int i = 0; i < 16; ++i) {
        const uint8_t* o = ground.data() + 28 * i;
        ObjectInfo& oi = gs.objects[i];
        oi.flags = le16(o);
        oi.startFrame = o[2];
        oi.endFrame = o[3];
        oi.w = o[4];
        oi.h = o[5];
        oi.frameSize = le16(o + 6);
        oi.maskOffset = le16(o + 8);
        oi.trigLeft = le16(o + 14) * 4;
        oi.trigTop = le16(o + 16) * 4 - 4;
        oi.trigW = o[18] * 4;
        oi.trigH = o[19] * 4;
        oi.effect = TriggerEffect(o[20]);
        oi.baseLoc = le16(o + 21);
        oi.previewLoc = le16(o + 23);
        oi.sound = o[27];
        oi.used = oi.w > 0 && oi.h > 0;
        if (!oi.used) continue;
        int frames = std::max(1, oi.endFrame);
        for (int f = 0; f < frames; ++f) {
            size_t img = size_t(oi.baseLoc) + size_t(f) * oi.frameSize;
            oi.frames.push_back(decodePlanarMasked(objectData, img, img + oi.maskOffset,
                                                   oi.w, oi.h, 4));
        }
    }
    for (int i = 0; i < 64; ++i) {
        const uint8_t* t = ground.data() + 448 + 8 * i;
        TerrainInfo& ti = gs.terrain[i];
        ti.w = t[0];
        ti.h = t[1];
        ti.imageLoc = le16(t + 2);
        ti.maskLoc = le16(t + 4);
        ti.used = ti.w > 0 && ti.h > 0;
        if (ti.used)
            ti.bmp = decodePlanarMasked(terrainData, ti.imageLoc, ti.maskLoc, ti.w, ti.h, 4);
    }
    const uint8_t* pal = ground.data() + 0x3C0;
    gs.egaCustom = readEga(pal);
    gs.egaStandard = readEga(pal + 8);
    gs.egaPreview = readEga(pal + 16);
    gs.vgaCustom = readVga(pal + 24);
    gs.vgaStandard = readVga(pal + 48);
    gs.vgaPreview = readVga(pal + 72);
    return gs;
}

std::vector<Bytes> decodeSpecialRle(const Bytes& src, size_t offset) {
    std::vector<Bytes> out(1);
    size_t i = offset;
    while (i < src.size()) {
        uint8_t c = src[i++];
        if (c == 0x80) {
            out.emplace_back();
        } else if (c < 0x80) {
            size_t n = size_t(c) + 1;
            if (i + n > src.size()) throw std::runtime_error("spec: raw run out of range");
            out.back().insert(out.back().end(), src.begin() + i, src.begin() + i + n);
            i += n;
        } else {
            if (i >= src.size()) throw std::runtime_error("spec: truncated run");
            out.back().insert(out.back().end(), size_t(257 - c), src[i++]);
        }
    }
    if (out.back().empty()) out.pop_back();
    return out;
}

SpecialSet loadSpecialSet(const std::string& dataDir, int n) {
    auto secs = decompressDat(readFile(joinPath(dataDir, "vgaspec" + std::to_string(n) + ".dat")));
    if (secs.size() != 1) throw std::runtime_error("vgaspec: expected 1 section");
    const Bytes& d = secs[0].data;
    SpecialSet s;
    s.vga = readVga(d.data());
    auto parts = decodeSpecialRle(d, 40);
    if (parts.size() < 4) throw std::runtime_error("vgaspec: expected 4 bitmap parts");
    s.terrain = Bitmap(960, 160);
    for (int q = 0; q < 4; ++q) {
        if (parts[q].size() != 14400) throw std::runtime_error("vgaspec: bad part size");
        Bitmap b = decodePlanar(parts[q], 0, 960, 40, 3);
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 960; ++x) {
                uint8_t v = b.at(x, y);
                s.terrain.at(x, q * 40 + y) = v ? uint8_t(v + 8) : kTransparent;
            }
    }
    return s;
}

} // namespace lem
