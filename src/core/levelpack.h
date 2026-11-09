// SPDX-License-Identifier: GPL-3.0-or-later
// The 120 levels of the original game: ratings, order and variations.
// See docs/formats/level-order.md.
#pragma once

#include <string>
#include <vector>

#include "level.h"

namespace lem {

constexpr int kRatings = 4;
constexpr int kLevelsPerRating = 30;

const char* ratingName(int rating);

class LevelPack {
public:
    // Reads levelNNN.dat and oddtable.dat from `dataDir`.
    explicit LevelPack(const std::string& dataDir);

    // rating 0..3 (Fun, Tricky, Taxing, Mayhem), number 0..29.
    Level level(int rating, int number) const;

    // Raw record index (0..79) and whether the variation table applies.
    struct Slot { int record; bool variation; };
    static Slot slot(int rating, int number);

private:
    std::vector<Bytes> records_;   // 80 decompressed level records
    Bytes oddtable_;               // 80 x 56 bytes
};

} // namespace lem
