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
    for (char& letter : name) letter = static_cast<char>(letter - 'a' + 'A');  // the names of the US version are in capitals
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

int wornPortrait(int character, int gfx, bool imp) {
    static constexpr int kOfGfx[FF6_MENU_PORTRAIT_GFX_COUNT] = FF6_MENU_PORTRAIT_OF_GFX;
    if (imp) return FF6_MENU_PORTRAIT_IMP;
    if (character == FF6_MENU_PORTRAIT_LOCKE) return FF6_MENU_PORTRAIT_LOCKE;
    return gfx >= 0 && gfx < FF6_MENU_PORTRAIT_GFX_COUNT ? kOfGfx[gfx] : menuPortrait(character);
}

int MenuScreen::characterPortrait(int character) const {
    if (character >= 0 && character < FF6_CHAR_COUNT && characterPortraits_[static_cast<size_t>(character)] >= 0) {
        return characterPortraits_[static_cast<size_t>(character)];
    }
    return menuPortrait(character);
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

void MenuScreen::windowPx(int x, int y, int w, int h, int gradientLine, int gradientLines) {
    const std::vector<uint8_t>& tiles = windowTiles_.empty() ? common_.windowTiles : windowTiles_;
    if (tiles.empty() || w < 2 * FF6_WINDOW_TILE_SIZE || h < 2 * FF6_WINDOW_TILE_SIZE) return;
    const int cols = (w + FF6_WINDOW_TILE_SIZE - 1) / FF6_WINDOW_TILE_SIZE, rows = (h + FF6_WINDOW_TILE_SIZE - 1) / FF6_WINDOW_TILE_SIZE;
    const auto put = [&](int col, int row) {
        const uint8_t* tile = &tiles[static_cast<size_t>(windowTile(col, row, cols, rows)) * FF6_WINDOW_TILE_BYTES];
        const int left = col == cols - 1 ? x + w - FF6_WINDOW_TILE_SIZE : x + col * FF6_WINDOW_TILE_SIZE;
        const int top = row == rows - 1 ? y + h - FF6_WINDOW_TILE_SIZE : y + row * FF6_WINDOW_TILE_SIZE;
        for (int ty = 0; ty < FF6_WINDOW_TILE_SIZE; ++ty) {
            const int screenY = top + ty;
            const int level = gradientLine < 0 || screenY < gradientLine ? 0
                : std::min((std::min(screenY, FF6_BATTLE_GRADIENT_LAST) - gradientLine) / std::max(gradientLines, 1), FF6_COLOR_LEVEL_MAX);
            for (int tx = 0; tx < FF6_WINDOW_TILE_SIZE; ++tx) {
                const uint8_t* source = tile + (ty * FF6_WINDOW_TILE_SIZE + tx) * FF6_BYTES_PER_PIXEL;
                if (gradientLine < 0) {
                    blendPixel(pixels_, left + tx, screenY, source);
                    continue;
                }
                uint8_t color[FF6_BYTES_PER_PIXEL];
                for (int c = 0; c < FF6_COLOR_CHANNELS; ++c) {
                    const int value = std::max(std::min((source[c] >> (8 - FF6_COLOR_BITS)) + FF6_BATTLE_WINDOW_BRIGHTEN, FF6_COLOR_LEVEL_MAX) - level, 0);
                    color[c] = static_cast<uint8_t>((value << (8 - FF6_COLOR_BITS)) | (value >> (2 * FF6_COLOR_BITS - 8)));
                }
                color[FF6_ALPHA_OFFSET] = source[FF6_ALPHA_OFFSET];
                blendPixel(pixels_, left + tx, screenY, color);
            }
        }
    };
    // The fill first, then the edges, then the corners: the last column and row overlap the ones before them.
    for (int row = 1; row < rows - 1; ++row) {
        for (int col = 1; col < cols - 1; ++col) put(col, row);
    }
    for (int col = 1; col < cols - 1; ++col) {
        put(col, 0);
        put(col, rows - 1);
    }
    for (int row = 1; row < rows - 1; ++row) {
        put(0, row);
        put(cols - 1, row);
    }
    put(0, 0);
    put(cols - 1, 0);
    put(0, rows - 1);
    put(cols - 1, rows - 1);
}

void MenuScreen::largeText(int x, int y, const std::vector<uint8_t>& codes, bool centered) {
    // The codes of the menus are those of the dialogues plus FF6_MENU_CODE_DIALOGUE_SHIFT; their space is the last code.
    std::vector<int> glyphs;
    for (const uint8_t code : codes) {
        const int glyph = code == FF6_MENU_CODE_SPACE ? FF6_DLG_GLYPH_SPACE : code - FF6_MENU_CODE_DIALOGUE_SHIFT;
        if (glyph >= FF6_FIRST_GLYPH_CODE && glyph < FF6_FIRST_GLYPH_CODE + FF6_GLYPH_COUNT) glyphs.push_back(glyph);
    }
    int width = 0;
    for (const int glyph : glyphs) width += common_.fontWidths[static_cast<size_t>(glyph)];
    const uint8_t shadow[FF6_BYTES_PER_PIXEL] = FF6_TEXT_SHADOW_RGBA;
    const uint8_t ink[FF6_BYTES_PER_PIXEL] = FF6_TEXT_INK_RGBA;
    for (const uint8_t* color : {shadow, ink}) {
        const int offset = color == shadow ? FF6_TEXT_SHADOW_OFFSET : 0;
        int pen = centered ? x - width / 2 : x;
        for (const int glyph : glyphs) {
            const size_t base = static_cast<size_t>(glyph - FF6_FIRST_GLYPH_CODE) * FF6_GLYPH_ROWS;
            for (int row = 0; row < FF6_GLYPH_ROWS && base + static_cast<size_t>(row) < common_.fontRows.size(); ++row) {
                const uint16_t bits = common_.fontRows[base + static_cast<size_t>(row)];
                for (int col = 0; col < FF6_GLYPH_ROW_BITS; ++col) {
                    if (bits & (FF6_GLYPH_LEFT_BIT >> col)) blendPixel(pixels_, pen + col + offset, y + row + offset, color);
                }
            }
            pen += common_.fontWidths[static_cast<size_t>(glyph)];
        }
    }
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
    for (size_t i = 0; i < 4; ++i) {
        uint16_t color = paletteColor(first + i);
        if (layer == Layer::Bg3 && bg3Palette_ && first + i < FF6_BATTLE_FONT_PAL_COLORS) {  // the battle: its own palettes, and the color of the font
            color = first + i == FF6_BATTLE_FONT_COLOR_ENTRY && fontColorSet_ ? fontColor_ : bg3Palette_[first + i];
        }
        colorToRgba(color, colors[i]);
    }
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

void MenuScreen::hand(int x, int y, bool flip) {
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
                const int px = (part % 2) * FF6_MENU_TILE_SIZE + column;
                blendPixel(pixels_, x + (flip ? 2 * FF6_MENU_TILE_SIZE - 1 - px : px), y + (part / 2) * FF6_MENU_TILE_SIZE + row,
                           colors[value - FF6_MENU_CURSOR_FIRST_COLOR]);
            }
        }
    }
}

}  // namespace ff6
