// The script commands of the music channels (ScriptCmd_* of src/spc/spc_main.asm).
#include <algorithm>

#include "audio/akao.hpp"
#include "audio/akao_tables.hpp"

namespace ff6 {
namespace {

using akao::envelopeRate;
using akao::kCommandBytes;
using akao::kFilter;

uint8_t hi(uint16_t value) { return static_cast<uint8_t>(value >> 8); }

int8_t half(uint8_t value) { return static_cast<int8_t>(static_cast<int8_t>(value) >> 1); }

}  // namespace

// CalcVibratoRate: how fast the amplitude of a vibrato or tremolo grows over its cycle.
uint16_t AkaoSequencer::calcVibratoRate(uint8_t param, uint8_t cycle) {
    const unsigned amplitude = (param & 0x3Fu) + 1;
    unsigned rate = cycle != 0 ? (amplitude * 256) / cycle : amplitude;
    rate &= 0xFFFF;
    rate >>= 2;
    if (rate == 0) rate = 1;
    return static_cast<uint16_t>(rate << 2);
}

// Returns false when the channel ends.
bool AkaoSequencer::execCommand(int ch, uint8_t cmd) {
    Channel& c = channels_[static_cast<size_t>(ch)];
    const uint8_t bit = static_cast<uint8_t>(1 << ch);
    const int count = kCommandBytes[cmd - FF6_AKAO_FIRST_COMMAND];
    const uint8_t a = count > 0 ? fetch(ch) : 0;  // the first parameter
    switch (cmd) {
        case 0xC4:  // voice volume
            c.volume = static_cast<uint16_t>((a & 0x7F) << 8);
            c.volumeTicks = 0;
            volumeDirty_ |= bit;
            break;
        case 0xC5: {  // voice volume slide
            const uint8_t target = fetch(ch) & 0x7F;
            if (a == 0) {
                c.volume = static_cast<uint16_t>(target << 8);
                volumeDirty_ |= bit;
                c.volumeTicks = 0;
            } else if (target == hi(c.volume)) {
                c.volumeTicks = 0;
            } else {
                c.volumeTicks = a;
                c.volumeRate = envelopeRate(target, hi(c.volume), a);
            }
            break;
        }
        case 0xC6:  // pan
            c.pan = static_cast<uint16_t>(static_cast<uint8_t>(a << 1) << 8);
            c.panTicks = 0;
            volumeDirty_ |= bit;
            break;
        case 0xC7: {  // pan slide
            const uint8_t target = static_cast<uint8_t>(fetch(ch) << 1);
            if (a == 0) {
                c.pan = static_cast<uint16_t>(target << 8);
                volumeDirty_ |= bit;
                c.panTicks = 0;
            } else if (target == hi(c.pan)) {
                c.panTicks = 0;
            } else {
                c.panTicks = a;
                c.panRate = envelopeRate(target, hi(c.pan), a);
            }
            break;
        }
        case 0xC8:  // slide to another pitch, for the next note
            c.glideTicks = static_cast<uint8_t>(a + 1);
            c.glideSemitones = fetch(ch);
            break;
        case 0xC9:  // vibrato
            c.vibratoDelay = a;
            c.vibratoCycle = fetch(ch);
            c.vibratoParam = fetch(ch);
            c.vibratoRate = calcVibratoRate(c.vibratoParam, c.vibratoCycle);
            initVibrato(ch);
            break;
        case 0xCA:
            c.vibratoParam = 0;
            c.vibratoOffset = 0;
            break;
        case 0xCB:  // tremolo
            c.tremoloDelay = a;
            c.tremoloCycle = fetch(ch);
            c.tremoloParam = fetch(ch);
            c.tremoloRate = calcVibratoRate(c.tremoloParam, c.tremoloCycle);
            initTremolo(ch);
            break;
        case 0xCC:
            c.tremoloParam = 0;
            c.tremoloPhase = 0;
            break;
        case 0xCD: {  // pan sweep: a period in ticks, then the amplitude with its direction in the top bit
            uint8_t period = static_cast<uint8_t>(a + 1);
            bool carry = false;
            if (period == 0) {
                carry = true;
            } else {
                period &= 0xFE;
                if (period == 0) period = 2;
            }
            const uint8_t counter = static_cast<uint8_t>((period >> 1) | (carry ? 0x80 : 0));
            const uint8_t amplitude = fetch(ch);
            uint16_t magnitude = amplitude & 0x7F;
            if (amplitude & 0x80) magnitude ^= 0x7F;
            uint16_t step;
            if (counter & 0x80) {
                step = magnitude;
            } else {
                step = static_cast<uint16_t>((magnitude * 256) / counter);
                if (step == 0) step = 1;
            }
            if (amplitude & 0x80) step = static_cast<uint16_t>(0 - step);
            c.sweepPeriod = period;
            c.sweepLeft = counter;
            c.sweepStep = step;
            c.sweepParam = amplitude;
            c.sweepPhase = 0;
            break;
        }
        case 0xCE:
            c.sweepParam = 0;
            c.sweepPhase = 0;
            break;
        case 0xCF: noiseClock_ = a & 0x1F; break;
        case 0xD0: noiseChannels_ |= bit; break;
        case 0xD1: noiseChannels_ &= static_cast<uint8_t>(~bit); break;
        case 0xD2: pitchModChannels_ |= bit; break;
        case 0xD3: pitchModChannels_ &= static_cast<uint8_t>(~bit); break;
        case 0xD4:
            echoChannels_ |= bit;
            echoVoices_ = echoChannels_;
            break;
        case 0xD5:
            echoChannels_ &= static_cast<uint8_t>(~bit);
            echoVoices_ = echoChannels_;
            break;
        case 0xD6: c.octave = a; break;
        case 0xD7: ++c.octave; break;
        case 0xD8: --c.octave; break;
        case 0xD9: c.transpose = a; break;
        case 0xDA: c.transpose = static_cast<uint8_t>(c.transpose + a); break;
        case 0xDB: c.detune = a; break;
        case 0xDC: setInstrument(ch, a); break;
        case 0xDD:  // attack
            c.adsr1 = static_cast<uint8_t>((c.adsr1 & 0x70) | (a & 0x0F) | 0x80);
            applyAdsr(ch);
            break;
        case 0xDE:  // decay
            c.adsr1 = static_cast<uint8_t>((c.adsr1 & 0x0F) | ((a & 7) << 4) | 0x80);
            applyAdsr(ch);
            break;
        case 0xDF:  // sustain level
            c.adsr2 = static_cast<uint8_t>((c.adsr2 & 0x1F) | ((a & 7) << 5));
            applyAdsr(ch);
            break;
        case 0xE0:  // sustain rate, which the script calls release
            c.adsr2 = static_cast<uint8_t>((c.adsr2 & 0xE0) | (a & 0x1F));
            applyAdsr(ch);
            break;
        case 0xE1: {  // the envelope of the instrument again
            if (const Sample* sample = sampleOf(c.instrument)) {
                c.adsr1 = sample->adsr1;
                c.adsr2 = sample->adsr2;
            }
            applyAdsr(ch);
            break;
        }
        case 0xE2: {  // loop start
            c.loopTop = (c.loopTop + 1) & (FF6_AKAO_LOOP_DEPTH - 1);
            const size_t top = static_cast<size_t>(c.loopTop);
            c.loopCount[top] = a == 0 ? 0 : static_cast<uint8_t>(a + 1);
            c.loopRound[top] = 1;
            c.loopPc[top] = c.pc;
            break;
        }
        case 0xE3: {  // loop end
            const size_t top = static_cast<size_t>(c.loopTop);
            ++c.loopRound[top];
            const uint8_t left = c.loopCount[top];
            if (left == 0 || left != 1) {
                if (left != 0) c.loopCount[top] = static_cast<uint8_t>(left - 1);
                c.pc = c.loopPc[top];
            } else {
                c.loopTop = (c.loopTop - 1) & (FF6_AKAO_LOOP_DEPTH - 1);
            }
            break;
        }
        case 0xE4:
            slur_ |= bit;
            slurActive_ &= static_cast<uint8_t>(~bit);
            drumRoll_ &= static_cast<uint8_t>(~bit);
            break;
        case 0xE6:
            drumRoll_ |= bit;
            slur_ &= static_cast<uint8_t>(~bit);
            break;
        case 0xE8: c.duration = a; break;
        case 0xF0:
            tempo_ = static_cast<uint16_t>(a << 8);
            tempoTicks_ = 0;
            break;
        case 0xF1: {  // tempo slide
            const uint8_t target = fetch(ch);
            if (a == 0) {
                tempo_ = static_cast<uint16_t>(target << 8);
                tempoTicks_ = 0;
            } else if (target == hi(tempo_)) {
                tempoTicks_ = 0;
            } else {
                tempoTicks_ = a;
                tempoRate_ = envelopeRate(target, hi(tempo_), a);
            }
            break;
        }
        case 0xF2: {  // echo volume
            echoVolume_ = static_cast<uint16_t>(static_cast<uint8_t>(half(a)) << 8);
            echoVolumeTicks_ = 0;
            break;
        }
        case 0xF3: {  // echo volume slide
            const uint8_t target = fetch(ch);
            if (a == 0) {
                echoVolume_ = static_cast<uint16_t>(static_cast<uint8_t>(half(target)) << 8);
                echoVolumeTicks_ = 0;
            } else {
                const int now = static_cast<int8_t>(hi(echoVolume_));
                if (half(target) == now) {
                    echoVolumeTicks_ = 0;
                } else {
                    echoVolumeTicks_ = a;
                    echoVolumeRate_ = envelopeRate(half(target), now, a);
                }
            }
            break;
        }
        case 0xF4: masterVolume_ = a; break;
        case 0xF5: {  // jump to the second ending
            const int low = fetch(ch);
            const int target = resolve(c, low | (fetch(ch) << 8));
            const size_t top = static_cast<size_t>(c.loopTop);
            if (c.loopRound[top] == a) {
                if (c.loopCount[top] == 1) c.loopTop = (c.loopTop - 1) & (FF6_AKAO_LOOP_DEPTH - 1);
                c.pc = target;
            }
            break;
        }
        case 0xF6: {
            const int target = resolve(c, a | (fetch(ch) << 8));
            c.pc = target;
            break;
        }
        case 0xF7: {  // echo feedback
            const uint8_t target = fetch(ch);
            if (a == 0) {
                feedback_ = static_cast<uint16_t>((feedback_ & 0xFF) | (target << 8));
                feedbackTicks_ = 0;
            } else {
                feedbackTicks_ = a;
                feedbackRate_ = envelopeRate(static_cast<int8_t>(target), static_cast<int8_t>(hi(feedback_)), a);
            }
            break;
        }
        case 0xF8: {  // echo filter
            const unsigned filter = fetch(ch) & 3u;
            filterTicks_ = a;
            for (size_t i = 0; i < fir_.size(); ++i) {
                const uint8_t target = kFilter[filter][i];
                if (a == 0) {
                    fir_[i] = static_cast<uint16_t>((fir_[i] & 0xFF) | (target << 8));
                } else {
                    fir_[i] &= 0xFF00;
                    firRate_[i] = envelopeRate(static_cast<int8_t>(target), static_cast<int8_t>(hi(fir_[i])), a);
                }
            }
            break;
        }
        case 0xF9: ++outputCode_; break;
        case 0xFA: outputCode_ = 0; break;
        case 0xFB: ignoreSongVolume_ |= bit; break;
        case 0xFC: {  // a jump that the game asks for with the continue command
            const int target = resolve(c, a | (fetch(ch) << 8));
            if (conditionalJump_ & bit) {
                conditionalJump_ &= static_cast<uint8_t>(~bit);
                c.pc = target;
            }
            break;
        }
        case 0xE9:  // the voice runs the script of a game sound effect, and the song does not come back to it
        case 0xEA: {
            const std::vector<uint8_t>& memory = audio_->sfxMemory;
            const size_t at = FF6_SFX_POINTERS + static_cast<size_t>(a) * FF6_SFX_POINTER_BYTES + (cmd == 0xEA ? 2 : 0);
            const uint16_t address = static_cast<uint16_t>(memory[at] | (memory[at + 1] << 8));
            if ((address >> 8) == 0) {  // the effect has no script for this voice
                terminate(ch);
                return false;
            }
            c.pc = address;
            c.sfxScript = true;
            break;
        }
        case 0xE5:
        case 0xE7:
            break;
        default:  // end of the script, and the codes that end it
            terminate(ch);
            return false;
    }
    return true;
}

}  // namespace ff6
