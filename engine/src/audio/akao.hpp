#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "audio/audio_data.hpp"

namespace ff6 {

// What the music script asks of one voice of the sound chip.
struct VoiceRegs {
    uint16_t pitch = 0;
    uint8_t volumeLeft = 0, volumeRight = 0;
    uint8_t instrument = 0;  // 0x20 + the instrument of the song
    uint8_t adsr1 = 0, adsr2 = 0;
};

// Key on and key off requests, and the registers of the voices when they were made.
struct KeyEvents {
    uint8_t keyOn = 0, keyOff = 0;  // one bit per voice
    std::array<VoiceRegs, FF6_AKAO_VOICES> regs;
};

// Port of the music part of the sound program (src/spc/spc_main.asm): eight channels of script that run on a clock
// of FF6_AKAO_TICK_SAMPLES samples and drive the eight voices of the sound chip. Sound effect channels are not here.
class AkaoSequencer {
public:
    void load(const AudioData& audio, size_t song, uint8_t songVolume = 0xFF, bool alternateStart = false);
    // The sound effects use a sequencer of their own: its channels are the voices that they take from the music, its
    // scripts are read from the sound memory by address and its song volume is the volume of the sound effects.
    void loadSfx(const AudioData& audio);
    void startSfx(int voice, uint16_t address, bool system);
    void stopSfx(int voice);
    void setSfxPan(int duration, uint8_t target);
    bool takeSfxStop() {
        const bool requested = sfxStopRequested_;
        sfxStopRequested_ = false;
        return requested;
    }
    // The instrument number of a script: 0 to 7 for the sound effects, 0x20 and more for those of the song.
    const Sample* sampleOf(uint8_t instrument) const;
    // What the game asks of the song: its volume (0x80, 0x81), and the jump of the continue command (0x89).
    void setSongVolume(int duration, uint8_t target);
    void setTempoRatio(int duration, uint8_t target);  // 0x80 is half, 0x00 the tempo of the song, 0x7F twice
    void setPitchShift(int duration, uint8_t target);  // 0x80 is an octave down, 0x00 none, 0x7F an octave up
    void keepGameSettings(const AkaoSequencer& other);
    void continueSong() { conditionalJump_ = activeChannels_; }
    // What the sound program tells the game: the output code (port 1), the channels that still wait for the continue
    // command (port 2) and the channels that play (port 3).
    uint8_t outputCode() const { return outputCode_; }
    uint8_t conditionalJump() const { return conditionalJump_; }
    uint8_t activeChannels() const { return activeChannels_; }
    uint8_t systemChannels() const { return systemChannels_; }
    // Runs one tick of 4.875 ms. The key events are those that the previous tick left for the chip.
    KeyEvents tick();
    bool finished() const { return activeChannels_ == 0; }
    const std::array<VoiceRegs, FF6_AKAO_VOICES>& regs() const { return regs_; }

    // The echo unit of the chip as the script sets it.
    uint8_t echoVolume() const { return static_cast<uint8_t>(echoVolume_ >> 8); }
    uint8_t echoFeedback() const { return static_cast<uint8_t>(feedback_ >> 8); }
    uint8_t echoVoices() const { return echoVoices_; }
    int8_t firCoefficient(int i) const { return static_cast<int8_t>(fir_[static_cast<size_t>(i)] >> 8); }
    // The noise and pitch modulation registers, and the noise rate of the chip.
    uint8_t noiseVoices() const { return noiseChannels_; }
    uint8_t pitchModVoices() const { return pitchModChannels_; }
    uint8_t noiseClock() const { return noiseClock_; }
    // SetEchoDelay: with no channel playing, the echo volume goes back to zero.
    void echoDelayChanged() { if (activeChannels_ == 0) echoVolume_ = 0; }

private:
    struct Channel {
        int pc = 0;  // index into the script
        bool sfxScript = false;  // the voice runs a sound effect script, read from the sound memory by address
        int duration = 0;  // ticks left in the note
        int loopTop = 0;  // index of the loop stack in use
        uint8_t octave = 0, instrument = 0;
        uint16_t volume = 0, volumeRate = 0, pan = 0, panRate = 0;  // 8.8 fixed point, the high byte is the value
        uint8_t volumeTicks = 0, panTicks = 0;
        uint8_t vibratoDelay = 0, vibratoDelayLeft = 0, vibratoCycle = 0, vibratoCycleLeft = 0, vibratoParam = 0;
        uint16_t vibratoRate = 0, vibratoRateNow = 0, vibratoPhase = 0, vibratoOffset = 0;
        uint8_t tremoloDelay = 0, tremoloDelayLeft = 0, tremoloCycle = 0, tremoloCycleLeft = 0, tremoloParam = 0;
        uint16_t tremoloRate = 0, tremoloRateNow = 0, tremoloPhase = 0;
        uint8_t gain = 0;  // vibrato in the low 3 bits, tremolo in bits 4-6
        uint8_t sweepParam = 0, sweepPeriod = 0, sweepLeft = 0;
        uint16_t sweepStep = 0, sweepPhase = 0;
        uint8_t glideSemitones = 0, glideTicks = 0, glideFraction = 0;
        uint32_t glideRate = 0;  // 8.16 fixed point per tick
        uint16_t basePitch = 0;
        uint8_t transpose = 0, detune = 0, notePitch = 0;
        uint16_t instrumentPitch = 0;  // pitch multiplier of the instrument
        uint8_t adsr1 = 0, adsr2 = 0;
        std::array<uint8_t, 4> loopCount{}, loopRound{};
        std::array<int, 4> loopPc{};
    };

