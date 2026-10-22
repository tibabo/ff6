#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "audio/akao.hpp"
#include "audio/audio_data.hpp"
#include "audio/dsp_voice.hpp"

namespace ff6 {

// The sound chip with the music: eight voices, the master volume and the echo, driven by the sequencer.
// It makes 32 kHz stereo samples; the audio data must outlive it.
class Synth {
public:
    explicit Synth(const AudioData& audio);
    // `suspendCurrent` keeps the song that plays, so that asking for it again resumes it where it was.
    void playSong(size_t song, uint8_t volume = 0xFF, bool alternateStart = false, bool suspendCurrent = false);
    void stop();
    // The sound effects are scripts that run on the voices 4 to 7, in a sequencer of their own.
    void stopSfx();
    void stopSfxVoice(int voice);
    AkaoSequencer& sfx() { return sfx_; }
    const AkaoSequencer& sfx() const { return sfx_; }
    AkaoSequencer& sequencer() { return sequencer_; }
    const AkaoSequencer& sequencer() const { return sequencer_; }
    // The echo delay in steps of 16 ms (0 to 15), the game sets it with the sound command 0xFC. The echo is cleared.
    void setEchoDelay(int delay);
    // True while the script runs or a voice still sounds.
    bool playing() const;
    bool loaded() const { return loaded_; }
    // Fills `frames` stereo frames (left, right) of 16-bit samples.
    void render(int16_t* out, size_t frames);

private:
    void tick();

    const AudioData& audio_;
    AkaoSequencer sequencer_;
    AkaoSequencer sfx_;
    uint8_t echoMask_ = 0, noiseMask_ = 0, pitchModMask_ = 0;  // the voices that the echo, the noise and the pitch modulation reach
    size_t song_ = 0;
    bool loaded_ = false;
    AkaoSequencer suspended_;
    size_t suspendedSong_ = 0;
    bool hasSuspended_ = false;
    std::array<DspVoice, FF6_AKAO_VOICES> voices_;
    uint32_t counter_ = 0;  // counts down, and times the envelopes and the noise
    int noise_ = 0x4000;
    int untilTick_ = 0;
    std::vector<int16_t> echoBuffer_;  // left, right
    size_t echoPosition_ = 0;
    std::array<std::array<int, FF6_AKAO_FIR_TAPS>, 2> firHistory_{};
};

}  // namespace ff6
