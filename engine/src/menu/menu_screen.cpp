#include "menu/menu_screen.hpp"

#include <algorithm>

#include "field/window.hpp"
#include "menu/window_style.hpp"

namespace ff6 {

uint8_t menuCode(char character) {
    if (character >= 'A' && character <= 'Z') return static_cast<uint8_t>(FF6_MENU_CODE_UPPER_A + (character - 'A'));
    if (character >= 'a' && character <= 'z') return static_cast<uint8_t>(FF6_MENU_CODE_LOWER_A + (character - 'a'));
    if (character >= '0' && character <= '9') return static_cast<uint8_t>(FF6_MENU_CODE_ZERO + (character - '0'));
    // The symbols follow the digits in the order of the dialogue font, whose glyphs are 0x60 below.
    static const struct { char character; uint8_t glyph; } kSymbols[] = {
        {'!', 0x5E}, {'?', 0x5F}, {'/', 0x60}, {':', 0x61}, {'"', 0x62}, {'\'', 0x63}, {'-', 0x64}, {'.', 0x65},
        {',', 0x66}, {';', 0x68}, {'#', 0x69}, {'+', 0x6A}, {'(', 0x6B}, {')', 0x6C}, {'%', 0x6D}, {'~', 0x6E},
        {'*', 0x6F}, {'@', 0x70}, {'=', 0x72}};
    for (const auto& symbol : kSymbols) {
        if (symbol.character == character) return static_cast<uint8_t>(symbol.glyph + FF6_MENU_CODE_DIALOGUE_SHIFT);
    }
    return FF6_MENU_CODE_SPACE;
}

MenuScreen::MenuScreen(const CommonData& common, const MenuData& menu) : common_(common), menu_(menu) {
    pixels_.assign(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL, 0);
}

void MenuScreen::clear() {
    for (size_t i = 0; i < pixels_.size(); i += FF6_BYTES_PER_PIXEL) {
        std::fill(pixels_.begin() + static_cast<std::ptrdiff_t>(i),
                  pixels_.begin() + static_cast<std::ptrdiff_t>(i + FF6_BYTES_PER_PIXEL), 0);
        pixels_[i + FF6_ALPHA_OFFSET] = 255;
    }
}

void MenuScreen::window(int x, int y, int w, int h) {
    drawWindow(pixels_, windowTiles_.empty() ? common_.windowTiles : windowTiles_, x * FF6_MENU_TILE_SIZE,
               y * FF6_MENU_TILE_SIZE, w + 2, h + 2);
}

void MenuScreen::applyConfig(const GameConfig& config) {
    windowTiles_ = buildWindowTiles(menu_, config);
    fontColor_ = config.fontColor;
    fontColorSet_ = true;
}

uint16_t MenuScreen::paletteColor(size_t index) const {
    static constexpr size_t kFontColorEntries[] = FF6_CONFIG_FONT_COLOR_ENTRIES;
    if (fontColorSet_) {
        for (const size_t entry : kFontColorEntries) {
            if (entry == index) return fontColor_;
        }
    }
    return menu_.fontPalette[index];
}

void MenuScreen::text(int x, int y, const std::string& text, Layer layer, int color) {
    std::vector<uint8_t> codes;
    for (const char character : text) codes.push_back(menuCode(character));
    this->codes(x, y, codes, layer, color);
}

void MenuScreen::codes(int x, int y, const std::vector<uint8_t>& codes, Layer layer, int color) {
    const int palette = (color >> FF6_MENU_TEXT_PALETTE_SHIFT) & FF6_MENU_TEXT_PALETTE_MASK;
    const size_t first = static_cast<size_t>(palette) *
                         (layer == Layer::Bg1 ? FF6_MENU_BG1_PALETTE_COLORS : FF6_MENU_BG3_PALETTE_COLORS);
    uint8_t colors[4][FF6_BYTES_PER_PIXEL];
    for (size_t i = 0; i < 4; ++i) colorToRgba(paletteColor(first + i), colors[i]);
    for (size_t at = 0; at < codes.size(); ++at) {
        const uint8_t* tile = &menu_.fontTiles[static_cast<size_t>(codes[at]) * FF6_MENU_FONT_TILE_BYTES];
        for (int row = 0; row < FF6_MENU_TILE_SIZE; ++row) {
            const int screenY = y * FF6_MENU_TILE_SIZE + row + textOffsetY_;
            if (layer == Layer::Bg1 && (screenY < clipTop_ || screenY >= clipBottom_)) continue;
            for (int column = 0; column < FF6_MENU_TILE_SIZE; ++column) {
                const int shift = FF6_MENU_TILE_SIZE - 1 - column;
                const int value = ((tile[row * 2] >> shift) & 1) | (((tile[row * 2 + 1] >> shift) & 1) << 1);
                if (value == 0) continue;
                blendPixel(pixels_, (x + static_cast<int>(at)) * FF6_MENU_TILE_SIZE + column, screenY, colors[value]);
            }
        }
    }
}

void MenuScreen::swatch(int x, int y, uint16_t color) {
    uint8_t rgba[FF6_BYTES_PER_PIXEL];
    colorToRgba(color, rgba);
    for (int row = 0; row < FF6_MENU_TILE_SIZE; ++row) {
        const int screenY = y * FF6_MENU_TILE_SIZE + row + textOffsetY_;
        if (screenY < clipTop_ || screenY >= clipBottom_) continue;
        for (int column = 0; column < FF6_MENU_TILE_SIZE; ++column) {
            blendPixel(pixels_, x * FF6_MENU_TILE_SIZE + column, screenY, rgba);
        }
    }
}

void MenuScreen::portrait(int x, int y, int portrait) {
    if (portrait < 0 || portrait >= FF6_MENU_PORTRAIT_COUNT) return;
    static constexpr int kOrder[FF6_MENU_PORTRAIT_TILES] = FF6_MENU_PORTRAIT_TILE_ORDER;
    uint8_t colors[FF6_MENU_PORTRAIT_COLORS][FF6_BYTES_PER_PIXEL];
    for (size_t i = 0; i < FF6_MENU_PORTRAIT_COLORS; ++i) {
        colorToRgba(menu_.portraitPalettes[static_cast<size_t>(portrait) * FF6_MENU_PORTRAIT_COLORS + i], colors[i]);
    }
    const size_t base = static_cast<size_t>(portrait) * FF6_MENU_PORTRAIT_TILES * FF6_MENU_CURSOR_TILE_BYTES;
    for (int cell = 0; cell < FF6_MENU_PORTRAIT_TILES; ++cell) {
        const uint8_t* tile = &menu_.portraitTiles[base + static_cast<size_t>(kOrder[cell]) * FF6_MENU_CURSOR_TILE_BYTES];
        for (int row = 0; row < FF6_MENU_TILE_SIZE; ++row) {
            for (int column = 0; column < FF6_MENU_TILE_SIZE; ++column) {
                const int shift = FF6_MENU_TILE_SIZE - 1 - column;
                const int value = ((tile[row * 2] >> shift) & 1) | (((tile[row * 2 + 1] >> shift) & 1) << 1) |
                                  (((tile[row * 2 + 16] >> shift) & 1) << 2) | (((tile[row * 2 + 17] >> shift) & 1) << 3);
                if (value == 0) continue;
                blendPixel(pixels_, x + (cell % FF6_MENU_PORTRAIT_COLUMNS) * FF6_MENU_TILE_SIZE + column,
                           y + (cell / FF6_MENU_PORTRAIT_COLUMNS) * FF6_MENU_TILE_SIZE + row, colors[value]);
            }
        }
    }
}

void MenuScreen::hand(int x, int y) {
    uint8_t colors[FF6_MENU_CURSOR_PALETTE_COLORS][FF6_BYTES_PER_PIXEL];
    for (size_t i = 0; i < FF6_MENU_CURSOR_PALETTE_COLORS; ++i) colorToRgba(menu_.cursorPalette[i], colors[i]);
    for (int part = 0; part < 4; ++part) {
        const uint8_t* tile = &menu_.cursorTiles[static_cast<size_t>(part) * FF6_MENU_CURSOR_TILE_BYTES];
        for (int row = 0; row < FF6_MENU_TILE_SIZE; ++row) {
            for (int column = 0; column < FF6_MENU_TILE_SIZE; ++column) {
                const int shift = FF6_MENU_TILE_SIZE - 1 - column;
                const int value = ((tile[row * 2] >> shift) & 1) | (((tile[row * 2 + 1] >> shift) & 1) << 1) |
                                  (((tile[row * 2 + 16] >> shift) & 1) << 2) | (((tile[row * 2 + 17] >> shift) & 1) << 3);
                if (value < FF6_MENU_CURSOR_FIRST_COLOR) continue;
                blendPixel(pixels_, x + (part % 2) * FF6_MENU_TILE_SIZE + column, y + (part / 2) * FF6_MENU_TILE_SIZE + row,
                           colors[value - FF6_MENU_CURSOR_FIRST_COLOR]);
            }
        }
    }
}

}  // namespace ff6
