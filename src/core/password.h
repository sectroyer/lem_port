// SPDX-License-Identifier: GPL-3.0-or-later
// Ten-letter level access codes. See docs/formats/passwords.md.
#pragma once

#include <optional>
#include <string>

namespace lem {

struct AccessCode {
    int level = 0;     // 0..119 (rating * 30 + number)
    int percent = 0;   // percentage rescued on the previous level (0..127)
    int flags = 0;     // 4 extra bits carried from code to code
};

std::string encodeAccessCode(const AccessCode& c);

// Returns the decoded code, or nothing if the code is not valid.
// `percent` is not stored reliably by every code and is informational only.
std::optional<AccessCode> decodeAccessCode(const std::string& code);

// The code that enables the cheat mode of the original game.
bool isCheatCode(const std::string& code);

} // namespace lem
