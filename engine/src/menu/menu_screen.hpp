#pragma once

#include <array>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "data/pack.hpp"
#include "menu/menu_config.hpp"
#include "menu/menu_data.hpp"

namespace ff6 {

// The picture of a menu: windows, text on the grid of 8x8 tiles, and the hand cursor, drawn into one RGBA screen.
class MenuScreen {
public:
    // Which background of the console the text is on: they take their colors from the font palette differently.
    enum class Layer { Bg1, Bg3 };

    MenuScreen(const CommonData& common, const MenuData& menu);

    void clear();
    // Moves everything that is drawn to the right (the scroll of the backgrounds); the left side is black.
    void shiftRight(int pixels);
    // Where the windows and the text start, in pixels from the left of the screen (they may begin off it, like a window that slides in).
    void setOriginX(int pixels) { originX_ = pixels; }
    // Moves the text down by some pixels, like the scrolling of the backgrounds of a menu.
    void setTextOffset(int pixels) { textOffsetY_ = pixels; }
    // The text of the first background is only seen between these two lines of the screen.
    void setClip(int top, int bottom) {
        clipTop_ = top;
        clipBottom_ = bottom;
    }
    // Codes of the menus written in the large font of the dialogues (variable widths, with its shadow), from a pixel of the screen
    // (the top of the glyphs); `centered` puts the middle of the text there.
    void largeText(int x, int y, const std::vector<uint8_t>& codes, bool centered = false);
    // The pattern and colors of the windows, and the color of the font, that the config has chosen.
    void applyConfig(const GameConfig& config);
    // A window with its frame at the tile x, y, and an inside of w x h tiles (DrawWindow).
    void window(int x, int y, int w, int h);
    // A window of w x h pixels, frame included, at a pixel of the screen: the corners and the edges are the tiles of the window,
    // the right and bottom ones placed against the far side. With `gradientLine` >= 0, the colors are those of the battles: the
    // palette brightened by FF6_BATTLE_WINDOW_BRIGHTEN, then a level taken off every `gradientLines` lines from that line (the HDMA of the game).
    void windowPx(int x, int y, int w, int h, int gradientLine = -1, int gradientLines = 1);
    // The palettes of the text of the second layer (BGR555, four colors each) to use instead of the ones of the menus; null for those.
    void setBg3Palette(const uint16_t* colors) { bg3Palette_ = colors; }
    // Text from the tile x, y; `color` is the text color byte of the game, a tile attribute (FF6_MENU_BG1_*, FF6_MENU_BG3_*).
    void text(int x, int y, const std::string& text, Layer layer, int color);
    // The same with the codes of the font, for its symbols (arrows, gauges, swatches of color).
    void codes(int x, int y, const std::vector<uint8_t>& codes, Layer layer, int color);
    // A tile filled with one color (BGR555), like the boxes that show the colors of the windows.
    void swatch(int x, int y, uint16_t color);
    const MenuData& data() const { return menu_; }
    // The hand cursor, with the position that the game gives to its sprite.
    void hand(int x, int y, bool flip = false);  // `flip`: turned to the left
    // A portrait of the game, 40x40 pixels, with its upper left corner at a pixel of the screen.
    void portrait(int x, int y, int portrait);
    // The portraits of the characters as the game shows them (by the sprites that they wear), -1 where menuPortrait decides.
    void setCharacterPortraits(const std::array<int, FF6_CHAR_COUNT>& portraits) { characterPortraits_ = portraits; }
    int characterPortrait(int character) const;
    // The sprite of a character on the map, facing down, with its upper left corner at a pixel of the screen; `palette` is a
    // sprite palette of the data pack.
    void mapSprite(int x, int y, int gfx, int palette);
    // A sprite of 16x16 pixels of the sheet of the menus (the tiles `tile`, +1, +16 and +17), in the colors 12 to 15 of the palette
    // of the status icons (`icon`) or of the hand; the position is the one that the game gives the sprite, drawn a line lower.
    void menuSprite(int x, int y, int tile, bool icon);
    // A picture of RGBA pixels, w x h, with its upper left corner at a pixel of the screen; what is outside is cut.
    void image(int x, int y, int w, int h, const uint8_t* rgba);
    // The same, with the colors added to the ones that are there (or taken from them), by half if `half`.
    void imageMath(int x, int y, int w, int h, const uint8_t* rgba, bool subtract, bool half);
    // Blends a color over a rectangle of the screen, `percent` of it.
    void tint(int x, int y, int w, int h, int red, int green, int blue, int percent);
    // The mosaic: each block of `size` pixels (counted from the top left corner of the screen) takes the color of its first one.
    void mosaic(int x, int y, int w, int h, int size);
    // One pixel of a color, for the effects that are drawn and not painted.
    void plot(int x, int y, int red, int green, int blue);
    const std::vector<uint8_t>& pixels() const { return pixels_; }

private:
    uint16_t paletteColor(size_t index) const;
    const CommonData& common_;
    const MenuData& menu_;
    std::vector<uint8_t> pixels_;
    std::map<std::pair<int, int>, std::vector<uint8_t>> strips_;  // the map sprites that were asked for, by sprite and palette
    int textOffsetY_ = 0;
    int originX_ = 0;
    int clipTop_ = 0, clipBottom_ = FF6_SCREEN_HEIGHT;
    std::vector<uint8_t> windowTiles_;  // empty until the config is applied: then the tiles of the common data are used
    const uint16_t* bg3Palette_ = nullptr;
    uint16_t fontColor_ = FF6_CONFIG_FONT_COLOR_DEFAULT;
    bool fontColorSet_ = false;
    std::array<int, FF6_CHAR_COUNT> characterPortraits_ = filledPortraits();
    static std::array<int, FF6_CHAR_COUNT> filledPortraits() {
        std::array<int, FF6_CHAR_COUNT> none;
        none.fill(-1);
        return none;
    }
};

// The code of a character of the text of the menus, FF6_MENU_CODE_SPACE for one that the font does not have.
uint8_t menuCode(char character);
// The name of a character of the game, from 0 for Terra; empty for a number that is not one.
std::string characterName(int id);
// The name that the player gave to a character (an event asked for it), empty for the one of the game.
void setCharacterName(int id, const std::string& name);
std::string customCharacterName(int id);
// The portrait of a character of the game, from 0 for Terra.
int menuPortrait(int character);
// LoadPortraitGfx: the portrait of character `character` that wears the sprite `gfx` (-1 when unknown), an imp or not.
int wornPortrait(int character, int gfx, bool imp);
// A portrait as an RGBA image of FF6_MENU_PORTRAIT_SIZE square pixels, transparent where its color is 0.
std::vector<uint8_t> buildPortraitImage(const MenuData& menu, int portrait);

}  // namespace ff6
