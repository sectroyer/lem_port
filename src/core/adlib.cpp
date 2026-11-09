// SPDX-License-Identifier: GPL-3.0-or-later
#include "adlib.h"

#include <algorithm>

#include "graphicset.h"
#include "x86.h"
#include "ymfm_opl.h"

namespace lem {

namespace {

constexpr uint16_t kDriverSeg = 0x1000;
constexpr uint32_t kOplClock = 3579545;

// The driver only needs the chip itself; timers and IRQs are not used.
class OplInterface : public ymfm::ymfm_interface {};

} // namespace

struct AdlibDriver::Impl {
    Cpu8086 cpu;
    OplInterface intf;
    ymfm::ym3812 opl{intf};
    uint8_t address = 0;
};

AdlibDriver::AdlibDriver(const Bytes& image) : impl_(std::make_unique<Impl>()) {
    Impl& m = *impl_;
    std::copy(image.begin(), image.end(), m.cpu.mem.begin() + (uint32_t(kDriverSeg) << 4));
    m.opl.reset();
    sampleRate_ = int(m.opl.sample_rate(kOplClock));
    m.cpu.out = [&m](uint16_t port, uint8_t v) {
        if (port == 0x388) m.opl.write_address(v);
        else if (port == 0x389) m.opl.write_data(v);
    };
    m.cpu.in = [&m](uint16_t port) -> uint8_t {
        return port == 0x388 ? m.opl.read_status() : 0xFF;
    };
    // Same start-up sequence as the game: prime the driver with track 1,
    // then select no track and silence the chip.
    call(3, 1);
    call(0);
    call(3, 0);
    call(2);
    call(0);
}

AdlibDriver::~AdlibDriver() = default;

AdlibDriver* AdlibDriver::load(const std::string& dataDir) {
    try {
        auto secs = decompressDat(readFile(joinPath(dataDir, "adlib.dat")));
        if (secs.empty()) return nullptr;
        auto* d = new AdlibDriver(secs[0].data);
        if (!d->ok()) { delete d; return nullptr; }
        return d;
    } catch (...) {
        return nullptr;
    }
}

void AdlibDriver::call(uint8_t ah, uint8_t al) {
    if (!error_.empty()) return;
    Cpu8086& cpu = impl_->cpu;
    cpu.r[Cpu8086::AX] = uint16_t(ah << 8 | al);
    if (!cpu.callInterrupt(kDriverSeg, 0)) error_ = "sound driver: " + cpu.error();
}

void AdlibDriver::playSfx(int id) { call(4, uint8_t(id)); }

void AdlibDriver::playMusic(int track) {
    call(3, uint8_t(track));
    call(0);
}

void AdlibDriver::stop() { call(2); }

void AdlibDriver::generate(int16_t* out, int frames) {
    ymfm::ym3812::output_data od;
    const double ticksPerSample = kTickHz / sampleRate_;
    for (int i = 0; i < frames; ++i) {
        tickPhase_ += ticksPerSample;
        if (tickPhase_ >= 1.0) {
            tickPhase_ -= 1.0;
            call(0);
        }
        impl_->opl.generate(&od, 1);
        out[i] = int16_t(std::clamp(od.data[0], -32768, 32767));
    }
}

} // namespace lem
