#pragma once

#include <array>

#include "cutscene/core.hpp"

namespace ff6 {

// The last screen (src/cutscene/the_end.asm): THE END comes up through a window above its reflection, the stars
// fly out of the middle, and the prelude plays. The game never leaves it; here the confirm button ends it, once the stars are on.
class TheEndScene : public Cutscene {
public:
    TheEndScene(const TitleData& data, Sound& sound);

    void tick(const InputState& input) override;
    void draw(uint8_t* rgba) override { ppu_.render(rgba); }
    bool finished() const override { return done_; }
    bool skipped() const override { return false; }

    const Ppu& ppu() const { return ppu_; }
    int phase() const { return phase_; }

private:
    static constexpr int kStars = 128;

    struct Star {
        uint8_t x = 0, y = 0, tile = 0;
        uint8_t horizontal = 0, vertical = 0, counter = 0, rate = 0, level = 0;  // $0cfc, $0cfd, $0afe, $0cfe, $0cff
    };

    void initStar(Star& star);
    void resetStars();
    void moveStars();
    void nmi();
    void fade(const std::array<uint8_t, 32>& target, std::array<int, 48>& accumulator, int factor, uint16_t* destination);
    void setLines();

    const TitleData& data_;
    Sound& sound_;
    Ppu ppu_;
    std::array<Star, kStars> stars_{};
    uint8_t random_ = 0;  // $00
    uint8_t group_ = 0;  // $3e: which fourth of the stars moves this frame
    int scrollCounter_ = 1, scrollIndex_ = 0, waveIndex_ = 0;  // $3c, $3a: the wave of the reflection, and the one in the table
    std::array<uint16_t, 16> letterPalette_{}, starPalette_{};  // $09a0 and $0980, copied to the palette every frame
    std::array<int, 48> letterFade_{}, starFade_{};  // the colors that rise, three channels each
    int phase_ = 0, wait_ = 0, scrollRows_ = 0x00A8, scrollWait_ = 2;
    bool previousConfirm_ = true, done_ = false;
};

}  // namespace ff6
