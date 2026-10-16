#include "field/color.hpp"

#include <algorithm>

namespace ff6 {

ColorMath decodeColorMath(const std::array<uint8_t, FF6_COLOR_MATH_BYTES>& bytes) {
    ColorMath math;
    math.designation = bytes[0];
    math.mainScreen = static_cast<uint8_t>(bytes[1] | FF6_SCREEN_BG1);
    math.subScreen = bytes[2];
    return math;
}

void composeScreen(const ColorMath& math, const uint8_t backdrop[FF6_COLOR_CHANNELS], const ScreenLayer* layers,
                   size_t count, uint8_t* out) {
    const size_t pixelCount = static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT;
    const bool subtract = (math.designation & FF6_COLOR_MATH_SUBTRACT) != 0;
    const bool half = (math.designation & FF6_COLOR_MATH_HALF) != 0;
    for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const size_t offset = pixel * FF6_BYTES_PER_PIXEL;
        const uint8_t* main = backdrop;
        const uint8_t* sub = nullptr;
        uint8_t source = FF6_SCREEN_BACKDROP;
        for (size_t i = 0; i < count; ++i) {
            const uint8_t* color = layers[i].pixels + offset;
            if (color[FF6_ALPHA_OFFSET] == 0) continue;
            if (layers[i].bit & math.mainScreen) {
                main = color;
                source = layers[i].bit;
            }
            if (layers[i].bit & math.subScreen) sub = color;
        }
        const bool blend = (math.designation & source) != 0 && sub != nullptr;
        for (int channel = 0; channel < FF6_COLOR_CHANNELS; ++channel) {
            int value = main[channel];
            if (blend) {
                value = subtract ? value - sub[channel] : value + sub[channel];
                if (half) value >>= 1;
                value = std::max(0, std::min(FF6_COLOR_MAX, value));
            }
            out[offset + static_cast<size_t>(channel)] = static_cast<uint8_t>(value);
        }
        out[offset + FF6_ALPHA_OFFSET] = FF6_COLOR_MAX;
    }
}

}  // namespace ff6
