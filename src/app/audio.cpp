// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio.h"

#include <SDL3/SDL.h>

#include <vector>

namespace lem {

Audio::~Audio() {
    if (stream_) SDL_DestroyAudioStream(stream_);
    if (lock_) SDL_DestroyMutex(lock_);
}

bool Audio::open(const std::string& dataDir) {
    driver_.reset(AdlibDriver::load(dataDir));
    if (!driver_) return false;
    lock_ = SDL_CreateMutex();
    SDL_AudioSpec spec{SDL_AUDIO_S16, 1, driver_->sampleRate()};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, this);
    if (!stream_) {
        SDL_Log("audio: %s", SDL_GetError());
        driver_.reset();
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

void SDLCALL Audio::feed(void* self, SDL_AudioStream* stream, int additional, int) {
    Audio& a = *static_cast<Audio*>(self);
    int frames = additional / int(sizeof(int16_t));
    if (frames <= 0) return;
    std::vector<int16_t> buf(static_cast<size_t>(frames));
    SDL_LockMutex(a.lock_);
    a.driver_->generate(buf.data(), frames);
    SDL_UnlockMutex(a.lock_);
    SDL_PutAudioStreamData(stream, buf.data(), frames * int(sizeof(int16_t)));
}

void Audio::sfx(int id) {
    if (!driver_ || !sfxOn || id <= 0) return;
    SDL_LockMutex(lock_);
    driver_->playSfx(id);
    SDL_UnlockMutex(lock_);
}

void Audio::music(int track) {
    if (!driver_ || !musicOn) return;
    SDL_LockMutex(lock_);
    driver_->playMusic(track);
    SDL_UnlockMutex(lock_);
}

void Audio::stop() {
    if (!driver_) return;
    SDL_LockMutex(lock_);
    driver_->stop();
    SDL_UnlockMutex(lock_);
}

} // namespace lem
