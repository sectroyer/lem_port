#include "levelpack.h"

#include <cstdio>
#include <stdexcept>

#include "graphicset.h"

namespace lem {

namespace {

// Odd value v: level record (v-1)/2. Even value v: variation entry v/2-1
// applied to the record with the same index.
const uint8_t kOrder[kRatings][kLevelsPerRating] = {
    {147, 155, 157, 149, 151, 153, 159, 14, 22, 54, 70, 16, 29, 32, 38,
     42, 48, 72, 84, 104, 138, 23, 68, 96, 98, 116, 78, 100, 108, 134},
    {1, 30, 36, 50, 52, 56, 58, 80, 102, 120, 128, 130, 136, 5, 148,
     152, 154, 156, 160, 7, 11, 13, 15, 17, 19, 21, 25, 27, 33, 31},
    {37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63, 3,
     65, 67, 69, 71, 73, 75, 77, 79, 81, 83, 85, 87, 89, 35, 111},
    {91, 93, 95, 97, 99, 101, 103, 105, 107, 109, 112, 113, 115, 117, 119,
     121, 123, 125, 127, 150, 129, 9, 131, 133, 135, 137, 139, 141, 143, 145},
};

constexpr size_t kOddEntrySize = 56;

} // namespace

const char* ratingName(int rating) {
    static const char* names[] = {"Fun", "Tricky", "Taxing", "Mayhem"};
    return rating >= 0 && rating < kRatings ? names[rating] : "?";
}

LevelPack::Slot LevelPack::slot(int rating, int number) {
    if (rating < 0 || rating >= kRatings || number < 0 || number >= kLevelsPerRating)
        throw std::out_of_range("level out of range");
    int v = kOrder[rating][number];
    if (v & 1) return {(v - 1) / 2, false};
    return {v / 2 - 1, true};
}

LevelPack::LevelPack(const std::string& dataDir) {
    for (int f = 0; f < 10; ++f) {
        char name[16];
        std::snprintf(name, sizeof name, "level%03d.dat", f);
        for (auto& s : decompressDat(readFile(joinPath(dataDir, name)))) records_.push_back(s.data);
    }
    if (records_.size() != 80) throw std::runtime_error("expected 80 level records");
    oddtable_ = readFile(joinPath(dataDir, "oddtable.dat"));
    if (oddtable_.size() != 80 * kOddEntrySize) throw std::runtime_error("oddtable.dat: bad size");
}

Level LevelPack::level(int rating, int number) const {
    Slot s = slot(rating, number);
    Bytes rec = records_[s.record];
    if (s.variation) {
        const uint8_t* e = oddtable_.data() + s.record * kOddEntrySize;
        std::copy(e, e + 24, rec.begin());          // header words
        std::copy(e + 24, e + 56, rec.begin() + 0x7E0); // name
    }
    return parseLevel(rec);
}

} // namespace lem
