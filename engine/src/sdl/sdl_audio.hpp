#pragma once

#include <SDL.h>

#include <cstddef>

#include "audio/sound_program.hpp"
#include "sound.hpp"

namespace ff6 {

// The sound device: the sound program makes the samples on the thread of the device, so the game reaches it with the
// device locked.
class SdlAudio : public Sound {
public:
    explicit SdlAudio(SoundProgram& program);
    ~SdlAudio() override;
    SdlAudio(const SdlAudio&) = delete;
    SdlAudio& operator=(const SdlAudio&) = delete;

    void command(uint8_t command, uint8_t arg1, uint8_t arg2) override;
    uint8_t port(int index) const override;
    int previousSong() const override;

private:
    static void callback(void* user, Uint8* stream, int length);

    SoundProgram& program_;
    SDL_AudioDeviceID device_ = 0;
};

}  // namespace ff6
