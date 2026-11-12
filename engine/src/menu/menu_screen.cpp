#include "menu/menu_screen.hpp"

#include <algorithm>

#include "field/window.hpp"
#include "field/map.hpp"
#include "field/sprite_data.hpp"
#include "menu/window_style.hpp"

namespace ff6 {

namespace {

std::array<std::string, FF6_CHAR_COUNT>& customNames() {
    static std::array<std::string, FF6_CHAR_COUNT> names;
    return names;
}

}  // namespace

void setCharacterName(int id, const std::string& name) {
    if (id >= 0 && id < FF6_CHAR_COUNT) customNames()[static_cast<size_t>(id)] = name;
}

std::string customCharacterName(int id) { return id >= 0 && id < FF6_CHAR_COUNT ? customNames()[static_cast<size_t>(id)] : std::string(); }

std::string characterName(int id) {
    if (!customCharacterName(id).empty()) return customCharacterName(id);
    static const char* const kNames[] = FF6_CHARACTER_NAMES;
    std::string name = id >= 0 && id < FF6_CHAR_COUNT ? kNames[id] : "";
    if (!name.empty()) name[0] = static_cast<char>(name[0] - 'a' + 'A');
    return name;
}

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

int menuPortrait(int character) {
    static constexpr int kPortraits[FF6_CHAR_COUNT] = FF6_MENU_PORTRAITS_OF_CHARACTER;
    return character >= 0 && character < FF6_CHAR_COUNT ? kPortraits[character] : -1;
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

void MenuScreen::shiftRight(int pixels) {
    const size_t row = static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_BYTES_PER_PIXEL, shift = static_cast<size_t>(pixels) * FF6_BYTES_PER_PIXEL;
    for (size_t y = 0; y < static_cast<size_t>(FF6_SCREEN_HEIGHT); ++y) {
        const auto line = pixels_.begin() + static_cast<std::ptrdiff_t>(y * row);
        std::copy_backward(line, line + static_cast<std::ptrdiff_t>(row - shift), line + static_cast<std::ptrdiff_t>(row));
        for (size_t i = 0; i < shift; i += FF6_BYTES_PER_PIXEL) {
            std::fill(line + static_cast<std::ptrdiff_t>(i), line + static_cast<std::ptrdiff_t>(i + FF6_BYTES_PER_PIXEL), 0);
            line[static_cast<std::ptrdiff_t>(i + FF6_ALPHA_OFFSET)] = 255;
        }
    }
}

void MenuScreen::window(int x, int y, int w, int h) {
    drawWindow(pixels_, windowTiles_.empty() ? common_.windowTiles : windowTiles_, x * FF6_MENU_TILE_SIZE + originX_,
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
                blendPixel(pixels_, (x + static_cast<int>(at)) * FF6_MENU_TILE_SIZE + column + originX_, screenY, colors[value]);
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

void MenuScreen::mapSprite(int x, int y, int gfx, int palette) {
    auto found = strips_.find({gfx, palette});
    if (found == strips_.end()) found = strips_.emplace(std::make_pair(gfx, palette), buildSpriteStrip(common_, gfx, palette)).first;
    drawSpriteRows(pixels_.data(), found->second, stopFrame(Down), 0, FF6_FRAME_HEIGHT, x, y);
}

void MenuScreen::menuSprite(int x, int y, int tile, bool icon) {
    const auto& palette = icon ? menu_.iconPalette : menu_.cursorPalette;
    uint8_t colors[FF6_MENU_CURSOR_PALETTE_COLORS][FF6_BYTES_PER_PIXEL];
    for (size_t i = 0; i < FF6_MENU_CURSOR_PALETTE_COLORS; ++i) colorToRgba(palette[i], colors[i]);
    for (int part = 0; part < 4; ++part) {
        const size_t first = (static_cast<size_t>(tile) + static_cast<size_t>(part % 2) + static_cast<size_t>(part / 2) * FF6_MENU_SPRITE_ROW_TILES) * FF6_MENU_CURSOR_TILE_BYTES;
        if (first + FF6_MENU_CURSOR_TILE_BYTES > menu_.spriteGfx.size()) continue;
        const uint8_t* data = &menu_.spriteGfx[first];
        for (int row = 0; row < FF6_MENU_TILE_SIZE; ++row) {
            for (int column = 0; column < FF6_MENU_TILE_SIZE; ++column) {
                const int shift = FF6_MENU_TILE_SIZE - 1 - column;
                const int value = ((data[row * 2] >> shift) & 1) | (((data[row * 2 + 1] >> shift) & 1) << 1) |
                                  (((data[row * 2 + 16] >> shift) & 1) << 2) | (((data[row * 2 + 17] >> shift) & 1) << 3);
                if (value < FF6_MENU_CURSOR_FIRST_COLOR) continue;
                blendPixel(pixels_, x + (part % 2) * FF6_MENU_TILE_SIZE + column, y + 1 + (part / 2) * FF6_MENU_TILE_SIZE + row,
                           colors[value - FF6_MENU_CURSOR_FIRST_COLOR]);
            }
        }
    }
}

std::vector<uint8_t> buildPortraitImage(const MenuData& menu, int portrait) {
    std::vector<uint8_t> image(static_cast<size_t>(FF6_MENU_PORTRAIT_SIZE) * FF6_MENU_PORTRAIT_SIZE * FF6_BYTES_PER_PIXEL, 0);
    if (portrait < 0 || portrait >= FF6_MENU_PORTRAIT_COUNT) return image;
    static constexpr int kOrder[FF6_MENU_PORTRAIT_TILES] = FF6_MENU_PORTRAIT_TILE_ORDER;
    uint8_t colors[FF6_MENU_PORTRAIT_COLORS][FF6_BYTES_PER_PIXEL];
    for (size_t i = 0; i < FF6_MENU_PORTRAIT_COLORS; ++i) {
        colorToRgba(menu.portraitPalettes[static_cast<size_t>(portrait) * FF6_MENU_PORTRAIT_COLORS + i], colors[i]);
    }
    const size_t base = static_cast<size_t>(portrait) * FF6_MENU_PORTRAIT_TILES * FF6_MENU_CURSOR_TILE_BYTES;
    for (int cell = 0; cell < FF6_MENU_PORTRAIT_TILES; ++cell) {
        const uint8_t* tile = &menu.portraitTiles[base + static_cast<size_t>(kOrder[cell]) * FF6_MENU_CURSOR_TILE_BYTES];
        for (int row = 0; row < FF6_MENU_TILE_SIZE; ++row) {
            for (int column = 0; column < FF6_MENU_TILE_SIZE; ++column) {
                const int shift = FF6_MENU_TILE_SIZE - 1 - column;
                const int value = ((tile[row * 2] >> shift) & 1) | (((tile[row * 2 + 1] >> shift) & 1) << 1) |
                                  (((tile[row * 2 + 16] >> shift) & 1) << 2) | (((tile[row * 2 + 17] >> shift) & 1) << 3);
                if (value == 0) continue;
                const int x = (cell % FF6_MENU_PORTRAIT_COLUMNS) * FF6_MENU_TILE_SIZE + column;
                const int y = (cell / FF6_MENU_PORTRAIT_COLUMNS) * FF6_MENU_TILE_SIZE + row;
                std::copy_n(colors[value], FF6_BYTES_PER_PIXEL, &image[(static_cast<size_t>(y) * FF6_MENU_PORTRAIT_SIZE + x) * FF6_BYTES_PER_PIXEL]);
            }
        }
    }
    return image;
}

void MenuScreen::portrait(int x, int y, int portrait) {
    const std::vector<uint8_t> image = buildPortraitImage(menu_, portrait);
    for (int row = 0; row < FF6_MENU_PORTRAIT_SIZE; ++row) {
        for (int column = 0; column < FF6_MENU_PORTRAIT_SIZE; ++column) {
            const uint8_t* pixel = &image[(static_cast<size_t>(row) * FF6_MENU_PORTRAIT_SIZE + column) * FF6_BYTES_PER_PIXEL];
            if (pixel[FF6_ALPHA_OFFSET] != 0) blendPixel(pixels_, x + column, y + row, pixel);
        }
    }
}

void MenuScreen::image(int x, int y, int w, int h, const uint8_t* rgba) {
    for (int row = 0; row < h; ++row) {
        if (y + row < 0 || y + row >= FF6_SCREEN_HEIGHT) continue;
        for (int column = 0; column < w; ++column) {
            if (x + column < 0 || x + column >= FF6_SCREEN_WIDTH) continue;
            const uint8_t* pixel = rgba + (static_cast<size_t>(row) * w + column) * FF6_BYTES_PER_PIXEL;
            if (pixel[FF6_ALPHA_OFFSET] != 0) blendPixel(pixels_, x + column, y + row, pixel);
        }
    }
}

void MenuScreen::imageMath(int x, int y, int w, int h, const uint8_t* rgba, bool subtract, bool half) {
    for (int row = 0; row < h; ++row) {
        if (y + row < 0 || y + row >= FF6_SCREEN_HEIGHT) continue;
        for (int column = 0; column < w; ++column) {
            if (x + column < 0 || x + column >= FF6_SCREEN_WIDTH) continue;
            const uint8_t* pixel = rgba + (static_cast<size_t>(row) * w + column) * FF6_BYTES_PER_PIXEL;
            if (pixel[FF6_ALPHA_OFFSET] == 0) continue;
            uint8_t* under = &pixels_[(static_cast<size_t>(y + row) * FF6_SCREEN_WIDTH + x + column) * FF6_BYTES_PER_PIXEL];
            for (int channel = 0; channel < 3; ++channel) {
                int value = subtract ? under[channel] - pixel[channel] : under[channel] + pixel[channel];
                if (half) value /= 2;
                under[channel] = static_cast<uint8_t>(std::clamp(value, 0, 255));
            }
        }
    }
}

void MenuScreen::tint(int x, int y, int w, int h, int red, int green, int blue, int percent) {
    const int color[3] = {red, green, blue};
    percent = std::clamp(percent, 0, 100);
    for (int row = std::max(y, 0); row < std::min(y + h, FF6_SCREEN_HEIGHT); ++row) {
        for (int column = std::max(x, 0); column < std::min(x + w, FF6_SCREEN_WIDTH); ++column) {
            uint8_t* pixel = &pixels_[(static_cast<size_t>(row) * FF6_SCREEN_WIDTH + column) * FF6_BYTES_PER_PIXEL];
            for (int channel = 0; channel < 3; ++channel) pixel[channel] = static_cast<uint8_t>((pixel[channel] * (100 - percent) + color[channel] * percent) / 100);
        }
    }
}

void MenuScreen::mosaic(int x, int y, int w, int h, int size) {
    if (size <= 1) return;
    for (int row = std::max(y, 0); row < std::min(y + h, FF6_SCREEN_HEIGHT); ++row) {
        for (int column = std::max(x, 0); column < std::min(x + w, FF6_SCREEN_WIDTH); ++column) {
            const int fromRow = std::max(row - row % size, y), fromColumn = std::max(column - column % size, x);
            const uint8_t* from = &pixels_[(static_cast<size_t>(fromRow) * FF6_SCREEN_WIDTH + fromColumn) * FF6_BYTES_PER_PIXEL];
            std::copy(from, from + FF6_BYTES_PER_PIXEL, &pixels_[(static_cast<size_t>(row) * FF6_SCREEN_WIDTH + column) * FF6_BYTES_PER_PIXEL]);
        }
    }
}

void MenuScreen::plot(int x, int y, int red, int green, int blue) {
    if (x < 0 || y < 0 || x >= FF6_SCREEN_WIDTH || y >= FF6_SCREEN_HEIGHT) return;
    uint8_t* pixel = &pixels_[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL];
    pixel[0] = static_cast<uint8_t>(red);
    pixel[1] = static_cast<uint8_t>(green);
    pixel[2] = static_cast<uint8_t>(blue);
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
