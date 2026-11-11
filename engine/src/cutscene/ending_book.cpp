#include "cutscene/ending_book.hpp"

#include <algorithm>

namespace ff6 {
namespace {

constexpr int kBase = 0x8A70;  // the address in its bank of the first byte of the animation data
constexpr int kBookPalette = 0x967C, kTablePalette = 0x969C, kLetterPalette = 0x9754, kLightPalette = 0x96DC, kGlowPalette = 0x96FC;
// Where each page is in the graphics (the tiles are read 0x4000 bytes into them), a rectangle of 14 x 6 tiles.
constexpr int kPages[6] = {0x0BC8, 0x0BE4, 0x0D48, 0x0D64, 0x0EC8, 0x0EE4};
constexpr int kPageWidth = 14, kPageHeight = 6, kPageRow = 10, kPageColumn = 12;
constexpr int kTurning[] = {1, 2, 3, 4, -1};  // BookAnim1 and BookAnim2: the pages 2, 3, 4 and 5, again and again
constexpr int kClosing[] = {1, 2, 3, 5, 0, -2};  // BookAnimEnd: the pages 2, 3, 4, 6, then the first one for good
constexpr int kPageFrames = 8;
constexpr int kBackgroundMap = 0x38A0, kSecondMap = 0x4448, kMapBytes = 0x780, kScreenColumns = 32;
constexpr int kAndYouAddress = 0x68;
constexpr int kAndYouData = 0xF8AB - 0xF450;  // the sprites of "and you" in the second block of animations

}  // namespace

EndingBook::EndingBook(const TitleData& data, int state) : data_(data), andYou_(state == 74) {
    ppu_.objsel = 0x03;
    ppu_.bgsc = {0x03, 0x11, 0x19};
    ppu_.nba12 = 0x33;
    ppu_.nba34 = 0x22;
    ppu_.tm = 0x17;
    ppu_.ts = 0x02;
    ppu_.brightness = 15;
    ppu_.forceBlank = false;
    // The characters of the background, in three pieces that LoadEndingBGGfx puts at $3000, $4000 and $5000 of the video memory.
    const auto transfer = [&](size_t from, size_t bytes, size_t to) {
        for (size_t i = 0; i + 1 < bytes && from + i + 1 < data.endingGfx1.size(); i += 2) {
            ppu_.vram[to + i / 2] = static_cast<uint16_t>(data.endingGfx1[from + i] | data.endingGfx1[from + i + 1] << 8);
        }
    };
    transfer(0x0000, 0x1F60, 0x3000);
    transfer(0x1F60, 0x0B40, 0x4000);
    transfer(0x2AA0, 0x0E00, 0x5000);
    transfer(kSecondMap, kMapBytes, 0x1000);  // the second background
    // The letters: two bits per pixel in the first planes of the tiles of the sprites.
    for (size_t tile = 0; tile * 16 + 16 <= data.endingFont.size(); ++tile) {
        for (size_t row = 0; row < 8; ++row) {
            const size_t at = tile * 16 + row * 2;
            ppu_.vram[0x7000 + tile * 16 + row] = static_cast<uint16_t>(data.endingFont[at] | data.endingFont[at + 1] << 8);
        }
    }
    // The first background: the same 30 rows on both screens, and the closed book over them.
    for (int screen = 0; screen < 2; ++screen) {
        for (int i = 0; i < kMapBytes / 2; ++i) {
            const size_t at = static_cast<size_t>(kBackgroundMap + i * 2);
            map_[static_cast<size_t>(screen * 0x400 + i)] = static_cast<uint16_t>(data.endingGfx1[at] | data.endingGfx1[at + 1] << 8);
        }
    }
    copyPage(0);
    fadeTo(fades_[0], 32, 2);
    fadeTo(fades_[1], 32, 2);
    if (andYou_) startAnimation(kTurning);
    applyPalettes();
    for (size_t i = 0; i < map_.size(); ++i) ppu_.vram[i] = map_[i];
}

// _c3edcd: the tiles of a page, row by row, into the map of the first screen.
void EndingBook::copyPage(int page) {
    for (int row = 0; row < kPageHeight; ++row) {
        for (int column = 0; column < kPageWidth; ++column) {
            const size_t from = static_cast<size_t>(0x4000 + kPages[page] + row * 0x40 + column * 2);
            if (from + 1 >= data_.endingGfx1.size()) continue;
            map_[static_cast<size_t>((kPageRow + row) * kScreenColumns + kPageColumn + column)] =
                static_cast<uint16_t>(data_.endingGfx1[from] | data_.endingGfx1[from + 1] << 8);
        }
    }
}

void EndingBook::startAnimation(const int* frames) {
    animation_.frames = frames;
    animation_.index = 0;
    animation_.duration = kPageFrames;
    started_ = false;
}

// _c3ee04: the first frame of the task only sets it up; then UpdateAnimData chooses the page that shows, every frame.
void EndingBook::tickAnimation() {
    if (!animation_.frames) return;
    if (!started_) {
        started_ = true;
        return;
    }
    for (;;) {
        if (animation_.duration == 0) {
            ++animation_.index;
            if (animation_.frames[animation_.index] == -2) {  // the last page stays
                animation_.frames = nullptr;
                return;
            }
            if (animation_.frames[animation_.index] < 0) animation_.index = 0;
            animation_.duration = kPageFrames;
            continue;
        }
        --animation_.duration;
        break;
    }
    copyPage(animation_.frames[animation_.index]);
}

uint16_t EndingBook::paletteWord(int address, int index) const {
    const size_t at = static_cast<size_t>(address - kBase + index * 2);
    return static_cast<uint16_t>(data_.endingAnim[at] | data_.endingAnim[at + 1] << 8);
}

void EndingBook::applyPalettes() {
    // The four background palettes fade together; the letters have their own fade.
    static constexpr int kTargets[5] = {kBookPalette, kTablePalette, kLetterPalette, kLightPalette, kGlowPalette};
    static constexpr int kFirst[5] = {16, 32, 128, 80, 96};
    static constexpr size_t kFade[5] = {0, 1, 2, 0, 1};
    for (size_t p = 0; p < 5; ++p) {
        for (int i = 0; i < 16; ++i) {
            const int target = paletteWord(kTargets[p], i);
            int color = 0;
            for (int shift = 0; shift <= 10; shift += 5) color |= ((target >> shift & 31) * fades_[kFade[p]].level / 32) << shift;
            ppu_.cgram[static_cast<size_t>(kFirst[p] + i)] = static_cast<uint16_t>(color);
        }
    }
}

// EndingBigTextTask for the one name of "and you": wait, rise, stay, rise out.
void EndingBook::tickLetters() {
    if (!letterAlive_) return;
    const auto wait = [&] {
        if (letterCounter_ == 0) {
            ++letterState_;
        } else {
            --letterCounter_;
        }
    };
    switch (letterState_) {
        case 0: ++letterState_; letterSpeed_ = 0; wait(); break;
        case 1: case 3: case 5: case 7: wait(); break;
        case 2: letterSpeed_ = -0x40; letterCounter_ = 0x96; letterState_ = 3; wait(); break;
        case 4: letterSpeed_ = 0; letterCounter_ = 0xF0; letterState_ = 5; wait(); break;
        case 6: letterSpeed_ = -0x40; letterCounter_ = 0x12C; letterState_ = 7; wait(); break;
        default: letterAlive_ = false; return;
    }
    letterY_ = (letterY_ + letterSpeed_) & 0xFFFF;
}

void EndingBook::drawLetters() {
    ppu_.sprites.clear();
    if (!andYou_ || !letterAlive_) return;
    // The sprites come from the count that the game gives, 24, which goes past the 12 that it lists.
    const int count = data_.endingAnim2[kAndYouData + 3];
    for (int i = 0; i < count && kAndYouData + 4 + i * 4 + 3 < static_cast<int>(data_.endingAnim2.size()); ++i) {
        const uint8_t* sprite = &data_.endingAnim2[static_cast<size_t>(kAndYouData + 4 + i * 4)];
        PpuSprite out;
        out.large = (sprite[0] & 0x80) != 0;
        out.x = ((sprite[0] & 0x7F) + kAndYouAddress) & 0xFF;
        out.y = (sprite[1] + (letterY_ >> 8) + 1) & 0xFF;
        out.attr = sprite[3];
        out.tile = sprite[2] | (sprite[3] & 1) << 8;
        ppu_.sprites.push_back(out);
    }
}

void EndingBook::tick() {
    // What the states of the scene do, at the frames that their waits add up to.
    if (!andYou_) {
        if (frame_ == 720) startAnimation(kTurning);  // the pages turn
        if (frame_ == 1560) {  // the picture goes: mosaic, fade out, scroll
            mosaicStart_ = frame_;
            fadeTo(fades_[0], 0, 4);
            fadeTo(fades_[1], 0, 4);
            scrollLeft_ = 180;
        }
    } else {
        if (frame_ == 416) {  // the book closes, the letters come
            startAnimation(kClosing);
            fadeTo(fades_[2], 32, 4);
        }
        if (frame_ == 780) {
            mosaicStart_ = frame_;
            fadeTo(fades_[0], 0, 4);
            fadeTo(fades_[1], 0, 4);
        }
    }
    if (frame_ == (andYou_ ? 900 : 1680)) fadeTo(fades_[2], 0, 4);  // FADE_CREDITS: the sprites go to black too
    for (Fade& fade : fades_) {
        if (fade.level == fade.target) continue;
        if (++fade.counter >= fade.speed) {
            fade.counter = 0;
            fade.level += fade.target > fade.level ? 1 : -1;
        }
    }
    tickAnimation();
    if (andYou_) tickLetters();
    if (scrollLeft_ > 0) {  // the background scrolls a pixel every fourth frame
        if ((frame_ & 3) == 0) ++hscroll_;
        --scrollLeft_;
    }
    ++frame_;
    applyPalettes();
    drawLetters();
    for (size_t i = 0; i < map_.size(); ++i) ppu_.vram[i] = map_[i];
    for (PpuLine& line : ppu_.lines) {
        line.bgMode = 0x09;
        line.cgwsel = 0x82;
        line.cgadsub = 0xD1;
        line.hofs[0] = static_cast<uint16_t>(hscroll_);
    }
}

// The screen, with the mosaic that the game grows by a step every 16 frames: it only takes the backgrounds, not the sprites.
void EndingBook::draw(uint8_t* rgba) const {
    ppu_.render(rgba);
    if (mosaicStart_ < 0) return;
    const int size = ((frame_ - mosaicStart_) / 16 + 1) & 15;
    if (size <= 1) return;
    Ppu backgrounds = ppu_;
    backgrounds.sprites.clear();
    std::vector<uint8_t> plain(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
    backgrounds.render(plain.data());
    for (int y = 0; y < FF6_SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
            const size_t at = (static_cast<size_t>(y) * FF6_SCREEN_WIDTH + static_cast<size_t>(x)) * FF6_BYTES_PER_PIXEL;
            if (!std::equal(plain.begin() + static_cast<std::ptrdiff_t>(at), plain.begin() + static_cast<std::ptrdiff_t>(at) + FF6_BYTES_PER_PIXEL, rgba + at)) continue;  // a sprite
            const size_t from = (static_cast<size_t>(y / size * size) * FF6_SCREEN_WIDTH + static_cast<size_t>(x / size * size)) * FF6_BYTES_PER_PIXEL;
            std::copy_n(plain.begin() + static_cast<std::ptrdiff_t>(from), FF6_BYTES_PER_PIXEL, rgba + at);
        }
    }
}

std::unique_ptr<EndingBook> makeEndingBook(const TitleData& data, int state) {
    if ((state != 45 && state != 74) || data.endingGfx1.size() < FF6_TITLE_ENDING_GFX_1_BYTES || data.endingAnim.empty()) return nullptr;
    return std::make_unique<EndingBook>(data, state);
}

}  // namespace ff6
