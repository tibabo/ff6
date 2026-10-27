#include "data/game_rules.hpp"

#include <stdexcept>

#include "data/pack.hpp"

namespace ff6 {

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
    if (reader.remaining() != 0) throw std::runtime_error("game data has trailing bytes");
    return rules;
}

}  // namespace ff6
