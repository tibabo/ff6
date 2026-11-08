#include <cstdlib>

#include "../check.hpp"
#include "../tests.hpp"
#include "data/battle_data.hpp"

// With the real pack: the first monster, the first battle and the groups.
void testBattleData() {
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::BattleData data = ff6::loadBattleData(pack);
    const uint8_t* guard = data.monster(0);
    CHECK(guard[0] == 30 && guard[1] == 16 && (guard[8] | (guard[9] << 8)) == 40);  // speed, attack and hp
    CHECK(data.name(0).size() == FF6_MONSTER_NAME_BYTES && data.name(0)[0] == 0x86);  // G
    CHECK(data.name(-1).empty() && data.name(FF6_MONSTER_COUNT).empty());
    CHECK(data.items(0)[3] == 0xFF);  // nothing common to drop... the fourth item of the guard is empty

    const ff6::BattleFormation first = data.formation(0);
    CHECK(first.monsters[0] == 0x19 && first.monsters[1] == FF6_MONSTER_NONE && first.monsters[5] == FF6_MONSTER_NONE);
    CHECK(first.positions[0] == 0x69 && first.flags == 0x00E3);
    int monsters = 0;
    for (int battle = 0; battle < FF6_BATTLE_COUNT; ++battle) {
        for (const int id : data.formation(battle).monsters) {
            CHECK(id == FF6_MONSTER_NONE || id < FF6_MONSTER_COUNT);
            monsters += id != FF6_MONSTER_NONE;
        }
    }
    CHECK(monsters > FF6_BATTLE_COUNT);  // most of the battles have more than one monster
    CHECK(data.formation(-1).monsters[0] == FF6_MONSTER_NONE);
    CHECK(data.randBattle(0, 0) == 6 && data.randBattle(0, 1) == 7 && data.randBattle(0, 4) == 0);  // the first group of the maps
    CHECK(data.eventBattle(1, 0) == 2 && data.eventBattle(1, 1) == 2);
    CHECK(data.sketchAttack(0, 0) == FF6_AI_ATTACK_BATTLE && data.controlAttack(0, 0) == FF6_AI_ATTACK_BATTLE);  // the guard has only the blow
}
