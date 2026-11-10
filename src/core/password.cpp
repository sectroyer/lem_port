// SPDX-License-Identifier: GPL-3.0-or-later
#include "password.h"

#include <array>
#include <cctype>
#include <cstdint>

namespace lem {

namespace {

// Base letter of each position; data bits are combined with these.
constexpr std::array<int, 10> kBase = {'A', 'J', 'H', 'L', 'D', 'H', 'B', 'B', 'C', 'J'};

int rotations(int level) { return 8 - (level & 7); }

} // namespace

std::string encodeAccessCode(const AccessCode& in) {
    int L = in.level & 0xFF, F = in.flags & 0xF, P = in.percent & 0x7F;
    std::array<int, 10> c{};
    c[0] = kBase[0] | (L & 1) << 3 | (F & 1) << 1 | (P & 1) << 2;
    c[1] = kBase[1] | (L & 2) << 1 | (P & 2) >> 1;
    c[2] = kBase[2] | (L & 4) | (F & 2) >> 1 | (P & 4) >> 1;
    c[3] = kBase[3] | (L & 8) >> 3 | (P & 8) >> 2;
    c[4] = kBase[4] | (L & 0x10) >> 3 | (F & 4) >> 2 | (P & 0x10) >> 1;
    c[5] = kBase[5] | (L & 0x20) >> 5 | (F & 8) >> 2 | (P & 0x20) >> 3;
    c[6] = kBase[6] | (L & 0xC0) >> 4 | (P & 0x40) >> 6;
    c[7] = (L & 0xF) + kBase[7];
    c[8] = (L >> 4) + kBase[8];
    int sum = 0;
    for (int i = 0; i < 9; ++i) sum += c[i];
    c[9] = (sum & 0xF) + kBase[9];
    // The first seven letters are rotated right by a level-dependent amount.
    for (int r = rotations(L); r > 0; --r) {
        int last = c[6];
        for (int i = 6; i > 0; --i) c[i] = c[i - 1];
        c[0] = last;
    }
    std::string s;
    for (int v : c) s += char(v);
    return s;
}

std::optional<AccessCode> decodeAccessCode(const std::string& code) {
    if (code.size() != 10) return std::nullopt;
    std::array<int, 10> c{};
    for (int i = 0; i < 10; ++i) c[i] = std::toupper(uint8_t(code[i]));
    int sum = 0;
    for (int i = 0; i < 9; ++i) sum += c[i];
    if (((c[9] - (sum & 0xF)) & 0xFF) != kBase[9]) return std::nullopt;
    int L = ((c[8] - kBase[8]) & 0xF) << 4 | ((c[7] - kBase[7]) & 0xF);
    for (int r = rotations(L); r > 0; --r) {
        int first = c[0];
        for (int i = 0; i < 6; ++i) c[i] = c[i + 1];
        c[6] = first;
    }
    int check = (c[0] >> 3 & 1) | (c[1] >> 1 & 2) | (c[2] & 4) | (c[3] << 3 & 8) |
                (c[4] << 3 & 0x10) | (c[5] << 5 & 0x20) | (c[6] << 4 & 0xC0);
    if (check != L) return std::nullopt;
    AccessCode r;
    r.level = L;
    r.flags = (c[0] >> 1 & 1) | (c[2] << 1 & 2) | (c[4] << 2 & 4) | (c[5] << 2 & 8);
    r.percent = (c[0] >> 2 & 1) | (c[1] << 1 & 2) | (c[2] << 1 & 4) | (c[3] << 2 & 8) |
                (c[4] << 1 & 0x10) | (c[5] << 3 & 0x20) | (c[6] << 6 & 0x40);
    return r;
}

bool isCheatCode(const std::string& code) {
    if (code.size() != 10) return false;
    static const char kCheat[] = "BILLANDTED";
    for (int i = 0; i < 10; ++i)
        if (std::toupper(uint8_t(code[i])) != kCheat[i]) return false;
    return true;
}

} // namespace lem
