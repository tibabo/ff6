#include <algorithm>
#include <cstdlib>
#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "audio/sound_program.hpp"

namespace {

// Renders `seconds` of sound and returns the highest sample of each side.
struct Peaks {
    int left = 0, right = 0;
    int any() const { return std::max(left, right); }
};

Peaks renderPeaks(ff6::SoundProgram& program, double seconds) {
    std::vector<int16_t> out(static_cast<size_t>(seconds * FF6_DSP_SAMPLE_RATE) * 2);
    program.render(out.data(), out.size() / 2);
    Peaks peaks;
    for (size_t i = 0; i < out.size(); i += 2) {
        peaks.left = std::max(peaks.left, std::abs(static_cast<int>(out[i])));
        peaks.right = std::max(peaks.right, std::abs(static_cast<int>(out[i + 1])));
    }
    return peaks;
}

int renderPeak(ff6::SoundProgram& program, double seconds) { return renderPeaks(program, seconds).any(); }

// A song that plays a quarter note over and over.
std::vector<uint8_t> notes() {
    std::vector<uint8_t> body = test::prologue();
    body.insert(body.end(), {0xE2, 0xFF, 0x04, 0xBA, 0xE3, 0xEB});
    return body;
}

}  // namespace

void testSoundProgram() {
    {  // a song command starts the song; asking for the song that plays does nothing; bad songs are ignored
        const ff6::AudioData audio = test::makeAudio(notes(), 3);
        ff6::SoundProgram program(audio);
        CHECK(program.port(3) == 0 && program.currentSong() == -1);
        program.command(FF6_SOUND_PLAY_SONG, 1, 0xFF);
        CHECK(program.currentSong() == 1 && program.port(3) == 0x01);
        renderPeak(program, 0.3);
        const uint8_t position = program.port(1);
        program.command(FF6_SOUND_PLAY_SONG, 1, 0xFF);  // not loaded again
        program.command(FF6_SOUND_PLAY_SONG, 9, 0xFF);
        program.command(0x12, 2, 0xFF);  // not a command that starts a song
        CHECK(program.currentSong() == 1 && program.port(1) == position);
        program.command(FF6_SOUND_QUICK_SONG_FIRST + 2, 0, 0);  // quick load: the song is in the command
        CHECK(program.currentSong() == 2);
        // The sound effects and the commands that the program does not handle change nothing.
        program.command(FF6_SOUND_GAME_SFX, 3, 0x80);
        program.command(0x82, 4, 0x20);
        CHECK(program.currentSong() == 2);
    }
    {  // stopping the song remembers it; the sound is gone and the ports show it
        const ff6::AudioData audio = test::makeAudio(notes(), 2);
        ff6::SoundProgram program(audio);
        program.command(FF6_SOUND_PLAY_SONG, 1, 0xFF);
        CHECK(renderPeak(program, 0.3) > 1000);
        program.command(FF6_SOUND_STOP_SONG, 0, 0);
        CHECK(program.currentSong() == -1 && program.previousSong() == 1 && program.port(3) == 0);
        CHECK(renderPeak(program, 0.1) == 0);
        program.command(FF6_SOUND_PLAY_SONG, 1, 0xFF);  // it plays again, as nothing plays
        CHECK(program.currentSong() == 1);
    }
    {  // the volume of the song slides to the volume that the game asks for
        const ff6::AudioData audio = test::makeAudio(notes(), 1);
        ff6::SoundProgram program(audio);
        program.command(FF6_SOUND_PLAY_SONG, 0, 0xFF);
        const int loud = renderPeak(program, 0.5);
        CHECK(loud > 1000);
        program.command(FF6_SOUND_SET_SONG_VOLUME, 1, 0x00);  // one step of 7 ticks
        renderPeak(program, 0.5);  // the slide and the note that was playing
        CHECK(renderPeak(program, 1.5) == 0);
        // A start with a low volume and a slide up (the fade in of an event).
        program.command(FF6_SOUND_STOP_SONG, 0, 0);
        program.command(FF6_SOUND_PLAY_SONG, 0, FF6_SOUND_FADE_IN_VOLUME);
        const int quiet = renderPeak(program, 0.3);
        program.command(FF6_SOUND_SET_SONG_VOLUME, 2, 0xFF);
        renderPeak(program, 0.5);
        const int faded = renderPeak(program, 1.2);
        CHECK(quiet < loud / 4 && faded > loud * 3 / 4);
    }
    {  // a song that is paused resumes where it was, instead of starting again
        std::vector<uint8_t> body = test::prologue();
        body.insert(body.end(), {0xE2, 0xFF, 0xF9, 0x04, 0xBA, 0xE3, 0xEB});  // the output code counts the rounds
        const ff6::AudioData audio = test::makeAudio(body, 2);
        ff6::SoundProgram program(audio);
        program.command(FF6_SOUND_PLAY_SONG, 0, 0xFF);
        renderPeak(program, 2.5);
        const uint8_t rounds = program.port(1);
        CHECK(rounds >= 2);
        program.command(FF6_SOUND_PAUSE_SONG, 1, 0xFF);
        renderPeak(program, 0.05);
        CHECK(program.currentSong() == 1 && program.port(1) < rounds);
        CHECK(program.previousSong() == 0);  // the song that the event F3 brings back
        program.command(FF6_SOUND_PLAY_SONG, 0, 0xFF);
        CHECK(program.currentSong() == 0 && program.port(1) == rounds);
        // A song that was not paused starts from its beginning.
        program.command(FF6_SOUND_PLAY_SONG, 1, 0xFF);
        program.command(FF6_SOUND_PLAY_SONG, 0, 0xFF);
        CHECK(program.port(1) == 0);
    }
    {  // the continue command makes a conditional jump of the song happen once
        std::vector<uint8_t> body = test::prologue();
        body.insert(body.end(), {0xFC, 0x0F, 0x01, 0x04, 0xBA, 0xEB});  // jumps to the end, which is at 0x010F
        const ff6::AudioData audio = test::makeAudio(body, 1);
        ff6::SoundProgram waits(audio), jumps(audio);
        waits.command(FF6_SOUND_PLAY_SONG, 0, 0xFF);
        jumps.command(FF6_SOUND_PLAY_SONG, 0, 0xFF);
        jumps.command(FF6_SOUND_CONTINUE_SONG, 0, 0);
        CHECK(jumps.port(2) == 0x01 && waits.port(2) == 0);
        renderPeak(waits, 0.3);
        renderPeak(jumps, 0.3);
        CHECK(waits.port(3) == 0x01);  // plays its note
        CHECK(jumps.port(3) == 0 && jumps.port(2) == 0);  // the song has ended
    }
    {  // the echo delay command is accepted at its limits
        const ff6::AudioData audio = test::makeAudio(notes(), 1);
        ff6::SoundProgram program(audio);
        program.command(FF6_SOUND_SET_ECHO_DELAY, 0, 0);
        program.command(FF6_SOUND_SET_ECHO_DELAY, 0xFF, 0);  // only the low 4 bits count
        program.command(FF6_SOUND_PLAY_SONG, 0, 0xFF);
        CHECK(renderPeak(program, 0.2) > 0);
    }
    {  // a game sound effect plays without a song, ends by itself, and can be stopped
        const ff6::AudioData audio = test::makeAudio(notes(), 1);
        ff6::SoundProgram program(audio);
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        CHECK(renderPeak(program, 0.3) > 1000 && program.currentSong() == -1);
        CHECK(renderPeak(program, 1.5) >= 0);
        CHECK(renderPeak(program, 0.2) == 0);  // it has ended
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        renderPeak(program, 0.1);
        program.command(FF6_SOUND_STOP_SFX, 0, 0);
        CHECK(renderPeak(program, 0.1) == 0);
        // An effect that does not exist stays silent.
        program.command(FF6_SOUND_GAME_SFX, 7, FF6_SFX_PAN_CENTER);
        CHECK(renderPeak(program, 0.2) == 0);
    }
    {  // the pan of an effect sets the side that it comes from
        const ff6::AudioData audio = test::makeAudio(notes(), 1);
        ff6::SoundProgram program(audio);
        program.command(FF6_SOUND_GAME_SFX, 0, 0x00);
        const Peaks left = renderPeaks(program, 0.3);
        CHECK(left.left > 1000 && left.right == 0);
        renderPeaks(program, 2.0);
        program.command(FF6_SOUND_GAME_SFX, 0, 0xFF);
        const Peaks right = renderPeaks(program, 0.3);
        CHECK(right.right > 1000 && right.left == 0);
        renderPeaks(program, 2.0);
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        const Peaks center = renderPeaks(program, 0.3);
        CHECK(center.left > 500 && center.right > 500);
        // The pan slides: the game asks for the left side over 4 steps of 7 ticks.
        renderPeaks(program, 2.0);
        program.command(FF6_SOUND_SET_SFX_PAN, 4, 0x00);
        renderPeaks(program, 0.5);
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        CHECK(renderPeaks(program, 0.3).right > 500);  // the effect sets its own pan when it starts
    }
    {  // the volume of the sound effects is not the volume of the song
        const ff6::AudioData audio = test::makeAudio(notes(), 1);
        ff6::SoundProgram program(audio);
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        const int loud = renderPeak(program, 0.3);
        renderPeak(program, 2.0);
        program.command(FF6_SOUND_SET_SFX_VOLUME, 0, 0x40);
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        const int quiet = renderPeak(program, 0.3);
        CHECK(quiet > 0 && quiet < loud * 2 / 3);
        program.command(FF6_SOUND_PLAY_SONG, 0, 0xFF);
        program.command(FF6_SOUND_SET_SONG_VOLUME, 0, 0x00);  // the song, not the effects
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        CHECK(renderPeak(program, 0.3) > 0);
    }
    {  // a slide of the volume of the effects that ends at zero stops them, and the volume comes back
        const ff6::AudioData audio = test::makeAudio(notes(), 1);
        ff6::SoundProgram program(audio);
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        renderPeak(program, 0.05);
        program.command(FF6_SOUND_SET_SFX_VOLUME, 1, 0x00);
        renderPeak(program, 0.3);
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        CHECK(renderPeak(program, 0.3) > 1000);
        // Set to zero at once, the volume stays there.
        program.command(FF6_SOUND_SET_SFX_VOLUME, 0, 0x00);
        renderPeak(program, 2.0);
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        CHECK(renderPeak(program, 0.3) == 0);
    }
    {  // a system effect plays, and the ones that are not in the table do not
        const ff6::AudioData audio = test::makeAudio(notes(), 1);
        ff6::SoundProgram program(audio);
        program.command(FF6_SOUND_SYSTEM_SFX_FIRST + 1, 0, 0);
        CHECK(renderPeak(program, 0.3) == 0);
        program.command(FF6_SOUND_SYSTEM_SFX_FIRST, 0, 0);
        const Peaks heard = renderPeaks(program, 0.3);
        CHECK(heard.left > 500 && heard.right > 500);  // centered, whatever the pan of the game's effects
        CHECK(renderPeak(program, 2.0) >= 0 && renderPeak(program, 0.2) == 0);
    }
    {  // an effect shares the sound with a song, and the song goes on
        const ff6::AudioData audio = test::makeAudio(notes(), 1);
        ff6::SoundProgram program(audio);
        program.command(FF6_SOUND_PLAY_SONG, 0, 0xFF);
        renderPeak(program, 0.2);
        program.command(FF6_SOUND_GAME_SFX, 0, FF6_SFX_PAN_CENTER);
        program.command(FF6_SOUND_SYSTEM_SFX_FIRST, 0, 0);
        renderPeak(program, 0.3);
        CHECK(program.currentSong() == 0 && program.port(3) == 0x01);
        program.command(FF6_SOUND_STOP_ALL, 0, 0);  // F0 stops the effects with the song
        CHECK(renderPeak(program, 0.1) == 0);
    }
}
