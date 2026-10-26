#include "menu/menu_data.hpp"

#include <stdexcept>

#include "data/pack.hpp"

namespace ff6 {

MenuData loadMenuData(const std::string& packDir) {
    const std::vector<uint8_t> file = readFile(packDir + "/" FF6_MENU_FILE);
    ByteReader reader(file);
    reader.expectMagic(FF6_MENU_MAGIC, "menu data");
    MenuData menu;
    menu.fontTiles = reader.bytes(static_cast<size_t>(FF6_MENU_FONT_TILES) * FF6_MENU_FONT_TILE_BYTES);
    for (uint16_t& color : menu.fontPalette) color = reader.u16();
    menu.cursorTiles = reader.bytes(4 * FF6_MENU_CURSOR_TILE_BYTES);
    for (uint16_t& color : menu.cursorPalette) color = reader.u16();
    menu.portraitTiles = reader.bytes(static_cast<size_t>(FF6_MENU_PORTRAIT_COUNT) * FF6_MENU_PORTRAIT_TILES *
                                      FF6_MENU_CURSOR_TILE_BYTES);
    menu.portraitPalettes = reader.words(static_cast<size_t>(FF6_MENU_PORTRAIT_COUNT) * FF6_MENU_PORTRAIT_COLORS);
    menu.windowGfx = reader.bytes(static_cast<size_t>(FF6_MENU_WINDOW_COUNT) * FF6_MENU_WINDOW_GFX_BYTES);
    for (auto& colors : menu.windowColors) {
        for (uint16_t& color : colors) color = reader.u16();
    }
    menu.itemNames = reader.bytes(static_cast<size_t>(FF6_ITEM_COUNT) * FF6_ITEM_NAME_BYTES);
    if (reader.remaining() != 0) throw std::runtime_error("menu data has trailing bytes");
    return menu;
}

}  // namespace ff6
