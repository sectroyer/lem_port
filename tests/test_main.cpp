// Unit tests that do not need the original game data.
#include <cstdio>
#include <cstdlib>

#include "dat.h"
#include "level.h"

using namespace lem;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static void testStoredRoundTrip() {
    for (size_t n : {1u, 5u, 8u, 9u, 100u, 264u, 265u, 2048u, 40000u}) {
        Bytes data(n);
        for (size_t i = 0; i < n; ++i) data[i] = uint8_t(i * 31 + 7);
        auto secs = decompressDat(compressSectionStored(data));
        CHECK(secs.size() == 1);
        CHECK(secs[0].data == data);
    }
}

// Bit stream example from the format description (docs/formats/dat-compression.md).
static void testReferenceExample() {
    const char* stream =
        "111" "00000000" "00000001" "00000010" "00000011" "00000100" "00000101" "00000110"
        "00000111" "00001000" "00001001" "101" "0000000111" "01" "00000001" "00" "001"
        "00000001" "11111111" "110" "00000110" "000000000010" "100" "000001110";
    size_t nbits = 0;
    for (const char* p = stream; *p; ++p) ++nbits;
    size_t nbytes = (nbits + 7) / 8;
    int first = int(nbits - (nbytes - 1) * 8);
    Bytes body(nbytes, 0);
    for (size_t k = 0; k < nbits; ++k) {
        size_t fromEnd = k < size_t(first) ? 0 : 1 + (k - first) / 8;
        size_t bit = k < size_t(first) ? k : (k - first) % 8;
        if (stream[k] == '1') body[nbytes - 1 - fromEnd] |= uint8_t(1u << bit);
    }
    const uint8_t expect[] = {0x04, 0x03, 0x02, 0x05, 0xFF, 0x01, 0x05, 0xFF, 0x01, 0x05,
                              0xFF, 0x01, 0x05, 0x04, 0x05, 0x04, 0x03, 0x02, 0x09, 0x08,
                              0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
    Bytes out = decompressSection(body.data(), body.size(), first, sizeof expect);
    CHECK(out == Bytes(expect, expect + sizeof expect));
}

static void testLevelParse() {
    Bytes rec(kLevelRecordSize, 0);
    rec[1] = 50; rec[3] = 40; rec[5] = 20; rec[7] = 5;
    // Terrain slots default to empty (0xFF); fill all but the first.
    for (size_t i = 0x120; i < 0x760; ++i) rec[i] = 0xFF;
    // Terrain piece: flags 0xC (no overwrite + upside down), x=0x011 -> 1,
    // y value 0x024 -> 36-4 = 32, id 5.
    rec[0x120] = 0xC0; rec[0x121] = 0x11; rec[0x122] = 0x12; rec[0x123] = 0x05;
    // Steel: 00 9F 52 00 -> (-12,124) 24x12.
    rec[0x760] = 0x00; rec[0x761] = 0x9F; rec[0x762] = 0x52;
    for (int i = 0; i < 32; ++i) rec[0x7E0 + i] = ' ';
    rec[0x7E0] = 'H'; rec[0x7E1] = 'i';
    Level L = parseLevel(rec);
    CHECK(L.releaseRate == 50 && L.lemmings == 40 && L.rescue == 20 && L.timeMinutes == 5);
    CHECK(L.terrain.size() == 1);
    CHECK(L.terrain[0].x == 1 && L.terrain[0].y == 32 && L.terrain[0].id == 5);
    CHECK(L.terrain[0].noOverwrite && L.terrain[0].upsideDown && !L.terrain[0].erase);
    CHECK(L.steel.size() == 1);
    CHECK(L.steel[0].x == -12 && L.steel[0].y == 124 && L.steel[0].w == 24 && L.steel[0].h == 12);
    CHECK(L.name == "Hi");
}

int main() {
    testStoredRoundTrip();
    testReferenceExample();
    testLevelParse();
    if (failures) { std::printf("%d failure(s)\n", failures); return EXIT_FAILURE; }
    std::printf("all tests passed\n");
    return EXIT_SUCCESS;
}
