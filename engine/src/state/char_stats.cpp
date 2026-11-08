#include "state/char_stats.hpp"

#include <algorithm>

namespace ff6 {
namespace {

int signedNibble(int nibble) { return nibble & 8 ? -(nibble & 7) : nibble; }

// The high byte of a statistic that went over 255 or under 0 clamps it.
int clampStat(int value) { return std::max(0, std::min(value, FF6_STAT_MAX)); }

int boosted(int base, bool half, bool quarter, bool eighth, int limit) {
    int result = base;
    if (half) {
        result += base / 2;
    } else if (quarter) {
        result += base / 4;
    } else if (eighth) {
        result += base / 8;
    }
    return std::min(result, limit);
}

}  // namespace

CharacterStats calcStats(const GameRules& rules, const CharacterState& character) {
    static constexpr int kEvade[] = FF6_EQUIP_EVADE_TABLE;
    const uint8_t* actor = rules.actor(character.actor);
    int vigor = character.vigor, speed = character.speed, stamina = character.stamina, magic = character.magicPower;
    int rightPower = actor[FF6_CHAR_PROP_ATTACK], leftPower = actor[FF6_CHAR_PROP_ATTACK];
    int defense = actor[FF6_CHAR_PROP_DEFENSE], magicDefense = actor[FF6_CHAR_PROP_MAGIC_DEFENSE];
    int evade = actor[FF6_CHAR_PROP_EVADE], magicBlock = actor[FF6_CHAR_PROP_MAGIC_BLOCK];
    int relic1 = 0, relic3 = 0, relic4 = 0;
    int hands = 0;  // 1 weapon in the left hand, 2 in the right, 4 and 8 nothing in them
    for (int slot = FF6_MENU_EQUIP_SLOTS - 1; slot >= 0; --slot) {
        const int id = character.equipment[static_cast<size_t>(slot)] & 0xFF;
        const uint8_t* item = rules.item(id);
        relic1 |= item[FF6_ITEM_PROP_RELIC1];
        relic3 |= item[FF6_ITEM_PROP_RELIC3];
        relic4 |= item[FF6_ITEM_PROP_RELIC4];
        vigor += signedNibble(item[FF6_ITEM_PROP_STATS] & 0x0F);
        speed += signedNibble(item[FF6_ITEM_PROP_STATS] >> 4);
        stamina += signedNibble(item[FF6_ITEM_PROP_STATS + 1] & 0x0F);
        magic += signedNibble(item[FF6_ITEM_PROP_STATS + 1] >> 4);
        const int evadeStep = item[FF6_ITEM_PROP_EVADE] & 0x0F, blockStep = item[FF6_ITEM_PROP_EVADE] >> 4;
        if (evadeStep < static_cast<int>(sizeof(kEvade) / sizeof(kEvade[0]))) evade += kEvade[evadeStep];
        if (blockStep < static_cast<int>(sizeof(kEvade) / sizeof(kEvade[0]))) magicBlock += kEvade[blockStep];
        // A character is not an imp here, so only an item made for imps loses its power.
        const int power = item[FF6_ITEM_PROP_EQUIP_FLAGS] & FF6_ITEM_IMP_BIT ? 1 : item[FF6_ITEM_PROP_POWER];
        if ((item[FF6_ITEM_PROP_TYPE] & FF6_ITEM_PROP_TYPE_MASK) != FF6_ITEM_TYPE_WEAPON) {
            defense = std::min(defense + power, FF6_STAT_MAX);
            magicDefense = std::min(magicDefense + item[FF6_ITEM_PROP_HIT_OR_MAGIC_DEFENSE], FF6_STAT_MAX);
        } else if (slot < 2) {
            const bool right = slot == 0;
            hands |= (right ? 2 : 1) << (id == FF6_ITEM_EMPTY ? 2 : 0);
            int& hand = right ? rightPower : leftPower;
            hand = std::min(hand + power, FF6_STAT_MAX);
        }
    }
    vigor = clampStat(vigor);
    speed = clampStat(speed);
    stamina = clampStat(stamina);
    magic = clampStat(magic);
    evade = clampStat(evade);
    magicBlock = clampStat(magicBlock);
    if (relic3 & FF6_RELIC3_STRENGTH_50) vigor += vigor / 2;  // the menu shows the low byte of it

    // The hands that attack, then what the gauntlet and the genji glove change (CheckHandEffects).
    switch (hands) {
        case 1: case 4: case 9: rightPower = 0; break;
        case 2: case 6: case 8: case 12: leftPower = 0; break;
        default: break;
    }
    const int rightItem = character.equipment[0] & 0xFF, leftItem = character.equipment[1] & 0xFF;
    const auto twoHanded = [&](int id) { return (rules.item(id)[FF6_ITEM_PROP_WEAPON_FLAGS] & FF6_ITEM_TWO_HANDED) != 0; };
    bool gauntlet = false;
    if (relic4 & FF6_RELIC4_GAUNTLET) {
        if (rightItem != FF6_ITEM_EMPTY && leftItem == FF6_ITEM_EMPTY) {
            gauntlet = twoHanded(rightItem);
        } else if (rightItem == FF6_ITEM_EMPTY && leftItem != FF6_ITEM_EMPTY) {
            gauntlet = twoHanded(leftItem);
        }
    }
    if (gauntlet) {
        if (rightPower != 0) {
            leftPower = rightPower;
        } else {
            rightPower = leftPower;
        }
    } else if (!(relic4 & FF6_RELIC4_GENJI_GLOVE) && rightPower != 0) {
        leftPower = 0;
    }

    CharacterStats stats;
    stats.vigor = vigor & 0xFF;
    stats.speed = speed;
    stats.stamina = stamina;
    stats.magicPower = magic;
    stats.attackPower = rightPower + leftPower;
    stats.handPower[0] = rightPower;
    stats.handPower[1] = leftPower;
    stats.handHitRate[0] = rules.item(rightItem)[FF6_ITEM_PROP_HIT_OR_MAGIC_DEFENSE];
    stats.handHitRate[1] = rules.item(leftItem)[FF6_ITEM_PROP_HIT_OR_MAGIC_DEFENSE];
    // The two-hand flag of a weapon stays only with the gauntlet; the copied weapon of the other hand has it too.
    const bool gauntletWorn = (relic4 & FF6_RELIC4_GAUNTLET) != 0;
    stats.handTwoHand[0] = gauntletWorn && (gauntlet || twoHanded(rightItem));
    stats.handTwoHand[1] = gauntletWorn && (gauntlet || twoHanded(leftItem));
    stats.defense = defense;
    stats.magicDefense = magicDefense;
    stats.evade = evade;
    stats.magicBlock = magicBlock;
    stats.genjiGlove = (relic4 & FF6_RELIC4_GENJI_GLOVE) != 0;
    stats.relic1 = relic1;
    stats.relic3 = relic3;
    stats.relic4 = relic4;
    stats.handLongRange[0] = (rules.item(rightItem)[FF6_ITEM_PROP_WEAPON_FLAGS] & FF6_WEAPON_BACK_ROW) != 0;
    stats.handLongRange[1] = (rules.item(leftItem)[FF6_ITEM_PROP_WEAPON_FLAGS] & FF6_WEAPON_BACK_ROW) != 0;
    stats.equipAnything = (relic4 & FF6_RELIC4_EQUIP_ANY) != 0;
    stats.maxHp = boosted(character.maxHp, relic1 & FF6_RELIC1_HP_50, relic1 & FF6_RELIC1_HP_25, relic1 & FF6_RELIC1_HP_12, FF6_MAX_HP);
    stats.maxMp = boosted(character.maxMp, relic1 & FF6_RELIC1_MP_50, relic1 & FF6_RELIC1_MP_25, relic1 & FF6_RELIC1_MP_12, FF6_MAX_MP);
    return stats;
}

long experienceToNextLevel(const GameRules& rules, const CharacterState& character) {
    if (character.level >= FF6_MAX_LEVEL) return 0;
    long needed = 0;
    for (int step = 0; step < character.level; ++step) needed += rules.levelExp[static_cast<size_t>(step)];
    return std::max(0L, (needed << FF6_EXP_SHIFT) - character.experience);
}

}  // namespace ff6
