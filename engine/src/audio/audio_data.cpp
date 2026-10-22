#include "audio/audio_data.hpp"

#include <algorithm>
#include <stdexcept>

namespace ff6 {

AudioData loadAudioData(const std::string& packDir) {
    const std::vector<uint8_t> file = readFile(packDir + "/audio.ff6data");
    ByteReader reader(file);
    reader.expectMagic(FF6_AUDIO_MAGIC, "audio data");
    AudioData audio;
    const uint32_t sampleCount = reader.u32();
    audio.samples.resize(sampleCount);
    for (Sample& sample : audio.samples) {
        sample.loopStart = reader.u16();
        sample.freqMult = reader.u16();
        sample.adsr1 = reader.u8();
        sample.adsr2 = reader.u8();
        const uint32_t size = reader.u32();
        if (size % FF6_BRR_BLOCK_BYTES != 0) throw std::runtime_error("audio data holds a partial BRR block");
        sample.brr = reader.bytes(size);
    }
    const uint32_t songCount = reader.u32();
    audio.songs.resize(songCount);
    for (Song& song : audio.songs) {
        for (uint16_t& sample : song.samples) {
            sample = reader.u16();
            if (sample > sampleCount) throw std::runtime_error("a song uses a sample that does not exist");
        }
        song.script = reader.bytes(reader.u32());
    }
    audio.sfxMemory.assign(FF6_SPC_MEMORY_BYTES, 0);
    const uint32_t blockCount = reader.u32();
    for (uint32_t i = 0; i < blockCount; ++i) {
        const uint16_t address = reader.u16();
        const std::vector<uint8_t> block = reader.bytes(reader.u32());
        if (address + block.size() > audio.sfxMemory.size()) throw std::runtime_error("an audio block does not fit the sound memory");
        std::copy(block.begin(), block.end(), audio.sfxMemory.begin() + address);
    }
    const std::vector<uint8_t>& memory = audio.sfxMemory;
    const auto word = [&](size_t at) { return static_cast<uint16_t>(memory[at] | (memory[at + 1] << 8)); };
    for (size_t i = 0; i < FF6_SFX_SAMPLE_COUNT; ++i) {
        Sample sample;
        const size_t start = word(FF6_SPC_DIRECTORY + 4 * i), loop = word(FF6_SPC_DIRECTORY + 4 * i + 2);
        sample.freqMult = word(FF6_SPC_FREQ_MULT + 2 * i);
        sample.adsr1 = memory[FF6_SPC_ADSR + 2 * i];
        sample.adsr2 = memory[FF6_SPC_ADSR + 2 * i + 1];
        // The blocks run up to the one that ends the sample.
        size_t at = start;
        while (at + FF6_BRR_BLOCK_BYTES <= memory.size()) {
            const bool last = (memory[at] & FF6_BRR_FLAG_END) != 0;
            at += FF6_BRR_BLOCK_BYTES;
            if (last) break;
        }
        if (start < memory.size() && at > start) sample.brr.assign(memory.begin() + start, memory.begin() + at);
        sample.loopStart = loop >= start && loop - start <= sample.brr.size() ? static_cast<uint16_t>(loop - start) : 0;
        audio.sfxSamples.push_back(sample);
    }
    return audio;
}

}  // namespace ff6
