#include "cutscene/ending_names.hpp"

#include <algorithm>

namespace ff6 {
namespace {

constexpr int kBase = 0x8A70;  // the address in its bank of the first byte of the data
constexpr int kNameTable = 0x8DE4, kAsAnim = 0x8E38, kPalette = 0x9754;  // the table of names, the "as" and the color set that the sprites fade to
constexpr int kAsX = 0x79, kAsY = 0xC0, kAsFrames = 600;
constexpr int kNameY = 0xD000;

// The state of a scene of a character and the character that it is about (the first one of two).
struct Scene {
    int state, character, second;
};
constexpr Scene kScenes[] = {{40, 3, -1}, {50, 2, -1},  {60, 10, -1}, {64, 4, 5},  {70, 12, -1}, {80, 11, -1},
                             {90, 0, -1}, {100, 1, 6},  {110, 8, -1}, {120, 7, -1}, {130, 9, -1}, {135, 13, -1}};

}  // namespace

EndingNames::EndingNames(const TitleData& data, int state) : data_(data) {
    for (const Scene& scene : kScenes) {
        if (scene.state != state) continue;
        character_ = scene.character;
        secondCharacter_ = scene.second;
        twoNames_ = scene.second >= 0;
    }
    ppu_.objsel = 0x03;
    ppu_.tm = 0x10;
    ppu_.brightness = 15;
    ppu_.forceBlank = false;
    // The letters are in two bits per pixel, which the game spreads into the first two planes of tiles of four bits.
    for (size_t tile = 0; tile * 16 + 16 <= data.endingFont.size(); ++tile) {
        for (size_t row = 0; row < 8; ++row) {
            const size_t at = tile * 16 + row * 2;
            ppu_.vram[0x7000 + tile * 16 + row] = static_cast<uint16_t>(data.endingFont[at] | data.endingFont[at + 1] << 8);
        }
    }
    startBig(character_, 300);
    startAs();
}

uint16_t EndingNames::word(int address) const {
    const size_t at = static_cast<size_t>(address - kBase);
    return at + 1 < data_.endingAnim.size() ? static_cast<uint16_t>(data_.endingAnim[at] | data_.endingAnim[at + 1] << 8) : 0;
}

// CreateBigCharNameTask: two tasks, one for each half of the name, from the table of the names.
void EndingNames::startBig(int character, int wait) {
    bigs_.clear();
    const int entry = kNameTable + character * 6;
    for (int half = 0; half < 2; ++half) {
        Big big;
        big.address = word(entry + half * 3);
        big.x = data_.endingAnim[static_cast<size_t>(entry + half * 3 + 2 - kBase)];
        big.counter = wait;
        big.y = kNameY;
        bigs_.push_back(big);
    }
}

void EndingNames::startAs() { asLeft_ = kAsFrames; }

// EndingBigTextTask: wait, rise for 150 frames, stay for 240, rise out for 300, then it is gone.
void EndingNames::tickBig(Big& big) {
    if (!big.alive) return;
    const auto wait = [&] {
        if (big.counter == 0) {
            ++big.state;
        } else {
            --big.counter;
        }
    };
    switch (big.state) {
        case 0: ++big.state; big.speed = 0; wait(); break;  // the counter was set when it was made
        case 1: case 3: case 5: case 7: wait(); break;
        case 2: big.speed = -0x40; big.counter = 0x96; big.state = 3; wait(); break;
        case 4: big.speed = 0; big.counter = 0xF0; big.state = 5; wait(); break;
        case 6: big.speed = -0x40; big.counter = 0x12C; big.state = 7; wait(); break;
        default: big.alive = false; return;
    }
    big.y = (big.y + big.speed) & 0xFFFF;
}

void EndingNames::tick() {
    // What the states of the scene do, at the frames that their waits add up to.
    const auto fadeLetters = [&](int target) { fadeTo(fades_[0], target); };
    const auto fadeAs = [&](int target) { fadeTo(fades_[1], target); };
    if (frame_ == 120) fadeAs(32);
    if (frame_ == 360) {
        fadeAs(0);
        fadeLetters(32);
    }
    if (twoNames_) {
        if (frame_ == 600) {  // the second name comes: its "as", and the letters of the first go
            startAs();
            fadeLetters(0);
            fadeAs(32);
        }
        if (frame_ == 720) {
            for (Big& big : bigs_) big.alive = false;
        }
        if (frame_ == 900) {
            startBig(secondCharacter_, 20);
            fadeLetters(32);
            fadeAs(0);
        }
        if (frame_ == 1200) fadeLetters(0);
    } else if (frame_ == 840) {
        fadeLetters(0);
    }
    for (Fade& fade : fades_) {
        if (fade.level == fade.target) continue;
        if (++fade.counter >= 4) {
            fade.counter = 0;
            fade.level += fade.target > fade.level ? 1 : -1;
        }
    }
    for (Big& big : bigs_) tickBig(big);
    if (asLeft_ > 0) --asLeft_;
    ++frame_;
    draw();
}

// The sprites of the animations, and the colors that the fades have reached.
void EndingNames::draw() {
    ppu_.sprites.clear();
    const auto add = [&](int address, int x, int y) {
        const int frame = word(address);
        const int count = data_.endingAnim[static_cast<size_t>(frame - kBase)];
        for (int i = 0; i < count; ++i) {
            const uint8_t* sprite = &data_.endingAnim[static_cast<size_t>(frame - kBase + 1 + i * 4)];
            PpuSprite out;
            out.large = (sprite[0] & 0x80) != 0;
            out.x = ((sprite[0] & 0x7F) + x) & 0xFF;
            out.y = (sprite[1] + y + 1) & 0xFF;
            out.attr = sprite[3];
            out.tile = sprite[2] | (sprite[3] & 1) << 8;
            ppu_.sprites.push_back(out);
        }
    };
    if (asLeft_ > 0) add(kAsAnim, kAsX, kAsY);
    for (const Big& big : bigs_) {
        if (big.alive) add(big.address, big.x, big.y >> 8);
    }
    for (size_t pal = 0; pal < 2; ++pal) {
        for (size_t i = 0; i < 16; ++i) {
            const int target = word(kPalette + static_cast<int>(i) * 2);
            int color = 0;
            for (int shift = 0; shift <= 10; shift += 5) color |= ((target >> shift & 31) * fades_[pal].level / 32) << shift;
            ppu_.cgram[128 + pal * 16 + i] = static_cast<uint16_t>(color);
        }
    }
}

std::unique_ptr<EndingNames> makeEndingNames(const TitleData& data, int state) {
    for (const Scene& scene : kScenes) {
        if (scene.state == state && data.endingFont.size() >= FF6_TITLE_ENDING_FONT_BYTES) return std::make_unique<EndingNames>(data, state);
    }
    return nullptr;
}

}  // namespace ff6
