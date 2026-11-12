#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "audio/akao.hpp"
#include "audio/synth.hpp"

namespace {

using test::makeAudio;
using test::prologue;

}  // namespace

void testAkao() {
    {  // a quarter note: the key goes on one tick after the script reads it, and off two script ticks before its end
        std::vector<uint8_t> body = prologue();
        body.insert(body.end(), {0x04, 0xBA, 0xEB});  // C quarter note, quarter rest, end
        const ff6::AudioData audio = makeAudio(body);
        ff6::AkaoSequencer sequencer;
        sequencer.load(audio, 0);
        int keyOnTick = -1, keyOffTick = -1;
        ff6::VoiceRegs regsAtKeyOn;
        int finishedTick = -1;
        for (int tick = 0; tick < 400; ++tick) {
            const ff6::KeyEvents events = sequencer.tick();
            if ((events.keyOn & 1) && keyOnTick < 0) {
                keyOnTick = tick;
                regsAtKeyOn = events.regs[0];
            }
            if ((events.keyOff & 1) && keyOffTick < 0) keyOffTick = tick;
            if (sequencer.finished() && finishedTick < 0) finishedTick = tick;
        }
        CHECK(keyOnTick == 1);
        CHECK(regsAtKeyOn.pitch == 0x0983);  // C of octave 5 on an instrument with the multiplier 1.0
        CHECK(regsAtKeyOn.instrument == 0x20 && regsAtKeyOn.adsr1 == 0xFF && regsAtKeyOn.adsr2 == 0xE0);
        CHECK(regsAtKeyOn.volumeLeft == 62 && regsAtKeyOn.volumeRight == 62);
        // 46 script ticks at 120/256 of a tick each: about 98 ticks.
        CHECK(keyOffTick > 90 && keyOffTick < 106);
        CHECK(finishedTick > 190 && finishedTick < 220);
    }
    {  // a song that starts near the end of the sound memory goes on at the beginning of it: the voice that starts there plays
        std::vector<uint8_t> body(12, 0xEB);  // the voice 0 ends at once
        const std::vector<uint8_t> second = prologue();
        body.insert(body.end(), second.begin(), second.end());
        body.insert(body.end(), {0x04, 0xBA, 0xEB});
        ff6::AudioData audio = makeAudio(body);
        std::vector<uint8_t>& script = audio.songs[0].script;
        const auto put = [&](size_t word, uint16_t value) {
            script[word * 2] = static_cast<uint8_t>(value);
            script[word * 2 + 1] = static_cast<uint8_t>(value >> 8);
        };
        const uint16_t start = 0xFFF8, end = static_cast<uint16_t>(start + body.size());
        put(0, start);
        put(1, end);
        for (size_t i = 0; i < 16; ++i) put(2 + i, i == 0 ? start : i == 1 ? static_cast<uint16_t>(start + 12) : end);
        ff6::AkaoSequencer sequencer;
        sequencer.load(audio, 0);
        int keyOns = 0;
        for (int tick = 0; tick < 400; ++tick) keyOns += (sequencer.tick().keyOn & 2) ? 1 : 0;
        CHECK(keyOns == 1);
    }
    {  // a loop with a count of 3 plays its body three times
        std::vector<uint8_t> body = prologue();
        body.insert(body.end(), {0xE2, 0x02, 0x04, 0xBA, 0xE3, 0xEB});
        const ff6::AudioData audio = makeAudio(body);
        ff6::AkaoSequencer sequencer;
        sequencer.load(audio, 0);
        int keyOns = 0;
        for (int tick = 0; tick < 1000; ++tick) keyOns += (sequencer.tick().keyOn & 1) ? 1 : 0;
        CHECK(keyOns == 3 && sequencer.finished());
    }
    {  // a tie keeps the voice on: one key on for two notes, and the octave of a note shows in its pitch
        std::vector<uint8_t> body = prologue();
        body.insert(body.end(), {0x04, 0xA8 + 4, 0xBA, 0xEB});  // C quarter, a tied quarter, a rest
        const ff6::AudioData audio = makeAudio(body);
        ff6::AkaoSequencer sequencer;
        sequencer.load(audio, 0);
        int keyOns = 0, firstKeyOff = -1;
        for (int tick = 0; tick < 1000; ++tick) {
            const ff6::KeyEvents events = sequencer.tick();
            keyOns += (events.keyOn & 1) ? 1 : 0;
            if ((events.keyOff & 1) && firstKeyOff < 0) firstKeyOff = tick;
        }
        // The voice is released only before the end of the tied note, about 94 script ticks in.
        CHECK(keyOns == 1 && firstKeyOff > 190 && firstKeyOff < 215);
    }
    {  // the voices make sound: a note is heard, then silence once it is released
        std::vector<uint8_t> body = prologue();
        body.insert(body.end(), {0x04, 0xBA, 0xEB});
        const ff6::AudioData audio = makeAudio(body);
        ff6::Synth synth(audio);
        CHECK(!synth.playing());
        synth.playSong(0);
        std::vector<int16_t> out(2 * FF6_DSP_SAMPLE_RATE);
        synth.render(out.data(), out.size() / 2);
        int peak = 0;
        for (int16_t sample : out) peak = peak < sample ? sample : peak;
        CHECK(peak > 1000);
        CHECK(!synth.playing());
    }
    {  // the echo returns what the voices feed it: with it, the first half second carries more energy
        const auto energy = [&](bool echo) {
            std::vector<uint8_t> body = prologue();
            body.insert(body.end(), {0xF2, 0x7F, 0xF8, 0x00, 0x00});  // echo volume, filter 0 (a plain delay)
            if (echo) body.push_back(0xD4);
            body.insert(body.end(), {0x04, 0xBA, 0xEB});
            const ff6::AudioData audio = makeAudio(body);
            ff6::Synth synth(audio);
            synth.setEchoDelay(1);
            synth.playSong(0);
            std::vector<int16_t> out(FF6_DSP_SAMPLE_RATE);
            synth.render(out.data(), out.size() / 2);
            double sum = 0;
            for (int16_t sample : out) sum += static_cast<double>(sample) * sample;
            return sum;
        };
        CHECK(energy(true) > 1.3 * energy(false));
    }
    {  // noise changes the sound of a voice, and a delay of 0 or 15 is accepted
        const auto render = [&](bool noise) {
            std::vector<uint8_t> body = prologue();
            if (noise) body.insert(body.end(), {0xCF, 0x08, 0xD0});
            body.insert(body.end(), {0x04, 0xBA, 0xEB});
            const ff6::AudioData audio = makeAudio(body);
            ff6::Synth synth(audio);
            synth.setEchoDelay(0);
            synth.setEchoDelay(15);
            synth.playSong(0);
            std::vector<int16_t> out(FF6_DSP_SAMPLE_RATE / 2);
            synth.render(out.data(), out.size() / 2);
            return out;
        };
        const std::vector<int16_t> clean = render(false), noisy = render(true);
        int different = 0;
        for (size_t i = 0; i < clean.size(); ++i) different += clean[i] != noisy[i] ? 1 : 0;
        CHECK(different > 1000);
    }
    {  // the game changes the pitch of the song: 0x7F is about an octave up and 0x80 an octave down
        std::vector<uint8_t> body = prologue();
        body.insert(body.end(), {0x04, 0xBA, 0xEB});
        const ff6::AudioData audio = makeAudio(body);
        const auto pitchWith = [&](uint8_t shift) {
            ff6::AkaoSequencer sequencer;
            sequencer.load(audio, 0);
            sequencer.setPitchShift(0, shift);
            for (int tick = 0; tick < 10; ++tick) {
                const ff6::KeyEvents events = sequencer.tick();
                if (events.keyOn & 1) return static_cast<int>(events.regs[0].pitch);
            }
            return -1;
        };
        CHECK(pitchWith(0) == 0x0983);
        CHECK(pitchWith(0x7F) == 2 * 1207 + 0x0983);
        CHECK(pitchWith(0x80) == 1217);
    }
    {  // the game changes the tempo of the song: 0x7F plays it about twice as fast, 0x80 half as fast
        const auto finishTick = [](uint8_t ratio) {
            std::vector<uint8_t> body = prologue();
            body.insert(body.end(), {0x04, 0xBA, 0xEB});
            const ff6::AudioData audio = makeAudio(body);
            ff6::AkaoSequencer sequencer;
            sequencer.load(audio, 0);
            sequencer.setTempoRatio(0, ratio);
            for (int tick = 0; tick < 1000; ++tick) {
                sequencer.tick();
                if (sequencer.finished()) return tick;
            }
            return -1;
        };
        const int normal = finishTick(0), fast = finishTick(0x7F), slow = finishTick(0x80);
        CHECK(normal > 190 && fast > 0 && slow > 0);
        CHECK(fast < normal * 6 / 10 && slow > normal * 17 / 10);
    }
    {  // the tempo and the pitch that the game set stay when another song is loaded
        std::vector<uint8_t> body = prologue();
        body.insert(body.end(), {0x04, 0xBA, 0xEB});
        const ff6::AudioData audio = makeAudio(body);
        ff6::AkaoSequencer sequencer;
        sequencer.load(audio, 0);
        sequencer.setTempoRatio(0, 0x7F);
        sequencer.setPitchShift(0, 0x7F);
        sequencer.load(audio, 0);
        int finished = -1;
        int pitch = -1;
        for (int tick = 0; tick < 1000 && finished < 0; ++tick) {
            const ff6::KeyEvents events = sequencer.tick();
            if ((events.keyOn & 1) && pitch < 0) pitch = events.regs[0].pitch;
            if (sequencer.finished()) finished = tick;
        }
        CHECK(finished > 0 && finished < 140 && pitch == 2 * 1207 + 0x0983);
    }
    {  // E9 and EA: the voice runs the script of a game sound effect and the song does not come back to it
        std::vector<uint8_t> body = prologue();
        body.insert(body.end(), {0xE9, 0x00, 0x04, 0xBA, 0xEB});  // the effect has a script for the first voice only
        const ff6::AudioData audio = makeAudio(body);
        ff6::AkaoSequencer sequencer;
        sequencer.load(audio, 0);
        int finished = -1;
        int instrument = -1;
        for (int tick = 0; tick < 1000 && finished < 0; ++tick) {
            const ff6::KeyEvents events = sequencer.tick();
            if ((events.keyOn & 1) && instrument < 0) instrument = events.regs[0].instrument;
            if (sequencer.finished()) finished = tick;
        }
        // The note of the effect uses its instrument 0, then its script ends: there is no rest after it.
        CHECK(instrument == 0 && finished > 90 && finished < 130);
        std::vector<uint8_t> second = prologue();
        second.insert(second.end(), {0xEA, 0x00, 0x04, 0xBA, 0xEB});  // no script for the second voice: it ends
        const ff6::AudioData none = makeAudio(second);
        ff6::AkaoSequencer ended;
        ended.load(none, 0);
        bool keyed = false;
        for (int tick = 0; tick < 100; ++tick) keyed = keyed || (ended.tick().keyOn & 1) != 0;
        CHECK(ended.finished() && !keyed);
    }
}
