#pragma once

#include "data/game_rules.hpp"
#include "state/party_state.hpp"

namespace ff6 {

// The statistics that the menus show: the character with its equipment (port of UpdateEquip and CalcEquipEffect).
struct CharacterStats {
    int vigor = 0, speed = 0, stamina = 0, magicPower = 0;
    int attackPower = 0, defense = 0, magicDefense = 0, evade = 0, magicBlock = 0;
    int maxHp = 0, maxMp = 0;  // with the boost of the relics
    bool genjiGlove = false;  // two weapons at once
    bool equipAnything = false;
};

CharacterStats calcStats(const GameRules& rules, const CharacterState& character);

// What the character needs to get to the next level, 0 at the last level.
long experienceToNextLevel(const GameRules& rules, const CharacterState& character);

}  // namespace ff6
