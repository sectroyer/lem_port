// SPDX-License-Identifier: GPL-3.0-or-later
// SDL audio output for the AdLib driver.
#pragma once

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_mutex.h>

#include <memory>
#include <string>

#include "adlib.h"

namespace lem {

class Audio {
public:
    ~Audio();
    // Returns false if the driver or the audio device is unavailable.
    bool open(const std::string& dataDir);

    void sfx(int id);
    void music(int track);
    void stop();
    void pauseOutput(bool paused);   // stop or restart the audio device

    bool musicOn = true;
    bool sfxOn = true;

private:
    static void SDLCALL feed(void* self, SDL_AudioStream* stream, int additional, int total);

    std::unique_ptr<AdlibDriver> driver_;
    SDL_AudioStream* stream_ = nullptr;
    SDL_Mutex* lock_ = nullptr;
};

} // namespace lem
