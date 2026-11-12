#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "constants.hpp"

namespace ff6 {

// game.ff6data: the tables of the ROM that decide what a character or an item is, raw as the game keeps them.
struct GameRules {
    std::vector<uint8_t> charProp;  // FF6_CHAR_PROP_COUNT actors of FF6_CHAR_PROP_BYTES
    std::vector<uint8_t> itemProp;  // FF6_ITEM_COUNT items of FF6_ITEM_PROP_BYTES
    std::vector<uint16_t> levelExp;  // what a level adds to the experience, from level 2
    std::vector<uint8_t> levelHp, levelMp;  // what a level adds, from level 2
    std::vector<uint8_t> shopProp;  // FF6_SHOP_COUNT shops of FF6_SHOP_PROP_BYTES
    std::vector<uint8_t> naturalMagic;  // FF6_NATURAL_MAGIC_BYTES: Terra's pairs of spell and level, then Celes'
    std::vector<uint8_t> bushidoLevels, blitzLevels;  // the level of the character that learns each technique
    std::vector<uint8_t> magicProp;  // FF6_MAGIC_PROP_COUNT attacks of FF6_MAGIC_PROP_BYTES
    std::vector<uint8_t> genjuProp;  // FF6_ESPER_COUNT espers of FF6_GENJU_PROP_BYTES: the spells that they teach and the bonus

    std::vector<uint8_t> charNames;  // FF6_CHAR_PROP_COUNT names of FF6_CHAR_NAME_BYTES letters, the ones of the menus, 0xFF ends them

    // The name that a character takes with the properties of an actor (command 0x7F), as the letters of the game.
    std::string actorName(int actor) const;
    const uint8_t* actor(int actor) const { return &charProp[static_cast<size_t>(actor) * FF6_CHAR_PROP_BYTES]; }
    const uint8_t* item(int item) const { return &itemProp[static_cast<size_t>(item) * FF6_ITEM_PROP_BYTES]; }
    const uint8_t* shop(int shop) const { return &shopProp[static_cast<size_t>(shop) * FF6_SHOP_PROP_BYTES]; }
    const uint8_t* genju(int esper) const { return &genjuProp[static_cast<size_t>(esper) * FF6_GENJU_PROP_BYTES]; }
    const uint8_t* attack(int attack) const { return &magicProp[static_cast<size_t>(attack) * FF6_MAGIC_PROP_BYTES]; }
};

GameRules loadGameRules(const std::string& packDir);

}  // namespace ff6
