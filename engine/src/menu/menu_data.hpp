#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "constants.hpp"

namespace ff6 {

// menu.ff6data: what the menus draw with, raw as the game keeps it.
struct MenuData {
    std::vector<uint8_t> fontTiles;  // FF6_MENU_FONT_TILES tiles of 2bpp
    std::array<uint16_t, FF6_MENU_FONT_COLORS> fontPalette{};  // BGR555
    std::vector<uint8_t> cursorTiles;  // the four 4bpp tiles of the hand: upper left, upper right, lower left, lower right
    std::array<uint16_t, FF6_MENU_CURSOR_PALETTE_COLORS> cursorPalette{};
    std::vector<uint8_t> portraitTiles;  // FF6_MENU_PORTRAIT_COUNT portraits of 25 tiles of 4bpp, as in the ROM
    std::vector<uint16_t> portraitPalettes;  // 16 colors for each portrait
    std::vector<uint8_t> windowGfx;  // FF6_MENU_WINDOW_COUNT patterns of FF6_WINDOW_TILE_COUNT tiles of 4bpp
    // The colors 1 to 7 that each pattern starts with, BGR555.
    std::array<std::array<uint16_t, FF6_MENU_WINDOW_COLORS>, FF6_MENU_WINDOW_COUNT> windowColors{};
    // The name of each item: its symbol and 12 letters, in the codes of the small font.
    std::vector<uint8_t> itemNames;};

MenuData loadMenuData(const std::string& packDir);

}  // namespace ff6
