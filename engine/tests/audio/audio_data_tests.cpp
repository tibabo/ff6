#include <cstdlib>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "audio/audio_data.hpp"

void testAudioData() {
    CHECK(test::throws([] { ff6::loadAudioData("missing-pack-directory"); }));
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::AudioData audio = ff6::loadAudioData(pack);
    CHECK(audio.samples.size() == 63 && audio.songs.size() == 85);
    // Sample 0 is the steel guitar, with its loop at 0x0b88 and the first envelope of the table.
    CHECK(audio.samples[0].loopStart == 0x0b88 && audio.samples[0].freqMult == 0xa0fd);
    CHECK(audio.samples[0].adsr1 == 0xff && audio.samples[0].adsr2 == 0xf1);
    for (const ff6::Sample& sample : audio.samples) CHECK(!sample.brr.empty() && sample.brr.size() % FF6_BRR_BLOCK_BYTES == 0);
    // The prelude (song 1) uses the harp, the flute, the strings, the synth voice and the contrabass.
    const ff6::Song& prelude = audio.songs[1];
    CHECK(prelude.samples[0] == 27 && prelude.samples[1] == 7 && prelude.samples[4] == 44 && prelude.samples[5] == 0);
    // Its script starts with the header: the base address, the end pointer and the channel pointers.
    CHECK(prelude.script.size() > 0x24 && prelude.script[0] == 0xc6 && prelude.script[1] == 0x5c);
    // The sound effects: eight instruments that follow each other from $4800, and the cursor sound of the system.
    CHECK(audio.sfxSamples.size() == FF6_SFX_SAMPLE_COUNT && audio.sfxMemory.size() == FF6_SPC_MEMORY_BYTES);
    CHECK(audio.sfxSamples[0].freqMult == 0xa0e1 && audio.sfxSamples[0].adsr1 == 0xff);
    size_t total = 0;
    for (const ff6::Sample& sample : audio.sfxSamples) {
        total += sample.brr.size();
        CHECK(sample.brr.size() % FF6_BRR_BLOCK_BYTES == 0 && sample.loopStart <= sample.brr.size());
    }
    CHECK(total == 0x14d);  // the whole block
    CHECK(audio.sfxSamples[3].loopStart == 0x1b);  // $486c to $4887
    const std::vector<uint8_t>& memory = audio.sfxMemory;
    CHECK(memory[FF6_SYSTEM_SFX_TABLE] == 0xff && memory[FF6_SYSTEM_SFX_TABLE + 1] == 0x17 && memory[0x17ff] == 0xc4);
}
