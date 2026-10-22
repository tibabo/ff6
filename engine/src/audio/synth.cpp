#include "audio/synth.hpp"

#include <algorithm>

namespace ff6 {
namespace {

int clamp16(int value) { return std::max(-32768, std::min(32767, value)); }

}  // namespace

Synth::Synth(const AudioData& audio) : audio_(audio) {
    sfx_.loadSfx(audio);
    setEchoDelay(FF6_AKAO_ECHO_DELAY);
}

void Synth::setEchoDelay(int delay) {
    const int steps = std::max(0, std::min(delay, FF6_AKAO_ECHO_DELAY_MAX));
    const size_t frames = steps == 0 ? 1 : static_cast<size_t>(steps) * FF6_AKAO_ECHO_FRAMES_PER_DELAY;
    echoBuffer_.assign(frames * 2, 0);
    echoPosition_ = 0;
    for (auto& history : firHistory_) history.fill(0);
    sequencer_.echoDelayChanged();
}

void Synth::playSong(size_t song, uint8_t volume, bool alternateStart, bool suspendCurrent) {
    if (suspendCurrent && loaded_ && !sequencer_.finished()) {
        suspended_ = sequencer_;
        suspendedSong_ = song_;
        hasSuspended_ = true;
    }
    stopSfx();  // loading a song cuts the sound effects too
    stop();
    if (song >= audio_.songs.size()) return;
    song_ = song;
    loaded_ = true;
    untilTick_ = 0;
    if (hasSuspended_ && suspendedSong_ == song) {
        hasSuspended_ = false;
        const AkaoSequencer current = sequencer_;
        sequencer_ = suspended_;
        sequencer_.keepGameSettings(current);
        sequencer_.setSongVolume(0, volume);
        return;
    }
    sequencer_.load(audio_, song, volume, alternateStart);
}

// The song stops; the voices of the sound effects go on.
void Synth::stop() {
    loaded_ = false;
    for (size_t v = 0; v < voices_.size(); ++v) {
        if (!(sfx_.activeChannels() & (1 << v))) voices_[v] = DspVoice();
    }
}

void Synth::stopSfx() {
    for (int v = 0; v < FF6_AKAO_VOICES; ++v) {
        if (!(sfx_.activeChannels() & (1 << v))) continue;
        sfx_.stopSfx(v);
        voices_[static_cast<size_t>(v)] = DspVoice();
    }
}

// A new effect takes the voice: the one that played there is released.
void Synth::stopSfxVoice(int voice) {
    sfx_.stopSfx(voice);
    voices_[static_cast<size_t>(voice)].keyOff();
}

bool Synth::playing() const {
    if (!loaded_) return false;
    if (!sequencer_.finished()) return true;
    return std::any_of(voices_.begin(), voices_.end(), [](const DspVoice& voice) { return voice.active(); });
}

// One tick of the script clock: the key events that the last tick left, then the new registers. A voice that a sound
// effect runs on follows the effect and not the song.
void Synth::tick() {
    KeyEvents music;
    if (loaded_) music = sequencer_.tick();
    const uint8_t owned = sfx_.activeChannels();  // the key events are those of the channels that were running
    const KeyEvents effects = sfx_.tick();
    if (sfx_.takeSfxStop()) stopSfx();
    for (size_t v = 0; v < voices_.size(); ++v) {
        const uint8_t bit = static_cast<uint8_t>(1 << v);
        const bool effect = (owned & bit) != 0;
        const KeyEvents& events = effect ? effects : music;
        if (events.keyOff & bit) voices_[v].keyOff();
        if (!(events.keyOn & bit)) continue;
        const VoiceRegs& regs = events.regs[v];
        const Sample* sample = (effect ? sfx_ : sequencer_).sampleOf(regs.instrument);
        if (!sample) continue;
        voices_[v].keyOn(sample->brr.data(), sample->brr.size(), sample->loopStart, regs.adsr1, regs.adsr2);
    }
    const uint8_t running = sfx_.activeChannels();
    for (size_t v = 0; v < voices_.size(); ++v) {
        const VoiceRegs& regs = (running & (1 << v) ? sfx_ : sequencer_).regs()[v];
        voices_[v].setPitch(regs.pitch);
        voices_[v].setVolume(static_cast<int8_t>(regs.volumeLeft), static_cast<int8_t>(regs.volumeRight));
        voices_[v].setAdsr(regs.adsr1, regs.adsr2);
    }
    const auto merge = [&](uint8_t music, uint8_t effect) {
        return static_cast<uint8_t>(((loaded_ ? music : 0) & ~running) | (effect & running));
    };
    echoMask_ = merge(sequencer_.echoVoices(), sfx_.echoVoices());
    noiseMask_ = merge(sequencer_.noiseVoices(), sfx_.noiseVoices());
    pitchModMask_ = merge(sequencer_.pitchModVoices(), sfx_.pitchModVoices());
}

void Synth::render(int16_t* out, size_t frames) {
    const size_t delay = echoBuffer_.size() / 2;
    for (size_t frame = 0; frame < frames; ++frame) {
        const bool idle = !loaded_ && sfx_.finished() &&
                          std::none_of(voices_.begin(), voices_.end(), [](const DspVoice& voice) { return voice.active(); });
        if (idle) {
            out[2 * frame] = out[2 * frame + 1] = 0;
            continue;
        }
        if (untilTick_ == 0) {
            tick();
            untilTick_ = FF6_AKAO_TICK_SAMPLES;
        }
        --untilTick_;
        counter_ = counter_ == 0 ? FF6_DSP_COUNTER_RANGE - 1 : counter_ - 1;
        if (DspVoice::rateFires(sequencer_.noiseClock(), counter_)) {
            noise_ = (((noise_ << 13) ^ (noise_ << 14)) & 0x4000) ^ (noise_ >> 1);
        }
        int32_t left = 0, right = 0, echoLeft = 0, echoRight = 0;
        int previousOutput = 0;
        for (size_t v = 0; v < voices_.size(); ++v) {
            int32_t voiceLeft = 0, voiceRight = 0;
            const uint8_t bit = static_cast<uint8_t>(1 << v);
            DspInputs inputs;
            inputs.counter = counter_;
            inputs.noise = noise_;
            inputs.noisy = (noiseMask_ & bit) != 0;
            inputs.modulated = v > 0 && (pitchModMask_ & bit) != 0;  // voice 0 has no voice before it
            inputs.previousOutput = previousOutput;
            previousOutput = voices_[v].render(inputs, &voiceLeft, &voiceRight);
            left += voiceLeft;
            right += voiceRight;
            if (echoMask_ & bit) {
                echoLeft += voiceLeft;
                echoRight += voiceRight;
            }
        }
        // The echo: the buffer is read, filtered, and written again with the voices that feed it.
        int16_t* slot = &echoBuffer_[echoPosition_ * 2];
        int fir[2];
        for (int channel = 0; channel < 2; ++channel) {
            std::array<int, FF6_AKAO_FIR_TAPS>& history = firHistory_[static_cast<size_t>(channel)];
            std::copy(history.begin() + 1, history.end(), history.begin());
            history[FF6_AKAO_FIR_TAPS - 1] = slot[channel] >> 1;
            int sum = 0;
            for (int i = 0; i < FF6_AKAO_FIR_TAPS - 1; ++i) sum += (history[static_cast<size_t>(i)] * sequencer_.firCoefficient(i)) >> 6;
            sum = static_cast<int16_t>(sum);
            sum += (history[FF6_AKAO_FIR_TAPS - 1] * sequencer_.firCoefficient(FF6_AKAO_FIR_TAPS - 1)) >> 6;
            fir[channel] = clamp16(sum) & ~1;
        }
        const int8_t feedback = static_cast<int8_t>(sequencer_.echoFeedback());
        slot[0] = static_cast<int16_t>(clamp16(clamp16(echoLeft) + ((fir[0] * feedback) >> 7)) & ~1);
        slot[1] = static_cast<int16_t>(clamp16(clamp16(echoRight) + ((fir[1] * feedback) >> 7)) & ~1);
        echoPosition_ = (echoPosition_ + 1) % delay;
        const int8_t echoVolume = static_cast<int8_t>(sequencer_.echoVolume());
        const int mixLeft = (clamp16(left) * FF6_AKAO_MASTER_VOLUME >> FF6_DSP_VOLUME_SHIFT) + ((fir[0] * echoVolume) >> 7);
        const int mixRight = (clamp16(right) * FF6_AKAO_MASTER_VOLUME >> FF6_DSP_VOLUME_SHIFT) + ((fir[1] * echoVolume) >> 7);
        out[2 * frame] = static_cast<int16_t>(clamp16(mixLeft));
        out[2 * frame + 1] = static_cast<int16_t>(clamp16(mixRight));
    }
}

}  // namespace ff6