    // Channel commands, tick by tick.
    void scriptTick();
    void songEnvelopes();
    void updateInterruptEnvelopes();
    static void slide(uint16_t* value, uint16_t* rate, uint8_t* ticks, int duration, uint8_t target);
    uint8_t effectiveTempo() const;
    uint16_t shiftPitch(uint16_t pitch) const;
    void channelEnvelopes(int ch);
    void execScript(int ch);
    bool execCommand(int ch, uint8_t cmd);
    void startNote(int ch, uint8_t note);
    void findNextNote(int ch, uint8_t* next);
    void updateFreqVol(int ch);
    void updateVolume(int ch);
    void updatePitch(int ch);
    void setInstrument(int ch, uint8_t instrument);
    void applyAdsr(int ch);
    void terminate(int ch);
    uint8_t fetch(int ch);
    int baseOf(const Channel& c) const;
    uint16_t calcFreq(const Channel& c, uint8_t note) const;
    static uint16_t calcVibratoRate(uint8_t param, uint8_t cycle);
    void initVibrato(int ch);
    void initTremolo(int ch);
    uint16_t vibratoTremoloRate(uint8_t counter, uint16_t maxRate, uint8_t param, uint16_t previous, uint8_t* newGain) const;
    void disableEffects(int ch);

    const AudioData* audio_ = nullptr;
    const Song* song_ = nullptr;
    const std::vector<uint8_t>* script_ = nullptr;  // the song script, or the sound memory for the sound effects
    bool sfx_ = false;
    bool sfxStopRequested_ = false;
    uint8_t systemChannels_ = 0;  // the sound effects of the system, among the channels that play
    uint16_t sfxPan_ = FF6_SFX_PAN_CENTER << 8, sfxPanRate_ = 0;
    uint8_t sfxPanTicks_ = 0;
    uint16_t tempoRatio_ = 0, tempoRatioRate_ = 0, pitchShift_ = 0, pitchShiftRate_ = 0;  // 8.8, the high byte is the value
    uint8_t tempoRatioTicks_ = 0, pitchShiftTicks_ = 0;
    int base_ = 0;  // script address + base_ = index into the script
    std::array<Channel, FF6_AKAO_VOICES> channels_;
    std::array<VoiceRegs, FF6_AKAO_VOICES> regs_;
    uint8_t activeChannels_ = 0;  // one bit per channel
    uint8_t keyOn_ = 0, keyOff_ = 0;
    uint8_t playing_ = 0;  // channels whose note is sounding (vibrato and volume updates run for them)
    uint8_t noKeyOff_ = 0, slur_ = 0, slurActive_ = 0, drumRoll_ = 0, ignoreSongVolume_ = 0;
    uint8_t volumeDirty_ = 0, pitchDirty_ = 0;
    uint16_t songVolume_ = 0xFF00, songVolumeRate_ = 0;  // 8.8, the high byte is the volume
    uint8_t songVolumeTicks_ = 0, interruptCounter_ = FF6_AKAO_INTERRUPT_ENV_TICKS, conditionalJump_ = 0;
    uint8_t masterVolume_ = 0xFF;
    uint16_t tempo_ = 0, tempoRate_ = 0;  // 8.8, the high byte is the tempo
    uint8_t tempoTicks_ = 0, tempoAccumulator_ = 0;
    uint16_t echoVolume_ = 0, echoVolumeRate_ = 0, feedback_ = 0, feedbackRate_ = 0;
    uint8_t echoVolumeTicks_ = 0, feedbackTicks_ = 0, filterTicks_ = 0;
    std::array<uint16_t, 8> fir_{}, firRate_{};
    uint8_t echoChannels_ = 0, echoVoices_ = 0, noiseChannels_ = 0, pitchModChannels_ = 0;
    uint8_t noiseClock_ = 0;
    uint8_t outputCode_ = 0;
};

}  // namespace ff6
