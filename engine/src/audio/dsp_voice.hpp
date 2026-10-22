#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "constants.hpp"

namespace ff6 {

// What the chip gives a voice on each output sample.
struct DspInputs {
    uint32_t counter = 0;  // the global counter that times the envelopes and the noise
    int noise = 0;  // the noise generator, when the voice plays noise instead of its sample
    bool noisy = false;
    int previousOutput = 0;  // the output of the previous voice, when this one modulates its pitch
    bool modulated = false;
};

// One voice of the sound chip: reads BRR blocks, shifts the pitch and applies the ADSR envelope.
// The data that it plays must outlive it.
class DspVoice {
public:
    // The sound chip keys a voice on with the registers of its instrument.
    void keyOn(const uint8_t* brr, size_t size, uint16_t loopStart, uint8_t adsr1, uint8_t adsr2);
    void keyOff() { if (active_) state_ = State::Release; }
    void setPitch(uint16_t pitch) { pitch_ = pitch & FF6_DSP_PITCH_MASK; }
    void setVolume(int8_t left, int8_t right) { left_ = left; right_ = right; }
    void setAdsr(uint8_t adsr1, uint8_t adsr2) { adsr1_ = adsr1; adsr2_ = adsr2; }
    bool active() const { return active_; }
    int envelope() const { return env_; }

    // One output sample at FF6_DSP_SAMPLE_RATE. The voice adds its output to `left` and `right` and returns it
    // before the channel volumes, for the voice after it.
    int render(const DspInputs& inputs, int32_t* left, int32_t* right);
    int render(uint32_t counter, int32_t* left, int32_t* right) {
        DspInputs inputs;
        inputs.counter = counter;
        return render(inputs, left, right);
    }

    // True when the envelope or the noise steps on this value of the counter, at this rate (0 to 31).
    static bool rateFires(int rate, uint32_t counter);

private:
    enum class State { Attack, Decay, Sustain, Release };

    void nextSample();
    void decodeBlock();
    void stepEnvelope(uint32_t counter);

    const uint8_t* brr_ = nullptr;
    size_t blocks_ = 0, loopBlock_ = 0, block_ = 0;
    uint8_t adsr1_ = 0, adsr2_ = 0;
    bool active_ = false;
    State state_ = State::Release;
    int env_ = 0;
    uint32_t pos_ = 0;  // 12-bit fraction of the position between two samples of the source
    uint16_t pitch_ = FF6_DSP_PITCH_ONE;
    int8_t left_ = 0, right_ = 0;
    std::array<int16_t, FF6_BRR_BLOCK_SAMPLES> decoded_{};
    size_t index_ = FF6_BRR_BLOCK_SAMPLES;  // next sample to take from `decoded_`
    int16_t history_[2] = {0, 0};  // BRR filter state
    int recent_[4] = {0, 0, 0, 0};  // the last samples of the source, oldest first
};

}  // namespace ff6
