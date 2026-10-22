#include <vector>

#include "../check.hpp"
#include "../tests.hpp"
#include "audio/dsp_voice.hpp"
#include "audio/gauss_table.hpp"

namespace {

// BRR blocks of constant samples (range 12, nibble 7: 28672 at full scale), with the given flags.
std::vector<uint8_t> constantBlocks(int count, uint8_t lastFlags) {
    std::vector<uint8_t> brr;
    for (int block = 0; block < count; ++block) {
        brr.push_back(static_cast<uint8_t>(0xC0 | (block == count - 1 ? lastFlags : 0)));
        for (int i = 0; i < 8; ++i) brr.push_back(0x77);
    }
    return brr;
}

int ticksUntilSilent(ff6::DspVoice& voice, int limit) {
    int32_t left = 0, right = 0;
    for (int tick = 0; tick < limit; ++tick) {
        if (!voice.active()) return tick;
        voice.render(static_cast<uint32_t>(tick), &left, &right);
    }
    return limit;
}

}  // namespace

void testDspVoice() {
    {  // a looping sample at full envelope plays a steady level, scaled by the channel volume
        const std::vector<uint8_t> brr = constantBlocks(1, FF6_BRR_FLAG_END | FF6_BRR_FLAG_LOOP);
        ff6::DspVoice voice;
        voice.keyOn(brr.data(), brr.size(), 0, 0xFF, 0xE0);
        voice.setVolume(100, -50);
        int32_t left = 0, right = 0;
        for (uint32_t tick = 0; tick < 100; ++tick) {
            left = right = 0;
            voice.render(tick, &left, &right);
        }
        CHECK(voice.active() && voice.envelope() > 0x7F0 && voice.envelope() <= FF6_DSP_ENV_MAX);  // one decay step on the way
        CHECK(left > 22000 && left < 22600);  // about 28672 * 100 / 128
        CHECK(right < -11000 && right > -11300);
    }
    {  // the first sample is silent: the envelope starts at zero
        const std::vector<uint8_t> brr = constantBlocks(1, FF6_BRR_FLAG_END | FF6_BRR_FLAG_LOOP);
        ff6::DspVoice voice;
        voice.keyOn(brr.data(), brr.size(), 0, 0xFF, 0xE0);
        voice.setVolume(127, 127);
        int32_t left = 0, right = 0;
        voice.render(0, &left, &right);
        CHECK(left == 0 && right == 0);
    }
    {  // releasing ends the voice, 8 envelope steps per sample
        const std::vector<uint8_t> brr = constantBlocks(1, FF6_BRR_FLAG_END | FF6_BRR_FLAG_LOOP);
        ff6::DspVoice voice;
        voice.keyOn(brr.data(), brr.size(), 0, 0xFF, 0xE0);
        int32_t left = 0, right = 0;
        for (uint32_t tick = 0; tick < 50; ++tick) voice.render(tick, &left, &right);
        voice.keyOff();
        const int ticks = ticksUntilSilent(voice, 1000);
        CHECK(ticks >= FF6_DSP_ENV_MAX / FF6_DSP_ENV_RELEASE_STEP - 2 && ticks <= FF6_DSP_ENV_MAX / FF6_DSP_ENV_RELEASE_STEP + 2);
    }
    {  // a sample without a loop stops by itself, and a higher pitch gets there sooner
        const std::vector<uint8_t> brr = constantBlocks(2, FF6_BRR_FLAG_END);
        ff6::DspVoice slow, fast;
        slow.keyOn(brr.data(), brr.size(), 0, 0xFF, 0xE0);
        fast.keyOn(brr.data(), brr.size(), 0, 0xFF, 0xE0);
        fast.setPitch(2 * FF6_DSP_PITCH_ONE);
        const int slowTicks = ticksUntilSilent(slow, 1000);
        const int fastTicks = ticksUntilSilent(fast, 1000);
        CHECK(slowTicks > 20 && slowTicks < 40);
        CHECK(fastTicks * 2 >= slowTicks - 4 && fastTicks * 2 <= slowTicks + 4);
    }
    {  // a slow attack climbs 32 steps per rate period
        const std::vector<uint8_t> brr = constantBlocks(1, FF6_BRR_FLAG_END | FF6_BRR_FLAG_LOOP);
        ff6::DspVoice voice;
        voice.keyOn(brr.data(), brr.size(), 0, 0x80, 0xE0);  // attack rate 0: a step every 2048 samples
        int32_t left = 0, right = 0;
        for (uint32_t tick = 1; tick <= 4096; ++tick) voice.render(tick, &left, &right);
        CHECK(voice.envelope() == 2 * FF6_DSP_ATTACK_STEP);
    }
    {  // the four weights of the gaussian interpolation add up to full scale at every position
        bool exact = true;
        for (int offset = 0; offset < 256; ++offset) {
            const int sum = ff6::akao::kGauss[255 - offset] + ff6::akao::kGauss[511 - offset] +
                            ff6::akao::kGauss[256 + offset] + ff6::akao::kGauss[offset];
            if (sum < 2047 || sum > 2049) exact = false;
        }
        CHECK(exact);
    }
    {  // the steps of the envelope: some rates are shifted by an offset of the counter, rate 0 never steps
        CHECK(ff6::DspVoice::rateFires(2, 496) && !ff6::DspVoice::rateFires(2, 0));
        CHECK(ff6::DspVoice::rateFires(31, 12345) && ff6::DspVoice::rateFires(1, 2048));
        bool never = true;
        for (uint32_t counter = 0; counter < FF6_DSP_COUNTER_RANGE; ++counter) never = never && !ff6::DspVoice::rateFires(0, counter);
        CHECK(never);
    }
    {  // noise replaces the sample, at the level of the envelope
        const std::vector<uint8_t> brr = constantBlocks(1, FF6_BRR_FLAG_END | FF6_BRR_FLAG_LOOP);
        ff6::DspVoice voice;
        voice.keyOn(brr.data(), brr.size(), 0, 0xFF, 0xE0);
        voice.setVolume(100, 100);
        ff6::DspInputs inputs;
        inputs.noisy = true;
        inputs.noise = 0x1000;
        int32_t left = 0, right = 0;
        for (uint32_t tick = 0; tick < 50; ++tick) {
            inputs.counter = tick;
            left = right = 0;
            voice.render(inputs, &left, &right);
        }
        CHECK(left > 6000 && left < 6500);  // 0x2000 at full envelope, times 100 / 128
    }
    {  // pitch modulation: a loud positive neighbour makes the source run faster, a negative one slower
        const std::vector<uint8_t> brr = constantBlocks(2, FF6_BRR_FLAG_END);
        const auto ticksFor = [&](int previousOutput) {
            ff6::DspVoice voice;
            voice.keyOn(brr.data(), brr.size(), 0, 0xFF, 0xE0);
            ff6::DspInputs inputs;
            inputs.modulated = true;
            inputs.previousOutput = previousOutput;
            int32_t left = 0, right = 0;
            int ticks = 0;
            while (voice.active() && ticks < 1000) {
                inputs.counter = static_cast<uint32_t>(ticks++);
                voice.render(inputs, &left, &right);
            }
            return ticks;
        };
        CHECK(ticksFor(20000) < ticksFor(0) && ticksFor(-20000) > ticksFor(0));
    }
    {  // keying on an empty sample leaves the voice silent
        ff6::DspVoice voice;
        voice.keyOn(nullptr, 0, 0, 0xFF, 0xE0);
        CHECK(!voice.active());
    }
}
