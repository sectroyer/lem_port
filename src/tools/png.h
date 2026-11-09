// SPDX-License-Identifier: GPL-3.0-or-later
// Minimal PNG writer (uncompressed deflate blocks) for debug exports.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lem {

// Writes a 32-bit RGBA image. `rgba` holds w*h*4 bytes.
void writePngRgba(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba);

} // namespace lem
