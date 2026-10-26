#include "menu/window_style.hpp"

#include <algorithm>

#include "data/pack.hpp"

namespace ff6 {

std::vector<uint8_t> buildWindowTiles(const MenuData& menu, const GameConfig& config) {
    const size_t pattern = static_cast<size_t>(std::min(config.windowPattern(), FF6_MENU_WINDOW_COUNT - 1));
    if (menu.windowGfx.size() < (pattern + 1) * FF6_MENU_WINDOW_GFX_BYTES) return {};  // no patterns: nothing to draw with
    const uint8_t* gfx = &menu.windowGfx[pattern * FF6_MENU_WINDOW_GFX_BYTES];
    uint8_t colors[FF6_MENU_WINDOW_COLORS][FF6_BYTES_PER_PIXEL];
    for (size_t i = 0; i < FF6_MENU_WINDOW_COLORS; ++i) colorToRgba(config.windowColors[pattern][i], colors[i]);
    std::vector<uint8_t> tiles(static_cast<size_t>(FF6_WINDOW_TILE_COUNT) * FF6_WINDOW_TILE_BYTES, 0);
    for (int tile = 0; tile < FF6_WINDOW_TILE_COUNT; ++tile) {
        const uint8_t* source = gfx + tile * FF6_MENU_CURSOR_TILE_BYTES;
        for (int row = 0; row < FF6_WINDOW_TILE_SIZE; ++row) {
            for (int column = 0; column < FF6_WINDOW_TILE_SIZE; ++column) {
                const int shift = FF6_WINDOW_TILE_SIZE - 1 - column;
                const int index = ((source[row * 2] >> shift) & 1) | (((source[row * 2 + 1] >> shift) & 1) << 1) |
                                  (((source[row * 2 + 16] >> shift) & 1) << 2) | (((source[row * 2 + 17] >> shift) & 1) << 3);
                if (index == 0 || index > FF6_MENU_WINDOW_COLORS) continue;  // clear
                std::copy(colors[index - 1], colors[index - 1] + FF6_BYTES_PER_PIXEL,
                          tiles.begin() + static_cast<std::ptrdiff_t>((static_cast<size_t>(tile) * FF6_WINDOW_TILE_SIZE * FF6_WINDOW_TILE_SIZE +
                                                                       static_cast<size_t>(row * FF6_WINDOW_TILE_SIZE + column)) * FF6_BYTES_PER_PIXEL));
            }
        }
    }
    return tiles;
}

}  // namespace ff6
