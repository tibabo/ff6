#pragma once

#include <cstddef>
#include <cstdint>

#include "audio/audio_data.hpp"
#include "audio/synth.hpp"
#include "sound.hpp"

namespace ff6 {

// The sound program as the game sees it: song and sound effect commands, volume slides, echo delay and the ports, on
// top of the synth. The audio data must outlive it.
class SoundProgram : public Sound {
public:
    explicit SoundProgram(const AudioData& audio) : audio_(audio), synth_(audio) {}

    void command(uint8_t command, uint8_t arg1, uint8_t arg2) override;
    uint8_t port(int index) const override;
    int previousSong() const override { return previousSong_; }
    // Fills `frames` stereo frames of 16-bit samples.
    void render(int16_t* out, size_t frames) { synth_.render(out, frames); }
    int currentSong() const { return currentSong_; }

private:
    void playSong(uint8_t command, uint8_t song, uint8_t volume);
    void playGameSfx(uint8_t effect, uint8_t pan);
    void playSystemSfx(uint8_t effect);
    void interrupt(uint8_t command, uint8_t arg1, uint8_t arg2);

    const AudioData& audio_;
    Synth synth_;
    int currentSong_ = -1;  // none after a stop command
    int previousSong_ = -1;
};

}  // namespace ff6
