#pragma once

#include <array>
#include <memory>

#include "cutscene/ppu.hpp"
#include "data/title_data.hpp"

namespace ff6 {

// The book of the ending (src/menu/ending.asm, states 45 to 48 and "and you", 74 to 77): a picture of an open book on the first
// background, whose pages the game changes by copying another part of the graphics over the same tiles, with the palette that
// fades in, and for "and you" the big letters of the words.
class EndingBook {
public:
    // `state` is 45 for the book and 74 for "and you".
    EndingBook(const TitleData& data, int state);

    void tick();
    void draw(uint8_t* rgba) const;

private:
    struct Fade {  // the colors of a palette that go to a target: 32 steps, one every `speed` frames
        int level = 0, target = 0, counter = 0, speed = 2;
    };
    struct Animation {  // BookAnim1, BookAnim2 and BookAnimEnd: the pages that follow each other
        const int* frames = nullptr;
        int index = 0, duration = 0;
    };

    void copyPage(int page);
    void startAnimation(const int* frames);
    void tickAnimation();
    void fadeTo(Fade& fade, int target, int speed) {
        fade.target = target;
        fade.speed = speed;
        fade.counter = 0;
    }
    uint16_t paletteWord(int address, int index) const;
    void applyPalettes();
    void drawLetters();
    void tickLetters();

    const TitleData& data_;
    Ppu ppu_;
    bool andYou_ = false;
    int frame_ = 0, hscroll_ = 0, scrollLeft_ = 0, mosaicStart_ = -1;
    std::array<uint16_t, 0x800> map_{};  // the two screens of the first background (wBG1Tiles)
    Animation animation_;
    bool started_ = false;
    std::array<Fade, 3> fades_{};  // palettes 1 and 2 of the background, and 0 of the sprites
    int letterState_ = 0, letterCounter_ = 300, letterY_ = 0xD000, letterSpeed_ = 0;
    bool letterAlive_ = true;
};

// The book for the state that starts it, null for the others.
std::unique_ptr<EndingBook> makeEndingBook(const TitleData& data, int state);

}  // namespace ff6
