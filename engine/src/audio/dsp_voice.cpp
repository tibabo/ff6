#include "audio/dsp_voice.hpp"

#include <algorithm>

#include "audio/brr.hpp"
#include "audio/gauss_table.hpp"

namespace ff6 {
namespace {

// Samples between two steps for each rate; rate 0 never steps. The offsets shift the steps of some rates.
constexpr uint16_t kRatePeriod[FF6_DSP_RATE_COUNT] = {FF6_DSP_COUNTER_RANGE + 1, 2048, 1536, 1280, 1024, 768, 640,
                                                      512, 384, 320, 256, 192, 160, 128, 96, 80, 64, 48, 40,
                                                      32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1};
constexpr uint16_t kRateOffset[FF6_DSP_RATE_COUNT] = {1,    0,   1040, 536, 0,   1040, 536, 0,
                                                      1040, 536, 0,    1040, 536, 0,   1040, 536,
                                                      0,    1040, 536, 0,    1040, 536, 0,   1040,
                                                      536,  0,   1040, 536, 0,   1040, 0,   0};

int clamp16(int value) { return std::max(-32768, std::min(32767, value)); }

// The gaussian kernel of the chip over the four last samples, at the fraction `pos` of the pitch counter.
int interpolate(const int* s, uint32_t pos) {
    const int offset = static_cast<int>(pos >> 4) & 0xFF;
    const int16_t* forward = akao::kGauss + 255 - offset;
    const int16_t* reverse = akao::kGauss + offset;
    int out = (forward[0] * s[0]) >> 11;
    out += (forward[256] * s[1]) >> 11;
    out += (reverse[256] * s[2]) >> 11;
    out = static_cast<int16_t>(out);
    out += (reverse[0] * s[3]) >> 11;
    return clamp16(out) & ~1;
}

}  // namespace

bool DspVoice::rateFires(int rate, uint32_t counter) {
    return (counter + kRateOffset[rate]) % kRatePeriod[rate] == 0;
}

void DspVoice::keyOn(const uint8_t* brr, size_t size, uint16_t loopStart, uint8_t adsr1, uint8_t adsr2) {
    brr_ = brr;
    blocks_ = size / FF6_BRR_BLOCK_BYTES;
    loopBlock_ = loopStart / FF6_BRR_BLOCK_BYTES;
    block_ = 0;
    adsr1_ = adsr1;
    adsr2_ = adsr2;
    active_ = blocks_ > 0;
    state_ = State::Attack;
    env_ = 0;
    pos_ = 0;
    index_ = FF6_BRR_BLOCK_SAMPLES;
    history_[0] = history_[1] = 0;
    std::fill(std::begin(recent_), std::end(recent_), 0);
    // The first samples are read before the first output.
    for (int i = 0; i < 4 && active_; ++i) nextSample();
}

void DspVoice::decodeBlock() {
    decodeBrrBlock(brr_ + block_ * FF6_BRR_BLOCK_BYTES, history_, decoded_.data());
    index_ = 0;
}

// Takes the next sample of the source; at the end of a block the next one is read, or the loop starts.
void DspVoice::nextSample() {
    if (index_ >= FF6_BRR_BLOCK_SAMPLES) decodeBlock();
    std::copy(recent_ + 1, recent_ + 4, recent_);
    recent_[3] = decoded_[index_++];
    if (index_ == FF6_BRR_BLOCK_SAMPLES) {
        const uint8_t flags = brr_[block_ * FF6_BRR_BLOCK_BYTES];
        if (flags & FF6_BRR_FLAG_END) {
            if (flags & FF6_BRR_FLAG_LOOP) {
                block_ = std::min(loopBlock_, blocks_ - 1);
            } else {
                active_ = false;
                env_ = 0;
                return;
            }
        } else if (++block_ >= blocks_) {
            active_ = false;
            env_ = 0;
        }
    }
}

void DspVoice::stepEnvelope(uint32_t counter) {
    switch (state_) {
        case State::Attack: {
            const int rate = (adsr1_ & FF6_DSP_ADSR_AR_MASK) * 2 + 1;
            if (rateFires(rate, counter)) {
                env_ += rate == FF6_DSP_RATE_COUNT - 1 ? FF6_DSP_ATTACK_FAST_STEP : FF6_DSP_ATTACK_STEP;
                if (env_ >= FF6_DSP_ENV_MAX) {
                    env_ = FF6_DSP_ENV_MAX;
                    state_ = State::Decay;
                }
            }
            break;
        }
        case State::Decay: {
            const int rate = ((adsr1_ >> FF6_DSP_ADSR_DR_SHIFT) & FF6_DSP_ADSR_DR_MASK) * 2 + 16;
            if (rateFires(rate, counter)) env_ -= ((env_ - 1) >> 8) + 1;
            if ((env_ >> 8) == (adsr2_ >> FF6_DSP_ADSR_SL_SHIFT)) state_ = State::Sustain;
            break;
        }
        case State::Sustain:
            if (rateFires(adsr2_ & FF6_DSP_ADSR_SR_MASK, counter)) env_ -= ((env_ - 1) >> 8) + 1;
            break;
        case State::Release:
            env_ -= FF6_DSP_ENV_RELEASE_STEP;
            break;
    }
    if (env_ <= 0) {
        env_ = 0;
        if (state_ == State::Release) active_ = false;
    }
}

int DspVoice::render(const DspInputs& inputs, int32_t* left, int32_t* right) {
    if (!active_) return 0;
    int sample = interpolate(recent_, pos_);
    if (inputs.noisy) sample = static_cast<int16_t>(inputs.noise * 2);
    const int out = ((sample * env_) >> 11) & ~1;
    *left += (out * left_) >> FF6_DSP_VOLUME_SHIFT;
    *right += (out * right_) >> FF6_DSP_VOLUME_SHIFT;
    stepEnvelope(inputs.counter);
    int pitch = pitch_;
    if (inputs.modulated) pitch += ((inputs.previousOutput >> 5) * pitch) >> 10;
    pos_ += static_cast<uint32_t>(std::max(0, pitch));
    while (pos_ >= FF6_DSP_PITCH_ONE && active_) {
        pos_ -= FF6_DSP_PITCH_ONE;
        nextSample();
    }
    return out;
}

}  // namespace ff6
