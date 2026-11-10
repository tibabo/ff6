#include "cutscene/the_end.hpp"

#include <algorithm>

namespace ff6 {
namespace {

constexpr int kWaitLimit = 1800;  // frames to wait for the sound program before going on without it
constexpr int kLetterFrames = 128, kStarFrames = 64, kPreludeFrames = 1200;
constexpr int kLettersPal = 0x60, kStarsPal = 0x80;  // in the palettes of the screen
constexpr int kWindowTop = 87, kWindowLines = 24, kWaveTop = 80, kWaveLines = 32;
constexpr uint8_t kWave[12] = {0xAC, 0xAD, 0xAE, 0xAE, 0xAE, 0xAD, 0xAC, 0xAB, 0xAA, 0xAA, 0xAA, 0xAB};
constexpr uint8_t kLettersFilled = 0x06;

// The tiles of the screen: 13 columns of the letters on bg1, 14 of their reflection on bg2.
void writeRow(Ppu& ppu, int address, int first, int count, uint16_t attributes, bool same = false) {
    for (int i = 0; i < count; ++i) ppu.vram[static_cast<size_t>(address + i)] = static_cast<uint16_t>(attributes | (same ? first : first + i));
}

}  // namespace

TheEndScene::TheEndScene(const TitleData& data, Sound& sound) : data_(data), sound_(sound) {
    ppu_.objsel = 0x00;
    ppu_.bgsc = {0x40, 0x50, 0x60};
    ppu_.nba12 = 0;
    ppu_.nba34 = 0;
    ppu_.tm = 0x13;
    ppu_.ts = 0;
    ppu_.brightness = 0;
    ppu_.forceBlank = true;
    const auto transfer = [&](const std::vector<uint8_t>& bytes, size_t to) {
        for (size_t i = 0; i + 1 < bytes.size(); i += 2) ppu_.vram[to + i / 2] = static_cast<uint16_t>(bytes[i] | bytes[i + 1] << 8);
    };
    transfer(data.theEndGfx, 0);
    transfer(data.theEndStars, data.theEndGfx.size() / 2);
    for (size_t i = 0; i < 128; ++i) {
        const uint16_t color = static_cast<uint16_t>(data.theEndPal[i * 2] | data.theEndPal[i * 2 + 1] << 8);
        ppu_.cgram[i] = color;
        ppu_.cgram[128 + i] = color;
    }
    // The map: three rows of nothing then the letters (bg1, palette 1), and their reflection (bg2, palette 3).
    for (int row = 0; row < 3; ++row) writeRow(ppu_, 0x4000 + row * 32, kLettersFilled, 13, 0x2400, true);
    for (int row = 0; row < 3; ++row) writeRow(ppu_, 0x4060 + row * 32, row * 16, 13, 0x2400);
    for (int row = 0; row < 4; ++row) writeRow(ppu_, 0x5000 + row * 32, 0x30 + row * 16, 14, 0x2C00);
    for (size_t n = kStars; n-- > 0;) initStar(stars_[n]);  // from the last one, as the game does
    random_ = 0;
    // What a line shows: the windows, the scroll of the letters, and the palettes that the fade holds back.
    for (size_t i = 0; i < 16; ++i) {
        ppu_.cgram[0x30 + i] = 0;
        ppu_.cgram[0xC0 + i] = 0;
    }
    setLines();
}

void TheEndScene::initStar(Star& star) {
    const std::vector<uint8_t>& table = data_.random;
    const uint8_t first = table[random_++];
    star.horizontal = first & 7;
    star.x = static_cast<uint8_t>((first & 0x3F) + 0x60);
    const uint8_t second = table[first];
    star.vertical = second & 7 ? second & 7 : 1;
    const uint8_t speeds = static_cast<uint8_t>(star.horizontal | star.vertical);
    star.y = static_cast<uint8_t>((second & 0x1F) + 0x60);
    const uint8_t third = table[second] & 0x3F;
    star.counter = third;
    star.rate = third & 0x0F ? third & 0x0F : 1;
    if (speeds >= 4 && star.rate < 7) {
        star.rate = static_cast<uint8_t>(star.rate * 2);
        star.counter >>= 1;
    }
    star.tile = 0x1F;
    star.level = 0;
}

void TheEndScene::resetStars() {
    for (size_t n = kStars; n-- > 0;) {
        if (stars_[n].horizontal & 0x80) initStar(stars_[n]);
    }
}

// DrawTheEndStars: a fourth of the stars move each frame. A star that leaves the screen is marked to start again.
void TheEndScene::moveStars() {
    const size_t first = static_cast<size_t>(group_ & 3) * 32;
    for (size_t n = first; n < first + 32; ++n) {
        Star& star = stars_[n];
        const auto gone = [&] { star.horizontal = 0xFF; };
        if (star.x & 0x80) {
            if (star.x + star.horizontal > 0xFF) { gone(); continue; }
            star.x = static_cast<uint8_t>(star.x + star.horizontal);
        } else {
            if (star.x < star.horizontal) { gone(); continue; }
            star.x = static_cast<uint8_t>(star.x - star.horizontal);
        }
        if (star.y >= 0x70) {
            const int moved = star.y + star.vertical;
            if (static_cast<uint8_t>(moved) >= 0xE0 || moved > 0xFF) { gone(); continue; }
            star.y = static_cast<uint8_t>(moved);
        } else {
            if (star.y < star.vertical) { gone(); continue; }
            star.y = static_cast<uint8_t>(star.y - star.vertical);
        }
        star.counter = static_cast<uint8_t>(std::min(0xFF, star.counter + star.rate));
        if (star.counter < 0x40) continue;
        const auto faster = [&](uint8_t level) {
            if (star.level == level) return;
            star.level = level;
            star.horizontal = static_cast<uint8_t>(star.horizontal + (star.horizontal >> 1) + (star.horizontal & 1));
            star.vertical = static_cast<uint8_t>(star.vertical + (star.vertical >> 1) + (star.vertical & 1));
        };
        if (star.counter < 0xC0) {
            star.tile = 0x0F;
        } else if (star.counter < 0xF0) {
            faster(1);
            star.tile = 0x0E;
        } else {
            faster(2);
            star.tile = 0x0D;
        }
    }
    ++group_;
}

// The colors that rise a step each frame: `factor` times the target of each channel, so that the last frame reaches it.
void TheEndScene::fade(const std::array<uint8_t, 32>& target, std::array<int, 48>& accumulator, int factor, uint16_t* destination) {
    for (size_t i = 0; i < 16; ++i) {
        const int word = target[i * 2] | target[i * 2 + 1] << 8;
        int color = 0;
        for (int channel = 0; channel < 3; ++channel) {
            int& value = accumulator[i * 3 + static_cast<size_t>(channel)];
            value += (word >> (channel * 5) & 31) * factor;
            color |= std::min(31, value >> 8) << (channel * 5);
        }
        destination[i] = static_cast<uint16_t>(color);
    }
}

// The lines of the screen: the registers that the HDMA sets and the scroll of the letters.
void TheEndScene::setLines() {
    for (size_t y = 0; y < ppu_.lines.size(); ++y) {
        PpuLine& line = ppu_.lines[y];
        line.bgMode = 1;
        line.cgwsel = 2;
        line.cgadsub = 0;
        line.w12sel = 0x33;
        line.wobjsel = 0x08;
        line.tmw = 0x13;
        const bool open = static_cast<int>(y) >= kWindowTop && static_cast<int>(y) < kWindowTop + kWindowLines;
        line.window = open ? std::array<uint8_t, 4>{0x4E, 0xB1, 0x4E, 0xB1} : std::array<uint8_t, 4>{0x18, 0x10, 0x18, 0x10};
        line.hofs = {0x00B6, 0x00B2, 0};
        line.vofs[0] = static_cast<uint16_t>(scrollRows_);
        const int wave = static_cast<int>(y) - kWaveTop;
        line.vofs[1] = wave >= 0 && wave < kWaveLines ? kWave[(waveIndex_ + wave) % 12] : 0;
        line.vofs[2] = 0;
    }
}

// TheEndNMI: the sprites and the two palettes go to the picture processor, one fourth of the stars move, and the
// wave of the reflection shifts every sixth frame.
void TheEndScene::nmi() {
    ppu_.sprites.clear();
    for (const Star& star : stars_) {
        PpuSprite sprite;
        sprite.x = star.x;
        sprite.y = star.y + 1;
        sprite.tile = star.tile;
        sprite.attr = 0x38;
        ppu_.sprites.push_back(sprite);
    }
    std::copy(letterPalette_.begin(), letterPalette_.end(), ppu_.cgram.begin() + 0x30);
    std::copy(starPalette_.begin(), starPalette_.end(), ppu_.cgram.begin() + 0xC0);
    moveStars();
    if (--scrollCounter_ == 0) {
        scrollCounter_ = 6;
        waveIndex_ = scrollIndex_;
        scrollIndex_ = (scrollIndex_ + 1) % 12;
    }
}

void TheEndScene::tick(const InputState& input) {
    if (done_) return;
    const bool pressed = input.confirm && !previousConfirm_;
    previousConfirm_ = input.confirm;
    nmi();
    const auto palette = [&](int from) {
        std::array<uint8_t, 32> colors{};
        std::copy_n(data_.theEndPal.begin() + from, 32, colors.begin());
        return colors;
    };
    switch (phase_) {
        case 0:  // the screen comes on, then the stars start again until the music gives its first sign
            ppu_.forceBlank = false;
            ppu_.brightness = 15;
            ++phase_;
            [[fallthrough]];
        case 1:
            resetStars();
            if (sound_.port(1) != 0 || ++wait_ > kWaitLimit) {
                phase_ = 2;
                wait_ = 0;
            }
            break;
        case 2:  // the letters scroll up behind the window, a row of pixels every other frame
            if (--scrollWait_ == 0) {
                scrollWait_ = 2;
                if (++scrollRows_ == 0xC1) {
                    --scrollRows_;
                    phase_ = 3;
                }
            }
            break;
        case 3:
            resetStars();
            if (sound_.port(1) == 2 || ++wait_ > kWaitLimit) {
                phase_ = 4;
                wait_ = kLetterFrames;
            }
            break;
        case 4:  // the reflection fades in
            fade(palette(kLettersPal), letterFade_, 2, letterPalette_.data());
            resetStars();
            if (--wait_ == 0) {
                phase_ = 5;
                wait_ = 0;
            }
            break;
        case 5:
            resetStars();
            if (sound_.port(3) == 0 || ++wait_ > kWaitLimit) {
                sound_.command(FF6_SOUND_PLAY_SONG, 1, 0);  // the prelude, silent for a while
                phase_ = 6;
                wait_ = kPreludeFrames;
            }
            break;
        case 6:
            resetStars();
            if (--wait_ == 0) {
                sound_.command(FF6_SOUND_PLAY_SONG, 1, FF6_SOUND_FULL_VOLUME);
                phase_ = 7;
                wait_ = kStarFrames;
            }
            break;
        case 7:  // the stars fade in
            fade(palette(kStarsPal), starFade_, 4, starPalette_.data());
            resetStars();
            if (--wait_ == 0) phase_ = 8;
            break;
        default:
            resetStars();
            if (pressed) done_ = true;
            break;
    }
    setLines();
}

}  // namespace ff6
