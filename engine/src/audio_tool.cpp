// Renders the instrument samples of the game to a WAV file, to listen to them without a sound device.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio/audio_data.hpp"
#include "audio/dsp_voice.hpp"
#include "audio/sound_program.hpp"
#include "audio/synth.hpp"

#define FF6_TOOL_DEFAULT_SECONDS 3
#define FF6_TOOL_DEFAULT_HOLD_SECONDS 2
#define FF6_TOOL_VOLUME 100
#define FF6_WAV_HEADER_BYTES 44
#define FF6_WAV_BITS 16
#define FF6_WAV_CHANNELS 2

namespace {

void put32(std::vector<uint8_t>& out, size_t at, uint32_t value) {
    for (int i = 0; i < 4; ++i) out[at + static_cast<size_t>(i)] = static_cast<uint8_t>(value >> (8 * i));
}

void put16(std::vector<uint8_t>& out, size_t at, uint16_t value) {
    out[at] = static_cast<uint8_t>(value);
    out[at + 1] = static_cast<uint8_t>(value >> 8);
}

void writeWav(const std::string& path, const std::vector<int16_t>& samples) {
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * 2);
    std::vector<uint8_t> file(FF6_WAV_HEADER_BYTES + dataBytes);
    std::memcpy(&file[0], "RIFF", 4);
    put32(file, 4, 36 + dataBytes);
    std::memcpy(&file[8], "WAVEfmt ", 8);
    put32(file, 16, 16);
    put16(file, 20, 1);
    put16(file, 22, FF6_WAV_CHANNELS);
    put32(file, 24, FF6_DSP_SAMPLE_RATE);
    put32(file, 28, FF6_DSP_SAMPLE_RATE * FF6_WAV_CHANNELS * FF6_WAV_BITS / 8);
    put16(file, 32, FF6_WAV_CHANNELS * FF6_WAV_BITS / 8);
    put16(file, 34, FF6_WAV_BITS);
    std::memcpy(&file[36], "data", 4);
    put32(file, 40, dataBytes);
    for (size_t i = 0; i < samples.size(); ++i) put16(file, FF6_WAV_HEADER_BYTES + i * 2, static_cast<uint16_t>(samples[i]));
    std::FILE* out = std::fopen(path.c_str(), "wb");
    if (!out) throw std::runtime_error("cannot write " + path);
    const size_t written = std::fwrite(file.data(), 1, file.size(), out);
    std::fclose(out);
    if (written != file.size()) throw std::runtime_error("cannot write " + path);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) throw std::runtime_error("usage: ff6_audio <pack> (--sample n [--pitch p] [--hold s] | --song n [--echo delay] | --sfx n [--pan p] | --system n) --wav out.wav [--seconds s]");
        long sampleIndex = -1, songIndex = -1, sfxIndex = -1, systemIndex = -1, pan = FF6_SFX_PAN_CENTER, echoDelay = FF6_AKAO_ECHO_DELAY;
        long pitch = FF6_DSP_PITCH_ONE;
        double seconds = FF6_TOOL_DEFAULT_SECONDS, hold = FF6_TOOL_DEFAULT_HOLD_SECONDS;
        std::string wavPath;
        for (int i = 2; i + 1 < argc; i += 2) {
            const std::string option = argv[i];
            if (option == "--sample") sampleIndex = std::stol(argv[i + 1]);
            else if (option == "--song") songIndex = std::stol(argv[i + 1]);
            else if (option == "--echo") echoDelay = std::stol(argv[i + 1]);
            else if (option == "--sfx") sfxIndex = std::stol(argv[i + 1]);
            else if (option == "--system") systemIndex = std::stol(argv[i + 1]);
            else if (option == "--pan") pan = std::stol(argv[i + 1], nullptr, 0);
            else if (option == "--pitch") pitch = std::stol(argv[i + 1], nullptr, 0);
            else if (option == "--seconds") seconds = std::stod(argv[i + 1]);
            else if (option == "--hold") hold = std::stod(argv[i + 1]);
            else if (option == "--wav") wavPath = argv[i + 1];
            else throw std::runtime_error("unknown option " + option);
        }
        const ff6::AudioData audio = ff6::loadAudioData(argv[1]);
        if (sfxIndex >= 0 || systemIndex >= 0) {
            if (wavPath.empty()) throw std::runtime_error("--wav is required");
            ff6::SoundProgram program(audio);
            if (sfxIndex >= 0) {
                program.command(FF6_SOUND_GAME_SFX, static_cast<uint8_t>(sfxIndex), static_cast<uint8_t>(pan));
            } else {
                program.command(static_cast<uint8_t>(FF6_SOUND_SYSTEM_SFX_FIRST + systemIndex), 0, 0);
            }
            std::vector<int16_t> out(static_cast<size_t>(seconds * FF6_DSP_SAMPLE_RATE) * 2);
            program.render(out.data(), out.size() / 2);
            int peak = 0;
            size_t last = 0;
            for (size_t i = 0; i < out.size(); ++i) {
                peak = std::max(peak, std::abs(static_cast<int>(out[i])));
                if (out[i] != 0) last = i / 2;
            }
            writeWav(wavPath, out);
            std::printf("%s %ld: peak %d, sound until %.2f s\n", sfxIndex >= 0 ? "sound effect" : "system effect",
                        sfxIndex >= 0 ? sfxIndex : systemIndex, peak, static_cast<double>(last) / FF6_DSP_SAMPLE_RATE);
            return 0;
        }
        if (songIndex >= 0) {
            if (static_cast<size_t>(songIndex) >= audio.songs.size() || wavPath.empty()) {
                throw std::runtime_error("--song (0 to " + std::to_string(audio.songs.size() - 1) + ") and --wav are required");
            }
            ff6::Synth synth(audio);
            synth.setEchoDelay(static_cast<int>(echoDelay));
            synth.playSong(static_cast<size_t>(songIndex));
            std::vector<int16_t> out(static_cast<size_t>(seconds * FF6_DSP_SAMPLE_RATE) * 2);
            synth.render(out.data(), out.size() / 2);
            int peak = 0;
            for (int16_t sample : out) peak = std::max(peak, std::abs(static_cast<int>(sample)));
            writeWav(wavPath, out);
            std::printf("song %ld: %.1f s, peak %d, %s\n", songIndex, seconds, peak, synth.playing() ? "still playing" : "ended");
            return 0;
        }
        if (sampleIndex < 0 || static_cast<size_t>(sampleIndex) >= audio.samples.size() || wavPath.empty()) {
            throw std::runtime_error("--sample (0 to " + std::to_string(audio.samples.size() - 1) + ") and --wav are required");
        }
        const ff6::Sample& sample = audio.samples[static_cast<size_t>(sampleIndex)];
        ff6::DspVoice voice;
        voice.keyOn(sample.brr.data(), sample.brr.size(), sample.loopStart, sample.adsr1, sample.adsr2);
        voice.setPitch(static_cast<uint16_t>(pitch));
        voice.setVolume(FF6_TOOL_VOLUME, FF6_TOOL_VOLUME);
        const size_t total = static_cast<size_t>(seconds * FF6_DSP_SAMPLE_RATE);
        const size_t release = static_cast<size_t>(hold * FF6_DSP_SAMPLE_RATE);
        std::vector<int16_t> out;
        int peak = 0;
        for (size_t tick = 0; tick < total; ++tick) {
            if (tick == release) voice.keyOff();
            int32_t left = 0, right = 0;
            voice.render(static_cast<uint32_t>(tick), &left, &right);
            const int16_t l = static_cast<int16_t>(std::max(-32768, std::min(32767, left)));
            const int16_t r = static_cast<int16_t>(std::max(-32768, std::min(32767, right)));
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
            out.push_back(l);
            out.push_back(r);
        }
        writeWav(wavPath, out);
        std::printf("sample %ld: %zu bytes of BRR, loop at %u, adsr %02X %02X, peak %d\n", sampleIndex, sample.brr.size(),
                    sample.loopStart, sample.adsr1, sample.adsr2, peak);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
}
