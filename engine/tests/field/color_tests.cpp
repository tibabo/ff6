#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/color.hpp"

namespace {

constexpr size_t kPixels = static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT;

// A full-screen layer that is transparent except for its first pixel.
std::vector<uint8_t> layerWith(int r, int g, int b, int a) {
    std::vector<uint8_t> pixels(kPixels * FF6_BYTES_PER_PIXEL, 0);
    pixels[0] = static_cast<uint8_t>(r);
    pixels[1] = static_cast<uint8_t>(g);
    pixels[2] = static_cast<uint8_t>(b);
    pixels[3] = static_cast<uint8_t>(a);
    return pixels;
}

bool firstPixelIs(const std::vector<uint8_t>& out, int r, int g, int b) {
    return out[0] == r && out[1] == g && out[2] == b && out[3] == 255;
}

}  // namespace

void testColor() {
    const uint8_t backdrop[FF6_COLOR_CHANNELS] = {10, 20, 30};
    std::vector<uint8_t> out(kPixels * FF6_BYTES_PER_PIXEL);
    const std::vector<uint8_t> ground = layerWith(100, 100, 100, 255);
    const std::vector<uint8_t> steam = layerWith(60, 80, 250, 255);
    const ff6::ScreenLayer layers[] = {{FF6_SCREEN_BG1, ground.data()}, {FF6_SCREEN_BG3, steam.data()}};

    ff6::ColorMath none;  // no color math: the top layer wins and empty pixels show the backdrop
    none.mainScreen = FF6_SCREEN_BG1 | FF6_SCREEN_BG3;
    ff6::composeScreen(none, backdrop, layers, 2, out.data());
    CHECK(firstPixelIs(out, 60, 80, 250));
    CHECK(out[4] == 10 && out[5] == 20 && out[6] == 30 && out[7] == 255);  // the second pixel is the backdrop

    // Narshe: bg3 is added to what is under it (bg1), and the sum stops at white.
    ff6::ColorMath add;
    add.designation = FF6_SCREEN_BG3;
    add.mainScreen = FF6_SCREEN_BG1 | FF6_SCREEN_BG3;
    add.subScreen = FF6_SCREEN_BG1;
    ff6::composeScreen(add, backdrop, layers, 2, out.data());
    CHECK(firstPixelIs(out, 160, 180, 255));

    ff6::ColorMath half = add;  // most bg3 maps: the average of the two
    half.designation |= FF6_COLOR_MATH_HALF;
    ff6::composeScreen(half, backdrop, layers, 2, out.data());
    CHECK(firstPixelIs(out, 80, 90, 175));

    ff6::ColorMath subtract;  // darkens: bg1 minus bg3, never below black
    subtract.designation = FF6_SCREEN_BG1 | FF6_COLOR_MATH_SUBTRACT;
    subtract.mainScreen = FF6_SCREEN_BG1;
    subtract.subScreen = FF6_SCREEN_BG3;
    ff6::composeScreen(subtract, backdrop, layers, 2, out.data());
    CHECK(firstPixelIs(out, 40, 20, 0));

    // A layer that is not designated is left alone; so is a pixel with nothing on the sub screen.
    ff6::ColorMath other = add;
    other.designation = FF6_SCREEN_BG1;
    ff6::composeScreen(other, backdrop, layers, 2, out.data());
    CHECK(firstPixelIs(out, 60, 80, 250));
    ff6::ColorMath empty = add;
    empty.subScreen = FF6_SCREEN_OBJ;
    ff6::composeScreen(empty, backdrop, layers, 2, out.data());
    CHECK(firstPixelIs(out, 60, 80, 250));

    // Transparent pixels never hide the layers below, and layers outside the main screen are not shown.
    const std::vector<uint8_t> glass = layerWith(1, 2, 3, 0);
    const ff6::ScreenLayer over[] = {{FF6_SCREEN_BG1, ground.data()}, {FF6_SCREEN_BG2, glass.data()}};
    ff6::ColorMath plain;
    plain.mainScreen = FF6_SCREEN_BG1 | FF6_SCREEN_BG2;
    ff6::composeScreen(plain, backdrop, over, 2, out.data());
    CHECK(firstPixelIs(out, 100, 100, 100));
    // The decoder always turns bg1 on in the main screen.
    const std::array<uint8_t, FF6_COLOR_MATH_BYTES> bytes = {0x44, FF6_SCREEN_BG2, 0x13};
    const ff6::ColorMath decoded = ff6::decodeColorMath(bytes);
    CHECK(decoded.designation == 0x44 && decoded.mainScreen == (FF6_SCREEN_BG1 | FF6_SCREEN_BG2) && decoded.subScreen == 0x13);
}
