#include "field/stage.hpp"

#include <algorithm>
#include <cmath>

namespace ff6 {
namespace {

constexpr int kLevels[4] = {4, 8, 16, 31};
constexpr int kShakeLimits[4] = {0, 3, 6, 12};  // ShakeAndTbl
constexpr int kFixedRates[4] = {0x81, 0x82, 0x84, 0x84};
constexpr int kChannelBlue = 0x80, kChannelGreen = 0x40, kChannelRed = 0x20;

struct Rgb {
    int r, g, b;
};

Rgb unpack(uint16_t color) { return {color & 31, (color >> 5) & 31, (color >> 10) & 31}; }

uint16_t pack(const Rgb& color) {
    return static_cast<uint16_t>(std::clamp(color.r, 0, 31) | (std::clamp(color.g, 0, 31) << 5) | (std::clamp(color.b, 0, 31) << 10));
}

}  // namespace

void modifyColors(int command, uint16_t* active, const uint16_t* base, int first, int last) {
    const int function = (command >> 5) & 7;
    if (function == 0 || function == 7) {
        std::copy(base, base + FF6_MAP_PALETTE_COLORS, active);
        return;
    }
    const int level = kLevels[command & 3];
    Rgb target = {(command & 0x10) ? level : 0, (command & 0x08) ? level : 0, (command & 0x04) ? level : 0};
    if (command & 0x80) target = {~target.r & 31, ~target.g & 31, ~target.b & 31};  // the subtracting functions
    for (int i = first; i < last && i < FF6_MAP_PALETTE_COLORS; ++i) {
        Rgb c = unpack(active[i]);
        const Rgb original = unpack(base[i]);
        switch (function) {
            case 1:
                c = {c.r + (c.r < target.r), c.g + (c.g < target.g), c.b + (c.b < target.b)};
                break;
            case 2: c = {31, 31, 31}; break;
            case 3: c = {c.r - (c.r != original.r), c.g - (c.g != original.g), c.b - (c.b != original.b)}; break;
            case 4: c = {c.r - (c.r > target.r), c.g - (c.g > target.g), c.b - (c.b > target.b)}; break;
            case 5: c = {c.r - (target.r + 4), c.g - (target.g + 4), c.b - (target.b + 4)}; break;
            case 6: c = {c.r + (c.r != original.r), c.g + (c.g != original.g), c.b + (c.b != original.b)}; break;
            default: break;
        }
        active[i] = pack(c);
    }
}

void FieldStage::reset() { *this = FieldStage(); }

int FieldStage::random() {
    seed_ = seed_ * 1103515245u + 12345u;
    return static_cast<int>((seed_ >> 16) & 0xFF);
}

void FieldStage::fixedColor(bool subtract, int bgrsssiii) {
    if (fixedActive_ && fixedCounter_ != 0) {  // it fades out from where it is
        fixedRate_ &= 0x7F;
        return;
    }
    fixedActive_ = true;
    fixedSubtract_ = subtract;
    fixedCounter_ = 0;
    fixedTarget_ = (bgrsssiii & 0xE0) | (((bgrsssiii & 7) * 4 + 3) & 0x1F);
    fixedRate_ = kFixedRates[(bgrsssiii >> 3) & 3];
}

void FieldStage::flash(int bgr) {
    fixedActive_ = true;
    fixedSubtract_ = false;
    fixedTarget_ = bgr & 0xE0;
    fixedCounter_ = 0xF8;
    fixedRate_ = 8;
}

void FieldStage::fixedOff() {
    fixedActive_ = false;
    fixedCounter_ = fixedRate_ = 0;
}

void FieldStage::fixedRgb(int rgb[3]) const {
    const int intensity = fixedCounter_ >> 3;
    rgb[0] = (fixedTarget_ & kChannelRed) ? std::min(intensity, 31) * 8 : 0;
    rgb[1] = (fixedTarget_ & kChannelGreen) ? std::min(intensity, 31) * 8 : 0;
    rgb[2] = (fixedTarget_ & kChannelBlue) ? std::min(intensity, 31) * 8 : 0;
}

void FieldStage::mosaic(int speed) {
    mosaicSpeed_ = speed;
    mosaicCounter_ = 0x1E00;
}

int FieldStage::mosaicSize() const {
    if (mosaicSpeed_ == 0) return 1;
    const int index = std::clamp(mosaicCounter_ >> 8, 0, 29);
    return (index < 16 ? index : 30 - index) + 1;
}

// UpdatePyramid: the apex is 48 pixels above, the other two corners are on an ellipse of 64 x 32 that turns.
void FieldStage::pyramidCorners(int corners[3][2]) const {
    const int angle = ((pyramidCounter_ & 0xC0) + ((pyramidCounter_ >> 8) & 0xFF) * 2) & 0xFF;
    corners[0][0] = 0;
    corners[0][1] = -0x30;
    for (int i = 0; i < 2; ++i) {
        const double turn = 2 * 3.14159265358979 * (angle + (i == 0 ? -0x20 : 0x20)) / 256.0;
        corners[i + 1][0] = static_cast<int>(0x40 * std::cos(turn));
        corners[i + 1][1] = static_cast<int>(0x20 * std::sin(turn));
    }
}

void FieldStage::update(int frame) {
    if (pyramidOn_) pyramidCounter_ = (pyramidCounter_ + 0x40) & 0xFFFF;
    if (fixedActive_) {
        if (fixedRate_ & 0x80) {
            fixedCounter_ = (fixedCounter_ + (fixedRate_ & 0x7F)) & 0xFF;
            if (((fixedTarget_ & 0x1F) << 3) < fixedCounter_) fixedCounter_ &= 0xF8;
        } else if (fixedCounter_ == 0) {
            fixedOff();
        } else {
            fixedCounter_ = std::max(0, fixedCounter_ - fixedRate_);
        }
    }
    if (frame & 1) {  // UpdateShake
        shakeAmplitude_ = shakeAmplitude_ * 0xC0 >> 8;
        std::fill(shakeOffsets_, shakeOffsets_ + 4, 0);
    } else {
        const int amplitude = shakeBits_ & 3;
        if (amplitude != 0) {
            const int frequency = (shakeBits_ & 0x0C) >> 2;
            if (frequency == 0) {
                shakeBits_ &= 0xFC;
                shakeAmplitude_ = kShakeLimits[amplitude];
            } else if ((random() & kShakeLimits[frequency]) == 0) {
                shakeAmplitude_ = random() & kShakeLimits[amplitude];
            }
        }
        for (int layer = 0; layer < 4; ++layer) shakeOffsets_[layer] = (shakeBits_ & (0x10 << layer)) ? shakeAmplitude_ : 0;
    }
    if (mosaicSpeed_ != 0) {
        mosaicCounter_ -= (mosaicSpeed_ + 1) * 16;
        if (mosaicCounter_ < 0) {
            mosaicCounter_ = 0;
            mosaicSpeed_ = 0;
        }
    }
    if (flashlightOn_) {
        if (flashlightSize_ < flashlightTarget_) flashlightSize_ += 2;
        else if (flashlightSize_ > flashlightTarget_) flashlightSize_ -= 2;
        if (flashlightSize_ <= 0) flashlightOn_ = false;
    }
}

}  // namespace ff6
