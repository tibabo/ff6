#pragma once

#include "data/game_rules.hpp"
#include "state/party_state.hpp"

namespace ff6 {

// The statistics that the menus show: the character with its equipment (port of UpdateEquip and CalcEquipEffect).
struct CharacterStats {
    int vigor = 0, speed = 0, stamina = 0, magicPower = 0;
    int attackPower = 0, defense = 0, magicDefense = 0, evade = 0, magicBlock = 0;
    // The right and the left hand attack on their own: battle power (0 for a hand that does not attack), hit rate of the
    // item in it (the fist when empty) and whether the gauntlet gives the weapon its two-hand effect.
    int handPower[2] = {0, 0}, handHitRate[2] = {0, 0};
    bool handTwoHand[2] = {false, false};
    int maxHp = 0, maxMp = 0;  // with the boost of the relics
    bool genjiGlove = false;  // two weapons at once
    bool equipAnything = false;
    // The effects of the relics, as the items keep them (RELIC_EFFECT1, 3 and 4), and the weapons that reach from the back row.
    int relic1 = 0, relic3 = 0, relic4 = 0;
    bool handLongRange[2] = {false, false};
};

CharacterStats calcStats(const GameRules& rules, const CharacterState& character);

// What the character needs to get to the next level, 0 at the last level.
long experienceToNextLevel(const GameRules& rules, const CharacterState& character);

}  // namespace ff6
