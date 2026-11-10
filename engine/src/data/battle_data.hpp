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
    std::vector<uint8_t> magicPoints;  // FF6_MAGIC_POINT_BYTES: what each battle gives to learn spells
    std::vector<uint8_t> monsterRage;  // of the first FF6_RAGE_MONSTERS monsters, the two attacks of their rage
    std::vector<uint8_t> danceProp;  // FF6_DANCE_COUNT dances of FF6_DANCE_ATTACKS attacks
    std::vector<uint8_t> battleBgDance;  // FF6_BG_DANCE_BYTES: the dance that Mog learns on each background of battle
    std::vector<uint8_t> danceBg;  // FF6_DANCE_BG_BYTES: the background that each dance shows
    std::vector<uint8_t> monsterDlgPtrs, monsterDlg;  // the dialogs of the monsters: pointers (words) and texts, ended by 0
    std::vector<uint8_t> battleDlgPtrs, battleDlg;  // the same for the scenes of the battles, where the characters talk
    std::vector<uint8_t> metamorph;  // FF6_METAMORPH_BYTES: the items that Metamorph draws from a monster
    std::vector<uint8_t> charAi;  // FF6_CHAR_AI_COUNT scenes of FF6_CHAR_AI_BYTES: the characters that act by a script
    std::vector<uint8_t> colosseum;  // FF6_COLOSSEUM_BYTES: the opponent and the prize of each item that is wagered

    const uint8_t* monster(int id) const { return &monsterProp[static_cast<size_t>(id) * FF6_MONSTER_PROP_BYTES]; }
    const uint8_t* items(int id) const { return &monsterItems[static_cast<size_t>(id) * FF6_MONSTER_ITEM_BYTES]; }
    std::vector<uint8_t> name(int id) const;
    // What a monster says (an F3 command of its script) as the bytes of the game's text, without the end; empty for none.
    std::vector<uint8_t> monsterDialog(int id) const;
    // A dialog of a scene of a battle (an F7 command), in the same way.
    std::vector<uint8_t> battleDialog(int id) const;
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
