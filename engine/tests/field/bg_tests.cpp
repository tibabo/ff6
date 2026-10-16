#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/bg.hpp"

namespace {

constexpr size_t kScreenBytes = static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL;

bool pixelIs(const std::vector<uint8_t>& image, int x, int y, int r, int g, int b, int a) {
    const uint8_t* p = &image[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL];
    return p[0] == r && p[1] == g && p[2] == b && p[3] == a;
}

}  // namespace

void testBg() {
    // Tile 3 has palette index 5 in its top-left pixel; the tile word of cell 1 uses palette 1.
    ff6::MapData map = test::makeMap();
    map.vram[3 * FF6_TILE4_BYTES + 0] = 0x80;  // plane 0, row 0
    map.vram[3 * FF6_TILE4_BYTES + FF6_TILE2_BYTES] = 0x80;  // plane 2, row 0
    map.palette[1 * FF6_PALETTE_COLORS + 5] = 0x001F;  // red
    map.tilesets[0][1] = 3 | (1 << FF6_TILE_WORD_PAL_SHIFT);
    std::vector<uint8_t> low(kScreenBytes, 0), high(kScreenBytes, 0);
    ff6::renderBg(map, FF6_BG1, 0, 0, low.data(), high.data());
    CHECK(pixelIs(low, 0, 0, 255, 0, 0, 255));
    CHECK(pixelIs(low, 1, 0, 0, 0, 0, 0));  // index 0 is transparent
    CHECK(pixelIs(high, 0, 0, 0, 0, 0, 0));
    ff6::renderBg(map, FF6_BG1, 16, 0, low.data(), high.data());  // scrolled by one tile: same picture
    CHECK(pixelIs(low, 0, 0, 255, 0, 0, 255));
    ff6::renderBg(map, FF6_BG1, 1, 0, low.data(), high.data());
    CHECK(pixelIs(low, 15, 0, 255, 0, 0, 255));  // the opaque pixel moved left by one pixel, wrapping around

    // The priority bit sends the pixels to the high buffer; a horizontal flip moves the pixel to the right edge.
    std::fill(low.begin(), low.end(), 0);
    map.tilesets[0][1] |= FF6_TILE_WORD_PRIORITY | FF6_TILE_WORD_HFLIP;
    ff6::renderBg(map, FF6_BG1, 0, 0, low.data(), high.data());
    CHECK(pixelIs(high, 7, 0, 255, 0, 0, 255));
    CHECK(pixelIs(low, 7, 0, 0, 0, 0, 0));

    // bg3: cell 1 is metatile 1, whose first 2bpp tile is number 4; palette 3, color 2.
    ff6::MapData bg3 = test::makeMap();
    bg3.layers[FF6_BG3].cells[0] = 1;
    bg3.bg3Gfx[4 * FF6_TILE2_BYTES + 1] = 0x80;  // plane 1, row 0, leftmost pixel
    bg3.bg3Palette[1] = 3 << FF6_BG3_PAL_SHIFT;
    bg3.palette[3 * FF6_BG3_COLORS + 2] = 0x03E0;  // green
    std::fill(low.begin(), low.end(), 0);
    std::fill(high.begin(), high.end(), 0);
    ff6::renderBg(bg3, FF6_BG3, 0, 0, low.data(), high.data());
    CHECK(pixelIs(low, 0, 0, 0, 255, 0, 255));
    bg3.prop[FF6_MAP_PROP_FLAGS] = FF6_BG3_FOREGROUND_FLAG;  // in front of everything
    std::fill(low.begin(), low.end(), 0);
    ff6::renderBg(bg3, FF6_BG3, 0, 0, low.data(), high.data());
    CHECK(pixelIs(high, 0, 0, 0, 255, 0, 255) && pixelIs(low, 0, 0, 0, 0, 0, 0));
    // Flipping the cell horizontally flips its four tiles and swaps them: the pixel lands on the right edge.
    bg3.layers[FF6_BG3].cells[0] = 1 | FF6_BG3_HFLIP;
    std::fill(high.begin(), high.end(), 0);
    ff6::renderBg(bg3, FF6_BG3, 0, 0, low.data(), high.data());
    CHECK(pixelIs(high, 15, 0, 0, 255, 0, 255));
}
