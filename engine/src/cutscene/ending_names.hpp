#pragma once

#include <array>
#include <memory>
#include <vector>

#include "cutscene/ppu.hpp"
#include "data/title_data.hpp"

namespace ff6 {

// The big letters of the names in the scenes of the characters (src/menu/ending.asm): two groups of sprites with the letters of a
// name, which wait, rise into the bottom of the screen, stay, then rise out, in a palette that fades in and out as the scene goes.
class EndingNames {
public:
    // `state` is the one of the ending that starts the scene: 40 to 135 for the characters.
    EndingNames(const TitleData& data, int state);

    void tick();
    // The sprites on black.
    void draw(uint8_t* rgba) const { ppu_.render(rgba); }

private:
    struct Big {  // EndingBigTextTask
        int x = 0, address = 0;  // the animation of the letters
        int state = 0, counter = 0;
        int y = 0xD000, speed = 0;  // 1/256 pixel
        bool alive = true;
    };
    struct Fade {  // the palette of a sprite color set: 32 steps, one every four frames
        int level = 0, target = 0, counter = 0;
    };

    void startBig(int character, int wait);
    void startAs();
    void tickBig(Big& big);
    uint16_t word(int address) const;
    void fadeTo(Fade& fade, int target) {
        fade.target = target;
        fade.counter = 0;
    }
    void draw();

    const TitleData& data_;
    Ppu ppu_;
    std::vector<Big> bigs_;
    int asLeft_ = 0;  // frames that the "as" word still shows
    std::array<Fade, 2> fades_{};  // sprite palettes 0 (the letters) and 1 (the "as")
    bool twoNames_ = false;
    int character_ = 0, secondCharacter_ = 0;
    int frame_ = 0;
};

// The letters of the scene that a state starts, null for the scenes that are not of the characters.
std::unique_ptr<EndingNames> makeEndingNames(const TitleData& data, int state);

}  // namespace ff6
