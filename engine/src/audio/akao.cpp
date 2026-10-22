#include "audio/akao.hpp"

#include <algorithm>

#include "audio/akao_tables.hpp"

namespace ff6 {
namespace {

using akao::kCommandBytes;
using akao::kFreqMult;
using akao::kNoteDuration;

uint16_t negate(uint16_t value) { return static_cast<uint16_t>(0 - value); }

uint8_t hi(uint16_t value) { return static_cast<uint8_t>(value >> 8); }
uint8_t lo(uint16_t value) { return static_cast<uint8_t>(value); }

}  // namespace

const Sample* AkaoSequencer::sampleOf(uint8_t instrument) const {
    if (!audio_) return nullptr;
    if (instrument < audio_->sfxSamples.size()) return &audio_->sfxSamples[instrument];
    const int slot = instrument - FF6_AKAO_SONG_INSTRUMENT_BASE;
    if (!song_ || slot < 0 || slot >= FF6_SONG_SAMPLE_SLOTS) return nullptr;
    const uint16_t id = song_->samples[static_cast<size_t>(slot)];
    return id != 0 && id <= audio_->samples.size() ? &audio_->samples[id - 1u] : nullptr;
}

uint8_t AkaoSequencer::fetch(int ch) {
    Channel& c = channels_[static_cast<size_t>(ch)];
    const int at = c.pc++;
    const std::vector<uint8_t>* script = c.sfxScript ? &audio_->sfxMemory : script_;
    if (!script || at < 0 || static_cast<size_t>(at) >= script->size()) return FF6_AKAO_END;
    return (*script)[static_cast<size_t>(at)];
}

// A channel that runs the script of a sound effect reads it from the sound memory, by address.
int AkaoSequencer::baseOf(const Channel& c) const { return c.sfxScript ? 0 : base_; }

// The settings that the game made stay when a song is loaded.
void AkaoSequencer::keepGameSettings(const AkaoSequencer& other) {
    tempoRatio_ = other.tempoRatio_;
    tempoRatioRate_ = other.tempoRatioRate_;
    tempoRatioTicks_ = other.tempoRatioTicks_;
    pitchShift_ = other.pitchShift_;
    pitchShiftRate_ = other.pitchShiftRate_;
    pitchShiftTicks_ = other.pitchShiftTicks_;
}

void AkaoSequencer::load(const AudioData& audio, size_t index, uint8_t songVolume, bool alternateStart) {
    const AkaoSequencer previous = *this;
    *this = AkaoSequencer();
    keepGameSettings(previous);
    if (index >= audio.songs.size()) return;
    audio_ = &audio;
    const Song& song = audio.songs[index];
    song_ = &song;
    script_ = &song.script;
    songVolume_ = static_cast<uint16_t>(songVolume << 8);
    const std::vector<uint8_t>& s = song.script;
    if (s.size() < FF6_AKAO_HEADER_BYTES) return;
    const auto word = [&](size_t at) { return static_cast<int>(s[at] | (s[at + 1] << 8)); };
    base_ = FF6_AKAO_HEADER_BYTES - word(0);
    const int end = word(FF6_AKAO_END_POINTER_OFFSET);
    const size_t pointers = FF6_AKAO_CHANNEL_POINTERS_OFFSET + (alternateStart ? 2 * FF6_AKAO_VOICES : 0);
    for (int ch = 0; ch < FF6_AKAO_VOICES; ++ch) {
        const int pointer = word(pointers + 2 * static_cast<size_t>(ch));
        if (pointer == end) continue;
        activeChannels_ |= static_cast<uint8_t>(1 << ch);
        Channel& c = channels_[static_cast<size_t>(ch)];
        c.pc = pointer + base_;
        c.duration = 1;
    }
    tempo_ = 1 << 8;  // the first tick runs the scripts
    tempoAccumulator_ = 0xFF;
}

KeyEvents AkaoSequencer::tick() {
    KeyEvents events;
    events.keyOn = keyOn_;
    events.keyOff = keyOff_;
    events.regs = regs_;
    keyOn_ = keyOff_ = 0;
    if (--interruptCounter_ == 0) {
        interruptCounter_ = FF6_AKAO_INTERRUPT_ENV_TICKS;
        updateInterruptEnvelopes();
    }
    scriptTick();
    for (int ch = 0; ch < FF6_AKAO_VOICES; ++ch) {
        const uint8_t bit = static_cast<uint8_t>(1 << ch);
        if ((activeChannels_ & bit) && (playing_ & bit)) updateFreqVol(ch);
    }
    volumeDirty_ = pitchDirty_ = 0;
    return events;
}

// The bank of sound effects: its channels are the voices that the effects take from the music.
void AkaoSequencer::loadSfx(const AudioData& audio) {
    *this = AkaoSequencer();
    audio_ = &audio;
    script_ = &audio.sfxMemory;
    sfx_ = true;
    tempo_ = static_cast<uint16_t>(FF6_SFX_TEMPO << 8);
}

// InitChan and what Interrupt_18 and SystemSfx set before the script runs.
void AkaoSequencer::startSfx(int voice, uint16_t address, bool system) {
    const uint8_t bit = static_cast<uint8_t>(1 << voice);
    channels_[static_cast<size_t>(voice)] = Channel();
    Channel& c = channels_[static_cast<size_t>(voice)];
    c.pc = address;
    c.duration = 2;
    activeChannels_ |= bit;
    playing_ &= static_cast<uint8_t>(~bit);
    slur_ &= static_cast<uint8_t>(~bit);
    drumRoll_ &= static_cast<uint8_t>(~bit);
    keyOn_ &= static_cast<uint8_t>(~bit);
    keyOff_ &= static_cast<uint8_t>(~bit);
    disableEffects(voice);
    if (system) {
        systemChannels_ |= bit;
    } else {
        systemChannels_ &= static_cast<uint8_t>(~bit);
        c.volume = static_cast<uint16_t>(FF6_SFX_DEFAULT_VOLUME << 8);
        c.pan = static_cast<uint16_t>(FF6_SFX_PAN_CENTER << 8);
        c.octave = FF6_SFX_DEFAULT_OCTAVE;
        setInstrument(voice, FF6_SFX_DEFAULT_INSTRUMENT);
    }
    volumeDirty_ |= bit;
}

// Interrupt 0x84 and 0x85: a signed value that slides in `duration` steps of 7 ticks, or changes at once.
void AkaoSequencer::slide(uint16_t* value, uint16_t* rate, uint8_t* ticks, int duration, uint8_t target) {
    const int current = hi(*value);
    if (duration == 0 || target == current) {
        *value = static_cast<uint16_t>(target << 8);
        *ticks = 0;
        return;
    }
    *ticks = static_cast<uint8_t>(duration);
    *rate = akao::envelopeRate(target ^ 0x80, current ^ 0x80, duration);  // the values are signed
}

void AkaoSequencer::setTempoRatio(int duration, uint8_t target) {
    slide(&tempoRatio_, &tempoRatioRate_, &tempoRatioTicks_, duration, target);
}

void AkaoSequencer::setPitchShift(int duration, uint8_t target) {
    slide(&pitchShift_, &pitchShiftRate_, &pitchShiftTicks_, duration, target);
    if (duration == 0) pitchDirty_ = activeChannels_;
}

// The tempo of UpdateScripts: the ratio of the game adds to it (positive) or takes from it (negative), as a fraction.
uint8_t AkaoSequencer::effectiveTempo() const {
    const uint8_t tempo = hi(tempo_), ratio = hi(tempoRatio_);
    if (ratio == 0) return tempo;
    const unsigned scaled = (tempo * ratio) >> 8;
    if (ratio & 0x80) return static_cast<uint8_t>(scaled == 0 ? 1 : scaled);
    const unsigned sum = ((scaled << 1) & 0xFF) + tempo;
    return static_cast<uint8_t>(sum > 0xFF ? 0xFF : sum);
}

// UpdateChanFreq: the pitch change of the game, for the voices of the song.
uint16_t AkaoSequencer::shiftPitch(uint16_t pitch) const {
    const uint8_t shift = hi(pitchShift_);
    const unsigned total = (shift * hi(pitch) + ((shift * lo(pitch)) >> 8)) & 0xFFFF;
    if (shift & 0x80) return static_cast<uint16_t>(total);
    return static_cast<uint16_t>(((total << 1) & 0xFFFF) + pitch);
}

void AkaoSequencer::stopSfx(int voice) {
    const uint8_t bit = static_cast<uint8_t>(1 << voice);
    playing_ &= static_cast<uint8_t>(~bit);
    systemChannels_ &= static_cast<uint8_t>(~bit);
    keyOn_ &= static_cast<uint8_t>(~bit);
    keyOff_ &= static_cast<uint8_t>(~bit);
    terminate(voice);
}

// Interrupt 0x83: the pan that the game sets for its sound effects, which moves over `duration` steps of 7 ticks.
void AkaoSequencer::setSfxPan(int duration, uint8_t target) {
    const int current = hi(sfxPan_);
    if (duration == 0 || target == current) {
        sfxPan_ = static_cast<uint16_t>(target << 8);
        sfxPanTicks_ = 0;
        volumeDirty_ = activeChannels_;
        return;
    }
    sfxPanTicks_ = static_cast<uint8_t>(duration);
    sfxPanRate_ = akao::envelopeRate(target, current, duration);
}

// Interrupt 0x80 and 0x81: the song volume slides to `target` in `duration` steps of 7 ticks, or changes at once.
void AkaoSequencer::setSongVolume(int duration, uint8_t target) {
    const int current = hi(songVolume_);
    if (duration == 0 || target == current) {
        songVolume_ = static_cast<uint16_t>(target << 8);
        songVolumeTicks_ = 0;
        volumeDirty_ = activeChannels_;
        return;
    }
    songVolumeTicks_ = static_cast<uint8_t>(duration);
    songVolumeRate_ = akao::envelopeRate(target, current, duration);
}

// UpdateInterruptEnv
void AkaoSequencer::updateInterruptEnvelopes() {
    if (sfxPanTicks_ != 0) {
        --sfxPanTicks_;
        const uint8_t before = hi(sfxPan_);
        sfxPan_ = static_cast<uint16_t>(sfxPan_ + sfxPanRate_);
        if (hi(sfxPan_) != before) volumeDirty_ |= activeChannels_;
    }
    if (tempoRatioTicks_ != 0) {
        --tempoRatioTicks_;
        tempoRatio_ = static_cast<uint16_t>(tempoRatio_ + tempoRatioRate_);
    }
    if (pitchShiftTicks_ != 0) {
        --pitchShiftTicks_;
        const uint8_t before = hi(pitchShift_);
        pitchShift_ = static_cast<uint16_t>(pitchShift_ + pitchShiftRate_);
        if (hi(pitchShift_) != before) pitchDirty_ |= activeChannels_;
    }
    if (songVolumeTicks_ == 0) return;
    --songVolumeTicks_;
    const uint8_t before = hi(songVolume_);
    songVolume_ = static_cast<uint16_t>(songVolume_ + songVolumeRate_);
    if (hi(songVolume_) != before) volumeDirty_ |= activeChannels_;
    // A slide of the volume of the sound effects that ends at zero stops them and brings the volume back.
    if (sfx_ && songVolumeTicks_ == 0 && hi(songVolume_) == 0) {
        songVolume_ = 0xFF00;
        sfxStopRequested_ = true;
    }
}

void AkaoSequencer::scriptTick() {
    const unsigned sum = tempoAccumulator_ + effectiveTempo();
    tempoAccumulator_ = static_cast<uint8_t>(sum);
    if (sum < 0x100) return;
    for (int ch = 0; ch < FF6_AKAO_VOICES; ++ch) {
        const uint8_t bit = static_cast<uint8_t>(1 << ch);
        if (!(activeChannels_ & bit)) continue;
        Channel& c = channels_[static_cast<size_t>(ch)];
        c.duration = (c.duration - 1) & 0xFF;
        if (c.duration == 0) {
            execScript(ch);
        } else if (c.duration == FF6_AKAO_DURATION_KEY_OFF && !(noKeyOff_ & bit)) {
            playing_ &= static_cast<uint8_t>(~bit);
            keyOff_ |= bit;
        }
        channelEnvelopes(ch);
    }
    songEnvelopes();
}

void AkaoSequencer::songEnvelopes() {
    if (tempoTicks_) {
        --tempoTicks_;
        tempo_ = static_cast<uint16_t>(tempo_ + tempoRate_);
    }
    if (echoVolumeTicks_) {
        --echoVolumeTicks_;
        echoVolume_ = static_cast<uint16_t>(echoVolume_ + echoVolumeRate_);
    }
    if (feedbackTicks_) {
        --feedbackTicks_;
        feedback_ = static_cast<uint16_t>(feedback_ + feedbackRate_);
    }
    if (filterTicks_) {
        --filterTicks_;
        for (size_t i = 0; i < fir_.size(); ++i) fir_[i] = static_cast<uint16_t>(fir_[i] + firRate_[i]);
    }
}

// UpdateChanEnv: the slides of one channel, once per script tick.
void AkaoSequencer::channelEnvelopes(int ch) {
    Channel& c = channels_[static_cast<size_t>(ch)];
    const uint8_t bit = static_cast<uint8_t>(1 << ch);
    if (c.vibratoDelayLeft) --c.vibratoDelayLeft;
    if (c.tremoloDelayLeft) --c.tremoloDelayLeft;
    if (c.volumeTicks) {
        --c.volumeTicks;
        const uint16_t before = c.volume;
        c.volume = static_cast<uint16_t>(c.volume + c.volumeRate);
        if (hi(c.volume) != hi(before)) volumeDirty_ |= bit;
    }
    if (c.panTicks) {
        --c.panTicks;
        const uint16_t before = c.pan;
        c.pan = static_cast<uint16_t>(c.pan + c.panRate);
        if (hi(c.pan) != hi(before)) volumeDirty_ |= bit;
    }
    if (c.glideRate != 0) {
        const uint32_t rate = c.glideRate;
        c.glideTicks = static_cast<uint8_t>(c.glideTicks - 1);
        if (c.glideTicks == 0) c.glideRate = 0;
        const uint32_t pitch = (static_cast<uint32_t>(c.basePitch) << 8 | c.glideFraction) + rate;
        c.basePitch = static_cast<uint16_t>(pitch >> 8);
        c.glideFraction = static_cast<uint8_t>(pitch);
        pitchDirty_ |= bit;
    }
    if (c.sweepParam) {
        const uint16_t before = c.sweepPhase;
        c.sweepPhase = static_cast<uint16_t>(c.sweepPhase + c.sweepStep);
        if (hi(c.sweepPhase) != hi(before)) volumeDirty_ |= bit;
        uint8_t left = static_cast<uint8_t>(c.sweepLeft - 1);
        if (left == 0) {
            c.sweepStep = negate(c.sweepStep);
            left = c.sweepPeriod;
        }
        c.sweepLeft = left;
    }
}

// UpdateVibratoTremolo: the next rate of change of a vibrato or tremolo, and its new gain counter.
uint16_t AkaoSequencer::vibratoTremoloRate(uint8_t counter, uint16_t maxRate, uint8_t param, uint16_t previous,
                                           uint8_t* newGain) const {
    *newGain = 0;
    if (param >= 0xC0) {  // balanced
        uint16_t rate = previous;
        if (counter != 0) {
            *newGain = static_cast<uint8_t>(counter - 1);
            if (rate & 0x8000) rate = negate(rate);
            rate = static_cast<uint16_t>((maxRate >> 2) + rate);
            if (hi(previous) & 0x80) return rate;
        }
        return negate(rate);
    }
    const uint16_t flipped = negate(previous);
    if (counter == 0) return flipped;
    *newGain = static_cast<uint8_t>(counter - 1);
    if (param < 0x80) {  // positive
        if (flipped & 0x8000) return flipped;
        return static_cast<uint16_t>((maxRate >> 2) + flipped);
    }
    if (!(flipped & 0x8000)) return flipped;  // negative
    return static_cast<uint16_t>(negate(static_cast<uint16_t>(maxRate >> 2)) + flipped);
}

// UpdateFreqVol: vibrato, tremolo and the registers of a voice, every tick.
void AkaoSequencer::updateFreqVol(int ch) {
    Channel& c = channels_[static_cast<size_t>(ch)];
    const uint8_t bit = static_cast<uint8_t>(1 << ch);
    if (c.vibratoParam != 0 && c.vibratoDelayLeft == 0) {
        const uint16_t before = c.vibratoPhase;
        c.vibratoPhase = static_cast<uint16_t>(before + c.vibratoRateNow);
        if (hi(c.vibratoPhase) != hi(before)) {
            const uint16_t swing = static_cast<uint16_t>(c.vibratoPhase << 1);
            const uint16_t scale = static_cast<uint16_t>(hi(c.basePitch) * 15 + ((lo(c.basePitch) * 15) >> 8));
            unsigned offset = hi(swing) * hi(scale);
            offset += (lo(scale) * hi(swing)) >> 8;
            offset += (lo(swing) * hi(scale)) >> 8;
            if (hi(swing) & 0x80) offset -= scale;
            const uint16_t value = static_cast<uint16_t>(offset);
            if (value != c.vibratoOffset) {
                c.vibratoOffset = value;
                pitchDirty_ |= bit;
            }
        }
        uint8_t left = static_cast<uint8_t>(c.vibratoCycleLeft - 1);
        if (left == 0) {
            uint8_t newGain = 0;
            c.vibratoRateNow = vibratoTremoloRate(c.gain & 7, c.vibratoRate, c.vibratoParam, c.vibratoRateNow, &newGain);
            c.gain = static_cast<uint8_t>(newGain | (c.gain & 0x70));
            left = c.vibratoCycle;
        }
        c.vibratoCycleLeft = left;
    }
    if (c.tremoloParam != 0 && c.tremoloDelayLeft == 0) {
        const uint16_t before = c.tremoloPhase;
        c.tremoloPhase = static_cast<uint16_t>(before + c.tremoloRateNow);
        if (hi(c.tremoloPhase) != hi(before)) volumeDirty_ |= bit;
        uint8_t left = static_cast<uint8_t>(c.tremoloCycleLeft - 1);
        if (left == 0) {
            uint8_t newGain = 0;
            c.tremoloRateNow =
                vibratoTremoloRate((c.gain >> 4) & 7, c.tremoloRate, c.tremoloParam, c.tremoloRateNow, &newGain);
            c.gain = static_cast<uint8_t>((newGain << 4) | (c.gain & 7));
            left = c.tremoloCycle;
        }
        c.tremoloCycleLeft = left;
    }
    if (volumeDirty_ & bit) {
        volumeDirty_ &= static_cast<uint8_t>(~bit);
        updateVolume(ch);
    }
    if (pitchDirty_ & bit) {
        pitchDirty_ &= static_cast<uint8_t>(~bit);
        updatePitch(ch);
    }
}

// UpdateChanVol: the volume registers from the voice volume, the tremolo, the pan and the song volumes.
void AkaoSequencer::updateVolume(int ch) {
    const Channel& c = channels_[static_cast<size_t>(ch)];
    const uint8_t bit = static_cast<uint8_t>(1 << ch);
    const bool system = (systemChannels_ & bit) != 0;
    uint8_t pan = hi(c.pan);
    if (sfx_ && !system) {  // the pan of the game's sound effects moves all of them
        const int moved = pan + static_cast<int8_t>(hi(sfxPan_) ^ 0x80);
        pan = static_cast<uint8_t>(std::max(0, std::min(moved, 0xFF)));
    }
    const uint8_t sweep = hi(c.sweepPhase);
    unsigned panned = pan + sweep;
    uint8_t result = static_cast<uint8_t>(panned);
    if (sweep & 0x80) {
        if (panned < 0x100) result = 0;
    } else if (panned > 0xFF) {
        result = 0xFF;
    }
    const uint8_t weightLeft = system ? 0x80 : static_cast<uint8_t>(result ^ 0xFF);
    const uint8_t weightRight = system ? 0x7F : result;  // a system sound effect is always centered
    unsigned volume = hi(c.volume);
    const uint8_t tremolo = hi(c.tremoloPhase);
    if (tremolo != 0) {
        const bool carry = (tremolo & 0x80) != 0;
        const unsigned scaled = (volume * static_cast<uint8_t>(tremolo << 1)) >> 8;
        if (!carry) {
            volume = (scaled + volume) & 0xFF;
            if (volume & 0x80) volume = 0x7F;
        }
    }
    unsigned level = (volume * hi(songVolume_)) >> 8;
    if (sfx_) {  // the volume of the sound effects instead of the song's; system effects ignore it
        if (system) level = volume * 0xFF >> 8;
    } else if (!(ignoreSongVolume_ & bit)) {
        level = (level * masterVolume_) >> 8;
    }
    regs_[static_cast<size_t>(ch)].volumeLeft = static_cast<uint8_t>((level * weightLeft) >> 8);
    regs_[static_cast<size_t>(ch)].volumeRight = static_cast<uint8_t>((level * weightRight) >> 8);
}

void AkaoSequencer::updatePitch(int ch) {
    const Channel& c = channels_[static_cast<size_t>(ch)];
    const uint16_t pitch = static_cast<uint16_t>(c.basePitch + c.vibratoOffset);
    regs_[static_cast<size_t>(ch)].pitch = sfx_ ? pitch : shiftPitch(pitch);
}

// CalcFreq: the pitch of a note, from its multiplier, the pitch multiplier of the instrument and the detune.
uint16_t AkaoSequencer::calcFreq(const Channel& c, uint8_t note) const {
    const uint8_t octave = note / 12;
    const uint16_t multiplier = kFreqMult[note % 12];
    const unsigned sum = lo(c.instrumentPitch) + c.detune;
    const uint8_t s = static_cast<uint8_t>(sum);
    unsigned pitch = hi(multiplier) * s;
    pitch = (pitch + ((lo(multiplier) * s) >> 8)) & 0xFFFF;
    const uint8_t wave = hi(c.instrumentPitch);
    pitch = wave == 0 ? pitch : ((((wave * (pitch >> 8)) >> 8) + pitch) & 0xFFFF);
    if (!(s & 0x80)) pitch = (pitch + multiplier) & 0xFFFF;
    uint16_t result = static_cast<uint16_t>(pitch);
    uint8_t position = 4;
    if (octave & 0x80) {
        do {
            result = static_cast<uint16_t>(result >> 1);
            --position;
        } while (position != octave);
    } else if (position >= octave) {
        while (position != octave) {
            result = static_cast<uint16_t>(result >> 1);
            --position;
        }
    } else {
        do {
            result = static_cast<uint16_t>(result << 1);
            ++position;
        } while (position != octave);
    }
    return result;
}

void AkaoSequencer::applyAdsr(int ch) {
    regs_[static_cast<size_t>(ch)].adsr1 = channels_[static_cast<size_t>(ch)].adsr1;
    regs_[static_cast<size_t>(ch)].adsr2 = channels_[static_cast<size_t>(ch)].adsr2;
}

// ChangeSample: the pitch multiplier and the envelope of the instrument.
void AkaoSequencer::setInstrument(int ch, uint8_t instrument) {
    Channel& c = channels_[static_cast<size_t>(ch)];
    c.instrument = instrument;
    c.instrumentPitch = 0;
    c.adsr1 = c.adsr2 = 0;
    if (const Sample* sample = sampleOf(instrument)) {
        c.instrumentPitch = sample->freqMult;
        c.adsr1 = sample->adsr1;
        c.adsr2 = sample->adsr2;
    }
    ignoreSongVolume_ &= static_cast<uint8_t>(~(1 << ch));
}

void AkaoSequencer::initVibrato(int ch) {
    Channel& c = channels_[static_cast<size_t>(ch)];
    c.vibratoOffset = 0;
    c.vibratoPhase = 0;
    c.vibratoCycleLeft = c.vibratoCycle;
    uint16_t rate = c.vibratoRate;
    const uint8_t kept = c.gain & 0x70;
    c.vibratoDelayLeft = c.vibratoDelay;
    if (c.vibratoDelay != 0) rate = static_cast<uint16_t>(rate >> 2);
    if (c.vibratoParam >= 0x80 && c.vibratoParam < 0xC0) rate = negate(rate);
    c.gain = c.vibratoDelay != 0 ? static_cast<uint8_t>(kept | 7) : kept;
    c.vibratoRateNow = rate;
}

// The original keeps the gain counter of the vibrato here too (the tremolo bug fix is off).
void AkaoSequencer::initTremolo(int ch) {
    Channel& c = channels_[static_cast<size_t>(ch)];
    c.tremoloPhase = 0;
    c.tremoloCycleLeft = c.tremoloCycle;
    uint16_t rate = c.tremoloRate;
    const uint8_t kept = c.gain & 0x70;
    c.tremoloDelayLeft = c.tremoloDelay;
    if (c.tremoloDelay != 0) rate = static_cast<uint16_t>(rate >> 2);
    if (c.tremoloParam >= 0x80 && c.tremoloParam < 0xC0) rate = negate(rate);
    c.gain = c.tremoloDelay != 0 ? static_cast<uint8_t>(kept | 7) : kept;
    c.tremoloRateNow = rate;
}

void AkaoSequencer::terminate(int ch) {
    activeChannels_ &= static_cast<uint8_t>(~(1 << ch));
    disableEffects(ch);
}

void AkaoSequencer::disableEffects(int ch) {
    const uint8_t keep = static_cast<uint8_t>(~(1 << ch));
    echoChannels_ &= keep;
    noiseChannels_ &= keep;
    pitchModChannels_ &= keep;
    echoVoices_ = echoChannels_;
}

void AkaoSequencer::execScript(int ch) {
    for (;;) {
        const uint8_t byte = fetch(ch);
        if (byte < FF6_AKAO_FIRST_COMMAND) {
            startNote(ch, byte);
            return;
        }
        if (!execCommand(ch, byte)) return;
    }
}

// FindNextNote: the next note code, to know whether the voice is released at the end of this note.
// It follows jumps and loops without changing them.
void AkaoSequencer::findNextNote(int ch, uint8_t* next) {
    Channel& c = channels_[static_cast<size_t>(ch)];
    const uint8_t bit = static_cast<uint8_t>(1 << ch);
    const std::vector<uint8_t>& script = c.sfxScript ? audio_->sfxMemory : *script_;
    const int base = baseOf(c);
    int at = c.pc;
    int top = c.loopTop;
    const auto byteAt = [&](int i) -> uint8_t {
        return i >= 0 && static_cast<size_t>(i) < script.size() ? script[static_cast<size_t>(i)] : FF6_AKAO_END;
    };
    const auto word = [&](int i) { return byteAt(i) | (byteAt(i + 1) << 8); };
    const auto popLoop = [&] { top = (top - 1) & (FF6_AKAO_LOOP_DEPTH - 1); };
    for (int guard = 0; guard < 4096; ++guard) {
        const uint8_t code = byteAt(at);
        if (code < FF6_AKAO_FIRST_COMMAND) {
            *next = code;
            return;
        }
        ++at;
        if (code == FF6_AKAO_END) {
            *next = code;
            return;
        }
        switch (code) {
            case 0xF6: at = word(at) + base; break;
            case 0xE3: {  // loop end
                const uint8_t count = c.loopCount[static_cast<size_t>(top)];
                if (count == 1) {
                    popLoop();
                } else {
                    at = c.loopPc[static_cast<size_t>(top)];
                }
                break;
            }
            case 0xF5: {  // second ending
                if (c.loopRound[static_cast<size_t>(top)] != byteAt(at)) {
                    at += 3;
                } else {
                    if (c.loopCount[static_cast<size_t>(top)] == 1) popLoop();
                    at = word(at + 1) + base;
                }
                break;
            }
            case 0xE5: slur_ &= static_cast<uint8_t>(~bit); break;
            case 0xE7: drumRoll_ &= static_cast<uint8_t>(~bit); break;
            case 0xDC:
                slur_ &= static_cast<uint8_t>(~bit);
                drumRoll_ &= static_cast<uint8_t>(~bit);
                break;  // the instrument number that follows is then read as the next note, like the original
            case 0xE9:
            case 0xEA:
                *next = FF6_AKAO_END;  // the script moves to a sound effect
                return;
            default:
                at += kCommandBytes[code - FF6_AKAO_FIRST_COMMAND];
                break;
        }
    }
    *next = FF6_AKAO_END;
}

void AkaoSequencer::startNote(int ch, uint8_t code) {
    Channel& c = channels_[static_cast<size_t>(ch)];
    const uint8_t bit = static_cast<uint8_t>(1 << ch);
    if (c.duration == 0) c.duration = kNoteDuration[code % FF6_AKAO_NOTE_DIVISOR];
    if (code >= FF6_AKAO_REST_FIRST) return;
    if (code < FF6_AKAO_TIE_FIRST) {
        const uint8_t pitch = static_cast<uint8_t>(c.octave * 12 + code / FF6_AKAO_NOTE_DIVISOR + c.transpose - 10);
        c.notePitch = pitch;
        c.basePitch = calcFreq(c, pitch);
        if (c.vibratoParam) initVibrato(ch);
        if (c.tremoloParam) initTremolo(ch);
        c.vibratoOffset = 0;
        c.tremoloPhase = 0;
        c.glideRate &= 0xFF;  // a new note stops the slide, but the low byte of its rate stays
        playing_ |= bit;
        const bool drum = (drumRoll_ & bit) != 0, slurred = (slur_ & bit) != 0;
        bool start = true;
        if (!drum && slurred) {
            if (slurActive_ & bit) {
                start = false;
            } else {
                slurActive_ |= bit;
            }
        }
        if (start) keyOn_ |= bit;
        volumeDirty_ |= bit;
        pitchDirty_ |= bit;
        regs_[static_cast<size_t>(ch)].instrument = c.instrument;
        applyAdsr(ch);
    }
    uint8_t next = FF6_AKAO_END;
    findNextNote(ch, &next);
    if (next >= FF6_AKAO_REST_FIRST) {
        slur_ &= static_cast<uint8_t>(~bit);
        drumRoll_ &= static_cast<uint8_t>(~bit);
    }
    if (next >= FF6_AKAO_TIE_FIRST && next < FF6_AKAO_REST_FIRST) {
        noKeyOff_ |= bit;
    } else {
        noKeyOff_ &= static_cast<uint8_t>(~bit);
        noKeyOff_ |= static_cast<uint8_t>((slur_ | drumRoll_) & bit);
    }
    if (c.glideSemitones != 0) {  // a slide to another pitch, over the ticks that c8 gave
        c.notePitch = static_cast<uint8_t>(c.notePitch + c.glideSemitones);
        const uint16_t target = calcFreq(c, c.notePitch);
        const int difference = static_cast<int>(target) - static_cast<int>(c.basePitch);
        const bool borrow = difference < 0;
        const unsigned magnitude = static_cast<unsigned>(borrow ? -difference : difference);
        uint32_t rate = 0;
        if (c.glideTicks != 0) rate = static_cast<uint32_t>((static_cast<uint64_t>(magnitude) << 8) / c.glideTicks);
        c.glideRate = borrow ? (0u - rate) & 0xFFFFFF : rate;
        c.glideFraction = 0;
        c.glideSemitones = 0;
    }
}

}  // namespace ff6
