#pragma once

#include <cstdint>

namespace ff6 {

// What the game asks of the sound program: the commands of ExecSound (src/sound/sound_main.asm) and what the program
// tells back through its ports.
class Sound {
public:
    virtual ~Sound() = default;
    virtual void command(uint8_t command, uint8_t arg1, uint8_t arg2) = 0;
    // Port 1: the song position, 2: the channels that wait for the continue command, 3: the channels that play.
    virtual uint8_t port(int index) const = 0;
    virtual int previousSong() const = 0;  // the song that the last stop command interrupted, or -1
};

// For a game without sound: the commands do nothing and the program never makes the game wait.
class NoSound : public Sound {
public:
    void command(uint8_t, uint8_t, uint8_t) override {}
    uint8_t port(int) const override { return 0; }
    int previousSong() const override { return -1; }
};

}  // namespace ff6
