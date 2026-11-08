#include "data/battle_data.hpp"

#include <stdexcept>

#include "data/pack.hpp"

namespace ff6 {

std::vector<uint8_t> BattleData::name(int id) const {
    if (id < 0 || id >= FF6_MONSTER_COUNT) return {};
    const auto first = monsterNames.begin() + static_cast<std::ptrdiff_t>(id) * FF6_MONSTER_NAME_BYTES;
    return std::vector<uint8_t>(first, first + FF6_MONSTER_NAME_BYTES);
}

std::vector<uint8_t> BattleData::attackName(int attack) const {
    const auto entry = [](const std::vector<uint8_t>& names, int index) {
        const auto first = names.begin() + static_cast<std::ptrdiff_t>(index) * FF6_ATTACK_NAME_BYTES;
        return std::vector<uint8_t>(first, first + FF6_ATTACK_NAME_BYTES);
    };
    if (attack >= FF6_FIRST_GENJU_ATTACK && attack < FF6_FIRST_NAMED_ATTACK) return entry(genjuAttackNames, attack - FF6_FIRST_GENJU_ATTACK);
    if (attack >= FF6_FIRST_NAMED_ATTACK && attack < FF6_FIRST_NAMED_ATTACK + FF6_ATTACK_NAME_COUNT) {
        return entry(attackNames, attack - FF6_FIRST_NAMED_ATTACK);
    }
    return {};
}

size_t BattleData::aiStart(int monster) const {
    if (monster < 0 || monster >= FF6_MONSTER_COUNT) return FF6_AI_SCRIPT_BYTES;
    return static_cast<size_t>(aiPtrs[static_cast<size_t>(monster) * 2] | (aiPtrs[static_cast<size_t>(monster) * 2 + 1] << 8));
}

BattleFormation BattleData::formation(int battle) const {
    BattleFormation result;
    result.monsters.fill(FF6_MONSTER_NONE);
    if (battle < 0 || battle >= FF6_BATTLE_COUNT) return result;
    const uint8_t* record = &battleMonsters[static_cast<size_t>(battle) * FF6_BATTLE_MONSTER_BYTES];
    const uint8_t* prop = &battleProp[static_cast<size_t>(battle) * FF6_BATTLE_PROP_BYTES];
    result.flags = static_cast<uint16_t>(prop[0] | (prop[1] << 8));
    result.aux = static_cast<uint16_t>(prop[2] | (prop[3] << 8));
    const int ninthBits = record[2 + 2 * FF6_BATTLE_SLOTS];
    for (size_t i = 0; i < result.monsters.size(); ++i) {
        result.monsters[i] = record[2 + i] | (((ninthBits >> i) & 1) << 8);
        result.positions[i] = record[2 + FF6_BATTLE_SLOTS + i];
    }
    return result;
}

int BattleData::randBattle(int group, int index) const {
    if (group < 0 || group >= FF6_BATTLE_GROUP_COUNT || index < 0 || index >= FF6_RAND_GROUP_BATTLES) return 0;
    const size_t at = (static_cast<size_t>(group) * FF6_RAND_GROUP_BATTLES + static_cast<size_t>(index)) * 2;
    return randGroups[at] | (randGroups[at + 1] << 8);
}

int BattleData::eventBattle(int group, int index) const {
    if (group < 0 || group >= FF6_BATTLE_GROUP_COUNT || index < 0 || index >= FF6_EVENT_GROUP_BATTLES) return 0;
    const size_t at = (static_cast<size_t>(group) * FF6_EVENT_GROUP_BATTLES + static_cast<size_t>(index)) * 2;
    return eventGroups[at] | (eventGroups[at + 1] << 8);
}

int BattleData::subRate(int map) const {
    if (map < 0 || map >= FF6_BATTLE_MAP_COUNT) return 0;
    return (subRates[static_cast<size_t>(map) / 4] >> (map % 4 * 2)) & 3;
}

int BattleData::subGroup(int map) const {
    return map >= 0 && map < FF6_BATTLE_MAP_COUNT ? subGroups[static_cast<size_t>(map)] : 0;
}

BattleData loadBattleData(const std::string& packDir) {
    const std::vector<uint8_t> file = readFile(packDir + "/" FF6_BATTLE_FILE);
    ByteReader reader(file);
    reader.expectMagic(FF6_BATTLE_MAGIC, "battle data");
    BattleData data;
    data.monsterProp = reader.bytes(static_cast<size_t>(FF6_MONSTER_COUNT) * FF6_MONSTER_PROP_BYTES);
    data.monsterItems = reader.bytes(static_cast<size_t>(FF6_MONSTER_COUNT) * FF6_MONSTER_ITEM_BYTES);
    data.monsterNames = reader.bytes(static_cast<size_t>(FF6_MONSTER_COUNT) * FF6_MONSTER_NAME_BYTES);
    data.battleProp = reader.bytes(static_cast<size_t>(FF6_BATTLE_COUNT) * FF6_BATTLE_PROP_BYTES);
    data.battleMonsters = reader.bytes(static_cast<size_t>(FF6_BATTLE_COUNT) * FF6_BATTLE_MONSTER_BYTES);
    data.randGroups = reader.bytes(static_cast<size_t>(FF6_BATTLE_GROUP_COUNT) * FF6_RAND_GROUP_BATTLES * 2);
    data.eventGroups = reader.bytes(static_cast<size_t>(FF6_BATTLE_GROUP_COUNT) * FF6_EVENT_GROUP_BATTLES * 2);
    data.subGroups = reader.bytes(FF6_BATTLE_MAP_COUNT);
    data.subRates = reader.bytes(FF6_SUB_RATE_BYTES);
    data.rngTable = reader.bytes(FF6_RNG_BYTES);
    data.aiPtrs = reader.bytes(static_cast<size_t>(FF6_MONSTER_COUNT) * 2);
    data.aiScript = reader.bytes(FF6_AI_SCRIPT_BYTES);
    data.genjuAttackNames = reader.bytes(static_cast<size_t>(FF6_GENJU_ATTACK_COUNT) * FF6_ATTACK_NAME_BYTES);
    data.attackNames = reader.bytes(static_cast<size_t>(FF6_ATTACK_NAME_COUNT) * FF6_ATTACK_NAME_BYTES);
    data.sketch = reader.bytes(static_cast<size_t>(FF6_MONSTER_COUNT) * FF6_SKETCH_BYTES);
    data.control = reader.bytes(static_cast<size_t>(FF6_MONSTER_COUNT) * FF6_CONTROL_BYTES);
    if (reader.remaining() != 0) throw std::runtime_error("battle data has trailing bytes");
    return data;
}

}  // namespace ff6
