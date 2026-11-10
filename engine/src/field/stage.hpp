#pragma once

#include <cstdint>

#include "constants.hpp"

namespace ff6 {

// The color functions of the event commands 0x50 to 0x53 (src/field/color.asm), one step of them on the colors
// `first` to `last` - 1 of `active`, which `base` holds the unmodified colors of. `command` is mmmrgbii: the function
// (0 and 7 restore, 1 raise to the intensity, 2 add, 3 lower back, 4 lower to the intensity, 5 subtract, 6 raise
// back), the channels, and the intensity (4, 8, 16 or 31).
void modifyColors(int command, uint16_t* active, const uint16_t* base, int first, int last);

// What the commands that stage a scene of the field do on the screen, a frame at a time: the fixed color that fades
// in and out, the shake, the mosaic, the flashlight, and the speeds of the layers that scroll by themselves.
class FieldStage {
public:
    void reset();
    // One frame (UpdateFixedColor, UpdateShake, UpdateMosaic, UpdateFlashlight); `frame` is the counter of frames.
    void update(int frame);

    // 0x56 and 0x57: the fixed color goes to its intensity, or fades out if it was already there; 0x55 flashes
    // from white (bgr-----) down; 0x54 ends it.
    void fixedColor(bool subtract, int bgrsssiii);
    void flash(int bgr);
    void fixedOff();
    bool fixedActive() const { return fixedActive_; }
    bool fixedSubtract() const { return fixedSubtract_; }
    // The color that the screen gets (FF6_COLOR_MAX at most).
    void fixedRgb(int rgb[3]) const;

    // 0x58: o321ffaa. The vertical offset of a layer (0 to 2 for bg1 to bg3, 3 for the objects).
    void shake(int bits) { shakeBits_ = bits; }
    int shakeOffset(int layer) const { return shakeOffsets_[layer]; }

    // 0x62: the size of the blocks in pixels, 1 for none.
    void mosaic(int speed);
    int mosaicSize() const;

    // 0x63: the radius in pixels that the light has now (0 for none).
    void flashlight(int radius) { flashlightTarget_ = (radius & 0x1F) * 2; flashlightOn_ = true; }
    int flashlightRadius() const { return flashlightOn_ ? flashlightSize_ : -1; }

    // 0xA6 and 0xA7: a triangle of light that turns around an object, which the three corners of it give (x, y) in pixels
    // from the center of the object, outside it the screen is black.
    void pyramid(bool on, int object) { pyramidOn_ = on; pyramidObject_ = object; pyramidCounter_ = 0; }
    bool pyramidOn() const { return pyramidOn_; }
    int pyramidObject() const { return pyramidObject_; }
    void pyramidCorners(int corners[3][2]) const;

    // 0x5D to 0x5F and 0x70 to 0x72: bg1 to bg3 move by themselves, in 1/256 pixel for each frame.
    void scrollSpeed(int layer, int x, int y) { speedX_[layer] = x; speedY_[layer] = y; }
    int speedX(int layer) const { return speedX_[layer]; }
    int speedY(int layer) const { return speedY_[layer]; }

private:
    int random();

    bool fixedActive_ = false, fixedSubtract_ = false;
    int fixedRate_ = 0, fixedCounter_ = 0, fixedTarget_ = 0;
    int shakeBits_ = 0, shakeAmplitude_ = 0, shakeOffsets_[4] = {0, 0, 0, 0};
    int mosaicSpeed_ = 0, mosaicCounter_ = 0;
    int flashlightTarget_ = 0, flashlightSize_ = 0;
    bool flashlightOn_ = false;
    int speedX_[3] = {0, 0, 0}, speedY_[3] = {0, 0, 0};
    bool pyramidOn_ = false;
    int pyramidObject_ = 0, pyramidCounter_ = 0;
    uint32_t seed_ = 1;
};

}  // namespace ff6
