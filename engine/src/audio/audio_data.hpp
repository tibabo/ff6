#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "data/pack.hpp"

namespace ff6 {

// One instrument sample of src/sound/sample_brr.asm.
struct Sample {
    uint16_t loopStart = 0;  // byte offset of the loop in `brr`
    uint16_t freqMult = 0;  // pitch multiplier of the instrument
    uint8_t adsr1 = 0, adsr2 = 0;  // DSP envelope registers
    std::vector<uint8_t> brr;  // whole 9-byte blocks
};

// One song: the instruments that it uses (sample number + 1, 0 for none) and its script.
struct Song {
    std::array<uint16_t, FF6_SONG_SAMPLE_SLOTS> samples{};
    std::vector<uint8_t> script;
};

struct AudioData {
    std::vector<Sample> samples;
    std::vector<Song> songs;
    // The sound memory as the game fills it at start: the sound effect scripts, tables and instruments.
    std::vector<uint8_t> sfxMemory;
    std::vector<Sample> sfxSamples;  // the instruments 0 to 7, which the sound effects use
};

AudioData loadAudioData(const std::string& packDir);

}  // namespace ff6
