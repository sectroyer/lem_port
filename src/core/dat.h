// SPDX-License-Identifier: GPL-3.0-or-later
// Decoder for the compressed .DAT container used by most game data files.
// See docs/formats/dat-compression.md.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lem {

using Bytes = std::vector<uint8_t>;

struct DatSection {
    Bytes data;
};

// Splits a compressed .DAT file into sections and decompresses each one.
// Throws std::runtime_error on malformed input (bad header, checksum mismatch).
std::vector<DatSection> decompressDat(const Bytes& file);

// Decompresses a single section body. `bitsInFirst` is the header's first byte.
Bytes decompressSection(const uint8_t* src, size_t srcLen, int bitsInFirst,
                        size_t outLen);

// Compresses `data` into one section (header + body) using only raw literal
// runs. The result is larger than the input but readable by any decoder.
Bytes compressSectionStored(const Bytes& data);

Bytes readFile(const std::string& path);
void writeFile(const std::string& path, const Bytes& data);

} // namespace lem
