#include "sdl/sdl_audio.hpp"

#include <stdexcept>

#define FF6_AUDIO_BUFFER_FRAMES 1024
#define FF6_AUDIO_CHANNELS 2

namespace ff6 {

SdlAudio::SdlAudio(SoundProgram& program) : program_(program) {
    SDL_AudioSpec wanted{};
    wanted.freq = FF6_DSP_SAMPLE_RATE;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = FF6_AUDIO_CHANNELS;
    wanted.samples = FF6_AUDIO_BUFFER_FRAMES;
    wanted.callback = &SdlAudio::callback;
    wanted.userdata = this;
    // SDL converts to what the device accepts, so no change of the format is allowed.
    device_ = SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0);
    if (device_ == 0) throw std::runtime_error(SDL_GetError());
    SDL_PauseAudioDevice(device_, 0);
}

SdlAudio::~SdlAudio() {
    if (device_ != 0) SDL_CloseAudioDevice(device_);
}

void SdlAudio::command(uint8_t command, uint8_t arg1, uint8_t arg2) {
    SDL_LockAudioDevice(device_);
    program_.command(command, arg1, arg2);
    SDL_UnlockAudioDevice(device_);
}

uint8_t SdlAudio::port(int index) const {
    SDL_LockAudioDevice(device_);
    const uint8_t value = program_.port(index);
    SDL_UnlockAudioDevice(device_);
    return value;
}

int SdlAudio::previousSong() const {
    SDL_LockAudioDevice(device_);
    const int song = program_.previousSong();
    SDL_UnlockAudioDevice(device_);
    return song;
}

void SdlAudio::callback(void* user, Uint8* stream, int length) {
    SdlAudio* self = static_cast<SdlAudio*>(user);
    const size_t frames = static_cast<size_t>(length) / (sizeof(int16_t) * FF6_AUDIO_CHANNELS);
    self->program_.render(reinterpret_cast<int16_t*>(stream), frames);
}

}  // namespace ff6
