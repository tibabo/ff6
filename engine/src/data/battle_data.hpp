#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "constants.hpp"

namespace ff6 {

// One battle: the monsters in its six places, with their positions, and its flags.
struct BattleFormation {
    uint16_t flags = 0;  // battle type and the auxiliary flags (BattleProp): a set bit in the types FF6_BATTLE_TYPES_MASK forbids it
    uint16_t aux = 0;  // the second word of BattleProp
    std::array<int, FF6_BATTLE_SLOTS> monsters{};  // FF6_MONSTER_NONE for an empty place
    std::array<uint8_t, FF6_BATTLE_SLOTS> positions{};  // x in the high nibble, y in the low one
};

// battle.ff6data: the monsters and the battles, raw as the game keeps them.
struct BattleData {
    std::vector<uint8_t> monsterProp;  // FF6_MONSTER_COUNT monsters of FF6_MONSTER_PROP_BYTES
    std::vector<uint8_t> monsterItems;
    std::vector<uint8_t> monsterNames;  // in the codes of the small font
    std::vector<uint8_t> battleProp;
    std::vector<uint8_t> battleMonsters;
    std::vector<uint8_t> randGroups;  // FF6_BATTLE_GROUP_COUNT groups of FF6_RAND_GROUP_BATTLES words
    std::vector<uint8_t> eventGroups;  // and of FF6_EVENT_GROUP_BATTLES words
    std::vector<uint8_t> subGroups;  // for each map, a group of randGroups
    std::vector<uint8_t> subRates;  // for each map 2 bits: the rate of the random battles
    std::vector<uint8_t> rngTable;  // FF6_RNG_BYTES numbers that the game draws from in order
    std::vector<uint8_t> aiPtrs;  // for each monster, where its script starts in aiScript: a word
    std::vector<uint8_t> aiScript;  // FF6_AI_SCRIPT_BYTES of commands
    std::vector<uint8_t> genjuAttackNames, attackNames;  // of 10 codes each
    std::vector<uint8_t> sketch, control;  // of each monster, FF6_SKETCH_BYTES and FF6_CONTROL_BYTES attacks

    const uint8_t* monster(int id) const { return &monsterProp[static_cast<size_t>(id) * FF6_MONSTER_PROP_BYTES]; }
    const uint8_t* items(int id) const { return &monsterItems[static_cast<size_t>(id) * FF6_MONSTER_ITEM_BYTES]; }
    std::vector<uint8_t> name(int id) const;
    // The name of an attack of the table from 54 up (an esper's attack, a special attack); empty for a spell.
    std::vector<uint8_t> attackName(int attack) const;
    // The attacks of a monster that Sketch and Control give; FF6_AI_ATTACK_NOTHING for none.
    int sketchAttack(int monster, int slot) const { return sketch[static_cast<size_t>(monster) * FF6_SKETCH_BYTES + static_cast<size_t>(slot)]; }
    int controlAttack(int monster, int slot) const { return control[static_cast<size_t>(monster) * FF6_CONTROL_BYTES + static_cast<size_t>(slot)]; }
    // Where the script of a monster starts in aiScript.
    size_t aiStart(int monster) const;
    BattleFormation formation(int battle) const;
    // The battles of a group for the maps (a bit of a battle's word adds a random number to it) and for the events.
    int randBattle(int group, int index) const;
    int eventBattle(int group, int index) const;
    // The rate (0 normal, 1 low, 2 high, 3 very high) and the group of the random battles of a map.
    int subRate(int map) const;
    int subGroup(int map) const;
};

BattleData loadBattleData(const std::string& packDir);

}  // namespace ff6
