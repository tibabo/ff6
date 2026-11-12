#include "data/game_rules.hpp"

#include <stdexcept>

#include "data/pack.hpp"

namespace ff6 {

// The names of the ROM are in capitals; the first letter keeps its case, as the other names of the engine.
std::string GameRules::actorName(int actor) const {
    std::string name;
    if (actor < 0 || actor >= FF6_CHAR_PROP_COUNT || charNames.size() < static_cast<size_t>(actor + 1) * FF6_CHAR_NAME_BYTES) return name;
    for (int i = 0; i < FF6_CHAR_NAME_BYTES; ++i) {
        const uint8_t code = charNames[static_cast<size_t>(actor) * FF6_CHAR_NAME_BYTES + static_cast<size_t>(i)];
        char letter = 0;
        if (code >= FF6_MENU_CODE_UPPER_A && code < FF6_MENU_CODE_LOWER_A) letter = static_cast<char>((i == 0 ? 'A' : 'a') + code - FF6_MENU_CODE_UPPER_A);
        else if (code >= FF6_MENU_CODE_LOWER_A && code < FF6_MENU_CODE_ZERO) letter = static_cast<char>('a' + code - FF6_MENU_CODE_LOWER_A);
        else if (code >= FF6_MENU_CODE_ZERO && code < FF6_MENU_CODE_ZERO + 10) letter = static_cast<char>('0' + code - FF6_MENU_CODE_ZERO);
        if (letter == 0) break;
        name.push_back(letter);
    }
    return name;
}

GameRules loadGameRules(const std::string& packDir) {
    const std::vector<uint8_t> file = readFile(packDir + "/" FF6_RULES_FILE);
    ByteReader reader(file);
    reader.expectMagic(FF6_RULES_MAGIC, "game data");
    GameRules rules;
    rules.charProp = reader.bytes(static_cast<size_t>(FF6_CHAR_PROP_COUNT) * FF6_CHAR_PROP_BYTES);
    rules.itemProp = reader.bytes(static_cast<size_t>(FF6_ITEM_COUNT) * FF6_ITEM_PROP_BYTES);
    rules.levelExp = reader.words(FF6_LEVEL_STEPS);
    rules.levelHp = reader.bytes(FF6_LEVEL_STEPS);
    rules.levelMp = reader.bytes(FF6_LEVEL_STEPS);
    rules.shopProp = reader.bytes(static_cast<size_t>(FF6_SHOP_COUNT) * FF6_SHOP_PROP_BYTES);
    rules.naturalMagic = reader.bytes(FF6_NATURAL_MAGIC_BYTES);
    rules.bushidoLevels = reader.bytes(FF6_TECH_COUNT);
    rules.blitzLevels = reader.bytes(FF6_TECH_COUNT);
    rules.magicProp = reader.bytes(static_cast<size_t>(FF6_MAGIC_PROP_COUNT) * FF6_MAGIC_PROP_BYTES);
    rules.genjuProp = reader.bytes(static_cast<size_t>(FF6_ESPER_COUNT) * FF6_GENJU_PROP_BYTES);
    rules.charNames = reader.bytes(static_cast<size_t>(FF6_CHAR_PROP_COUNT) * FF6_CHAR_NAME_BYTES);
    if (reader.remaining() != 0) throw std::runtime_error("game data has trailing bytes");
    return rules;
}

}  // namespace ff6
