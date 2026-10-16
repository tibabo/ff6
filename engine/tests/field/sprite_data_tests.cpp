#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/map.hpp"
#include "field/sprite_data.hpp"

void testSpriteData() {
    CHECK(ff6::stopFrame(ff6::Down) == 1 && ff6::stopFrame(ff6::Right) == 10);
    CHECK(ff6::moveFrame(ff6::Down, 0) == 1 && ff6::moveFrame(ff6::Down, 8) == 2);
    CHECK(ff6::moveFrame(ff6::Down, 16) == 1 && ff6::moveFrame(ff6::Down, 24) == 0);  // the walk cycle

    // Graphics 0 with palette 1: index 3 is the second color, index 0 stays transparent.
    ff6::CommonData common = test::makeCommon();
    common.spriteGfx.assign(FF6_SPRITE_SHEET_BYTES, 0);
    common.spriteGfx[0] = 3;  // frame 0, top-left pixel
    common.spriteGfx[FF6_FRAME_HEIGHT * FF6_FRAME_WIDTH] = 3;  // frame 1, top-left pixel
    common.spritePalettes.assign(2 * FF6_SPRITE_PAL_COLORS, 0);
    common.spritePalettes[FF6_SPRITE_PAL_COLORS + 3] = 0x001F;
    const std::vector<uint8_t> strip = ff6::buildSpriteStrip(common, 0, 1);
    const int stripWidth = FF6_FRAME_WIDTH * FF6_FRAME_COUNT;
    CHECK(strip.size() == static_cast<size_t>(stripWidth) * FF6_FRAME_HEIGHT * FF6_BYTES_PER_PIXEL);
    CHECK(strip[0] == 255 && strip[1] == 0 && strip[3] == 255);
    CHECK(strip[4 * FF6_FRAME_WIDTH] == 255 && strip[4 * FF6_FRAME_WIDTH + 3] == 255);  // frame 1 starts one frame to the right
    CHECK(strip[4] == 0 && strip[7] == 0);
    // Out of range graphics or palettes give an empty strip instead of failing.
    const std::vector<uint8_t> missing = ff6::buildSpriteStrip(common, 5, 1);
    CHECK(missing.size() == strip.size() && missing[3] == 0);

    // Rows are copied to the screen; transparent pixels and anything off the screen are skipped.
    std::vector<uint8_t> screen(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL, 9);
    ff6::drawSpriteRows(screen.data(), strip, 0, 0, FF6_SPRITE_TOP_HEIGHT, 10, 20);
    const auto at = [&](int x, int y) { return &screen[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL]; };
    CHECK(at(10, 20)[0] == 255 && at(10, 20)[3] == 255);
    CHECK(at(11, 20)[0] == 9);  // transparent: the screen is untouched
    ff6::drawSpriteRows(screen.data(), strip, 0, 0, FF6_SPRITE_TOP_HEIGHT, -1, -1);  // partly off the screen: no crash
    ff6::drawSpriteRows(screen.data(), strip, 0, 0, FF6_SPRITE_TOP_HEIGHT, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT);
    CHECK(at(0, 0)[0] == 9);
}
