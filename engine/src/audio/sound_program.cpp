#include "audio/sound_program.hpp"

#include <algorithm>

namespace ff6 {

void SoundProgram::command(uint8_t command, uint8_t arg1, uint8_t arg2) {
    if (command & FF6_SOUND_INTERRUPT_FLAG) {
        interrupt(command, arg1, arg2);
    } else if (command >= FF6_SOUND_QUICK_SONG_FIRST && command <= FF6_SOUND_QUICK_SONG_LAST) {
        playSong(FF6_SOUND_PLAY_SONG, static_cast<uint8_t>(command & 0x0F), 0xFF);
    } else if (command >= FF6_SOUND_PLAY_SONG && command < FF6_SOUND_GAME_SFX) {
        const int kind = command & 7;  // 0, 1, 4 and 5 start a song, the others do nothing
        if (kind == 0 || kind == 1 || kind == 4 || kind == 5) playSong(command, arg1, arg2);
    } else if (command == FF6_SOUND_GAME_SFX) {
        playGameSfx(arg1, arg2);
    } else if (command >= FF6_SOUND_SYSTEM_SFX_FIRST && command <= FF6_SOUND_SYSTEM_SFX_LAST) {
        playSystemSfx(command & 0x0F);
    }
}

// Interrupt_18: a game effect is up to two scripts, that take the voices from the top of the free ones; the effect that
// plays is replaced.
void SoundProgram::playGameSfx(uint8_t effect, uint8_t pan) {
    const std::vector<uint8_t>& memory = audio_.sfxMemory;
    const AkaoSequencer& sfx = synth_.sfx();
    const uint8_t active = sfx.activeChannels(), system = sfx.systemChannels();
    const uint8_t playing = static_cast<uint8_t>(active & ~system);
    const uint8_t candidates = playing != 0 ? playing : static_cast<uint8_t>(~system & 0xF0);
    int voice = (candidates & 0x80) ? 7 : (candidates & 0x40) ? 6 : 5;
    for (int v = 0; v < FF6_AKAO_VOICES; ++v) {
        if (playing & (1 << v)) synth_.stopSfxVoice(v);
    }
    const size_t at = FF6_SFX_POINTERS + static_cast<size_t>(effect) * FF6_SFX_POINTER_BYTES;
    for (const size_t pointer : {at + 2, at}) {  // the second script takes the first voice
        const uint16_t address = static_cast<uint16_t>(memory[pointer] | (memory[pointer + 1] << 8));
        if ((address >> 8) == 0 || voice < 0) continue;
        synth_.sfx().startSfx(voice--, address, false);
    }
    synth_.sfx().setSfxPan(0, pan);
}

// SystemSfx: the first free voice from the top, or else the first one that plays a system effect.
void SoundProgram::playSystemSfx(uint8_t effect) {
    const std::vector<uint8_t>& memory = audio_.sfxMemory;
    const size_t at = FF6_SYSTEM_SFX_TABLE + static_cast<size_t>(effect) * 2;
    const uint16_t address = static_cast<uint16_t>(memory[at] | (memory[at + 1] << 8));
    if (address == 0) return;
    const AkaoSequencer& sfx = synth_.sfx();
    const uint8_t busy = sfx.activeChannels(), system = sfx.systemChannels();
    int voice = FF6_SFX_FIRST_VOICE;
    for (int v = FF6_AKAO_VOICES - 1; v >= FF6_SFX_FIRST_VOICE; --v) {
        const bool usable = (busy & 0xF0) != 0xF0 ? (busy & (1 << v)) == 0 : (system & (1 << v)) != 0;
        if (usable) {
            voice = v;
            break;
        }
    }
    synth_.stopSfxVoice(voice);
    synth_.sfx().startSfx(voice, address, true);
}

// PlaySong: asking for the song that plays does nothing; a pause command or one of the battle songs keeps the song
// that plays, to resume it when it is asked for again.
void SoundProgram::playSong(uint8_t command, uint8_t song, uint8_t volume) {
    if (song == currentSong_ || song >= audio_.songs.size()) return;
    const uint8_t pausing[] = FF6_SOUND_PAUSING_SONGS;
    const bool suspend = command >= FF6_SOUND_PAUSE_SONG || std::find(std::begin(pausing), std::end(pausing), song) != std::end(pausing);
    synth_.playSong(song, volume, (command & 1) != 0, suspend);
    if (currentSong_ >= 0) previousSong_ = currentSong_;  // what the fade in of the event F3 brings back
    currentSong_ = song;
}

void SoundProgram::interrupt(uint8_t command, uint8_t arg1, uint8_t arg2) {
    switch (command) {
        case FF6_SOUND_SET_VOLUME:  // the volume of the song and of the sound effects
            synth_.sequencer().setSongVolume(arg1, arg2);
            synth_.sfx().setSongVolume(arg1, arg2);
            break;
        case FF6_SOUND_SET_SONG_VOLUME:
            synth_.sequencer().setSongVolume(arg1, arg2);
            break;
        case FF6_SOUND_SET_SFX_VOLUME:
            synth_.sfx().setSongVolume(arg1, arg2);
            break;
        case FF6_SOUND_SET_SFX_PAN:
            synth_.sfx().setSfxPan(arg1, arg2);
            break;
        case FF6_SOUND_SET_TEMPO_RATIO:
            synth_.sequencer().setTempoRatio(arg1, arg2);
            break;
        case FF6_SOUND_SET_PITCH_SHIFT:
            synth_.sequencer().setPitchShift(arg1, arg2);
            break;
        case FF6_SOUND_CONTINUE_SONG:
            synth_.sequencer().continueSong();
            break;
        case FF6_SOUND_STOP_ALL:
        case FF6_SOUND_STOP_SONG:
            if (currentSong_ >= 0) previousSong_ = currentSong_;
            currentSong_ = -1;
            synth_.stop();
            if (command == FF6_SOUND_STOP_ALL) synth_.stopSfx();
            break;
        case FF6_SOUND_STOP_SFX:
            synth_.stopSfx();
            break;
        case FF6_SOUND_SET_ECHO_DELAY:
            synth_.setEchoDelay(arg1 & 0x0F);
            break;
        default:
            break;  // the mono mode is not handled
    }
}

uint8_t SoundProgram::port(int index) const {
    if (!synth_.loaded()) return 0;
    const AkaoSequencer& sequencer = synth_.sequencer();
    switch (index) {
        case 1: return sequencer.outputCode();
        case 2: return sequencer.conditionalJump();
        case 3: return sequencer.activeChannels();
        default: return 0;
    }
}

}  // namespace ff6
