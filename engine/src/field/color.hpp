#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "constants.hpp"

namespace ff6 {

// One entry of the color math table: what InitColorMath puts in $2131, $212c and $212d.
struct ColorMath {
    uint8_t designation = 0;  // layers that get the color math; FF6_COLOR_MATH_HALF and _SUBTRACT select the operation
    uint8_t mainScreen = FF6_SCREEN_BG1;  // layers shown on the main screen (the first layer is always on)
    uint8_t subScreen = 0;  // layers that the main screen is blended with
};

ColorMath decodeColorMath(const std::array<uint8_t, FF6_COLOR_MATH_BYTES>& bytes);

// A layer of the screen, drawn in the order of an array of them: back to front.
struct ScreenLayer {
    uint8_t bit;  // FF6_SCREEN_BG1 ... FF6_SCREEN_OBJ
    const uint8_t* pixels;  // RGBA, FF6_SCREEN_WIDTH x FF6_SCREEN_HEIGHT; a zero alpha is transparent
};

// What the PPU does at the end: builds the main screen and the sub screen from their layers, then adds the sub
// screen to the main pixels that come from a designated layer (or subtracts it, or averages with it). Where the sub
// screen is empty the fixed color is black, so nothing changes. `out` is RGBA with every pixel opaque.
void composeScreen(const ColorMath& math, const uint8_t backdrop[FF6_COLOR_CHANNELS], const ScreenLayer* layers,
                   size_t count, uint8_t* out);

}  // namespace ff6
