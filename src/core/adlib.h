// SPDX-License-Identifier: GPL-3.0-or-later
// Music and sound effects: runs the game's own AdLib driver (adlib.dat) on a
// small 8086 interpreter and feeds its FM chip writes to an OPL2 emulator.
// See docs/formats/adlib.md.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "dat.h"

namespace lem {

class AdlibDriver {
public:
    // `image` is the decompressed adlib.dat.
    explicit AdlibDriver(const Bytes& image);
    ~AdlibDriver();

    static AdlibDriver* load(const std::string& dataDir);   // nullptr if absent

    void playSfx(int id);        // sound effect 1..18
    void playMusic(int track);   // track 1..21
    void stop();                 // silence everything

    // Renders mono samples at sampleRate(); advances the driver at its tick rate.
    void generate(int16_t* out, int frames);
    int sampleRate() const { return sampleRate_; }

    bool ok() const { return error_.empty(); }
    const std::string& error() const { return error_; }

    // Driver tick rate: the PC timer programmed with divisor 0x4000.
    static constexpr double kTickHz = 1193182.0 / 16384.0;

private:
    void call(uint8_t ah, uint8_t al = 0);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    int sampleRate_ = 49716;
    double tickPhase_ = 0;
    std::string error_;
};

} // namespace lem
