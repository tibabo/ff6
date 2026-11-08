#pragma once

#include <array>

#include "constants.hpp"

namespace ff6 {

// A character of the party as the game keeps it (the character data at $1600): what the story and the battles change.
struct CharacterState {
    int actor = 0;  // the properties it started from; Terra's number for Terra
    int level = 1;
    int hp = 0, maxHp = 0, mp = 0, maxMp = 0;  // the maximums without the boost of the relics
    long experience = 0;
    int vigor = 0, speed = 0, stamina = 0, magicPower = 0;  // without the equipment
    std::array<int, FF6_MENU_EQUIP_SLOTS> equipment{};  // right hand to second relic; FF6_ITEM_EMPTY for nothing
    int esper = -1;  // the esper that it carries, which gives it the command Summon; -1 for none

    CharacterState() { equipment.fill(FF6_ITEM_EMPTY); }
};

struct InventorySlot {
    int item = FF6_ITEM_EMPTY;
    int quantity = 0;
};

struct PartyState {
    std::array<CharacterState, FF6_CHAR_COUNT> characters;
    std::array<InventorySlot, FF6_ITEM_COUNT> inventory;
    long gil = 0, steps = 0;
    std::array<bool, FF6_TREASURE_BIT_COUNT> treasures{};  // the chests that were opened
    std::array<std::array<bool, FF6_SPELL_COUNT>, FF6_CHAR_COUNT> spells{};  // the spells that each character knows
    std::array<bool, FF6_ESPER_COUNT> espers{};  // the espers that the party has
    unsigned swordtechs = 0, blitzes = 0;  // a bit for each one that is known
    unsigned lores = 0;  // a bit for each lore that Strago knows
};

}  // namespace ff6
