#include "menu/menu_data.hpp"

#include <stdexcept>

#include "data/pack.hpp"

namespace ff6 {

namespace {

std::vector<uint8_t> entry(const std::vector<uint8_t>& table, int index, size_t size) {
    const size_t start = static_cast<size_t>(index) * size;
    if (index < 0 || start + size > table.size()) return {};
    return std::vector<uint8_t>(table.begin() + static_cast<std::ptrdiff_t>(start), table.begin() + static_cast<std::ptrdiff_t>(start + size));
}

}  // namespace

std::vector<uint8_t> MenuData::spellName(int index) const { return entry(spellNames, index, FF6_SPELL_NAME_BYTES); }
std::vector<uint8_t> MenuData::esperName(int index) const { return entry(esperNames, index, FF6_ESPER_NAME_BYTES); }
std::vector<uint8_t> MenuData::bushidoName(int index) const { return entry(bushidoNames, index, FF6_TECH_NAME_BYTES); }
std::vector<uint8_t> MenuData::blitzName(int index) const { return entry(blitzNames, index, FF6_TECH_NAME_BYTES); }

std::vector<uint8_t> MenuData::itemDescription(int item) const {
    std::vector<uint8_t> text;
    if (item < 0 || static_cast<size_t>(item) >= itemDescPtrs.size()) return text;
    for (size_t at = itemDescPtrs[static_cast<size_t>(item)]; at < itemDescriptions.size() && itemDescriptions[at] != 0; ++at) {
        text.push_back(itemDescriptions[at]);
    }
    return text;
}

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
    menu.itemDescPtrs = reader.words(FF6_ITEM_COUNT);
    menu.itemDescriptions = reader.bytes(FF6_ITEM_DESC_BYTES);
    menu.spellNames = reader.bytes(static_cast<size_t>(FF6_SPELL_COUNT) * FF6_SPELL_NAME_BYTES);
    menu.esperNames = reader.bytes(static_cast<size_t>(FF6_ESPER_COUNT) * FF6_ESPER_NAME_BYTES);
    menu.bushidoNames = reader.bytes(static_cast<size_t>(FF6_TECH_COUNT) * FF6_TECH_NAME_BYTES);
    menu.blitzNames = reader.bytes(static_cast<size_t>(FF6_TECH_COUNT) * FF6_TECH_NAME_BYTES);
    if (reader.remaining() != 0) throw std::runtime_error("menu data has trailing bytes");
    return menu;
}

}  // namespace ff6
