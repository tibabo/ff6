#include <cstdlib>

#include "../check.hpp"
#include "../tests.hpp"
#include "data/game_rules.hpp"
#include "state/char_stats.hpp"
#include "state/party_ops.hpp"

namespace {

void testInventory() {
    ff6::PartyState party;
    ff6::giveItem(party, 233);
    ff6::giveItem(party, 233);
    ff6::giveItem(party, 0);
    CHECK(party.inventory[0].item == 233 && party.inventory[0].quantity == 2);
    CHECK(party.inventory[1].item == 0 && party.inventory[1].quantity == 1);
    CHECK(party.inventory[2].item == FF6_ITEM_EMPTY);
    for (int i = 0; i < 200; ++i) ff6::giveItem(party, 233);
    CHECK(party.inventory[0].quantity == FF6_MAX_ITEM_QUANTITY);
    ff6::takeItem(party, 0);  // the last one empties the place
    CHECK(party.inventory[1].item == FF6_ITEM_EMPTY && party.inventory[1].quantity == 0);
    ff6::giveItem(party, 7);  // and the next item takes the first empty place
    CHECK(party.inventory[1].item == 7);
    ff6::takeItem(party, 99);  // an item that is not there changes nothing
    CHECK(party.inventory[0].quantity == FF6_MAX_ITEM_QUANTITY);

    ff6::giveGil(party, 65535);
    CHECK(party.gil == 65535);
    for (int i = 0; i < 300; ++i) ff6::giveGil(party, 65535);
    CHECK(party.gil == FF6_MAX_GIL);
    party.gil = 100;
    CHECK(!ff6::takeGil(party, 101) && party.gil == 100);
    CHECK(ff6::takeGil(party, 100) && party.gil == 0);
    party.steps = FF6_MAX_STEPS - 1;
    ff6::countStep(party);
    ff6::countStep(party);
    CHECK(party.steps == FF6_MAX_STEPS);
}

void testCharacters(const ff6::GameRules& rules) {
    ff6::PartyState party;
    for (int i = 0; i < FF6_CHAR_COUNT; ++i) ff6::setCharacterProperties(party, rules, i, i, 0);
    // Terra starts at the default level, with the points of her first levels.
    const ff6::CharacterState& terra = party.characters[0];
    CHECK(terra.level == FF6_DEFAULT_LEVEL && terra.maxHp == 40 + 11 + 12 && terra.maxMp == 16 + 4 + 4);
    CHECK(terra.hp == terra.maxHp && terra.mp == terra.maxMp && terra.experience == 8 * (4 + 8));
    CHECK(terra.vigor == 31 && terra.speed == 33 && terra.stamina == 28 && terra.magicPower == 39);
    CHECK(terra.equipment[0] == 1 && terra.equipment[1] == 0x5A && terra.equipment[4] == FF6_ITEM_EMPTY);
    CHECK(ff6::experienceToNextLevel(rules, terra) == 8 * (4 + 8 + 14) - 8 * (4 + 8));
    CHECK(party.characters[1].level == FF6_DEFAULT_LEVEL + 2);  // Locke joins above the average

    // The average is the one of the characters that have joined, and a level 1 character can be brought up to it.
    party.characters[0].level = 10;
    party.characters[1].level = 20;
    CHECK(ff6::averageLevel(party, 0b11) == 15 && ff6::averageLevel(party, 0) == FF6_DEFAULT_LEVEL);
    ff6::setCharacterProperties(party, rules, 4, 4, 0b11);  // Edgar
    CHECK(party.characters[4].level == 15 + 2);
    ff6::setCharacterProperties(party, rules, 3, 3, 0b11);  // Shadow is not above or below
    CHECK(party.characters[3].level == 15);
    party.characters[3].level = 5;
    party.characters[3].hp = 1;
    const int before = party.characters[3].maxHp;
    ff6::normalizeLevel(party, rules, 3, 0b11);
    CHECK(party.characters[3].level == 15 && party.characters[3].maxHp > before);
    CHECK(party.characters[3].hp == party.characters[3].maxHp);
    long total = 0;
    for (int step = 0; step < 14; ++step) total += rules.levelExp[static_cast<size_t>(step)];
    CHECK(party.characters[3].experience == 8 * total);
    ff6::normalizeLevel(party, rules, 3, 0b1);  // a character above the average keeps its level
    CHECK(party.characters[3].level == 15);
}

void testStats(const ff6::GameRules& rules) {
    ff6::CharacterState terra;
    ff6::PartyState party;
    ff6::setCharacterProperties(party, rules, 0, 0, 0);
    terra = party.characters[0];
    terra.equipment.fill(FF6_ITEM_EMPTY);
    ff6::CharacterStats stats = ff6::calcStats(rules, terra);
    CHECK(stats.vigor == 31 && stats.speed == 33 && stats.stamina == 28 && stats.magicPower == 39);
    CHECK(stats.defense == 42 && stats.magicDefense == 33 && stats.evade == 5 && stats.magicBlock == 7);
    CHECK(stats.attackPower == 12 + 10);  // the fists, in one hand
    CHECK(stats.maxHp == terra.maxHp && stats.maxMp == terra.maxMp);

    terra.equipment[0] = 0;  // a dirk, battle power 26
    stats = ff6::calcStats(rules, terra);
    CHECK(stats.attackPower == 12 + 26);
    CHECK(stats.defense == 42);

    // A relic that adds hp raises the maximum, and no more than the limit: the muscle belt adds half.
    terra.equipment[4] = 203;
    stats = ff6::calcStats(rules, terra);
    CHECK(stats.maxHp == terra.maxHp + terra.maxHp / 2);
    terra.maxHp = FF6_MAX_HP;
    CHECK(ff6::calcStats(rules, terra).maxHp == FF6_MAX_HP);
    terra.equipment[4] = 210;  // the hyper wrist adds half of the vigor
    CHECK(ff6::calcStats(rules, terra).vigor == 31 + 31 / 2);
    terra.equipment[4] = FF6_ITEM_EMPTY;
    // The equipment of Terra at the start, computed by hand from the properties of her items.
    ff6::CharacterState armed = party.characters[0];
    const ff6::CharacterStats start = ff6::calcStats(rules, armed);
    CHECK(start.attackPower == 12 + 30 && start.defense == 42 + 16 + 11 + 28 && start.magicDefense == 33 + 10 + 7 + 19);
    CHECK(start.evade == 5 + 10 && start.magicBlock == 7);
    // Every statistic stays between 0 and 255 whatever the equipment.
    for (int item = 0; item < FF6_ITEM_COUNT; ++item) {
        armed.equipment.fill(item);
        const ff6::CharacterStats all = ff6::calcStats(rules, armed);
        CHECK(all.speed >= 0 && all.speed <= FF6_STAT_MAX && all.defense >= 0 && all.defense <= FF6_STAT_MAX);
        CHECK(all.evade >= 0 && all.evade <= FF6_STAT_MAX && all.attackPower >= 0 && all.attackPower <= 2 * FF6_STAT_MAX);
    }
}

void testHpChanges(const ff6::GameRules& rules) {
    ff6::PartyState party;
    ff6::setCharacterProperties(party, rules, 0, 0, 0);
    ff6::CharacterState& terra = party.characters[0];
    terra.equipment.fill(FF6_ITEM_EMPTY);
    const int maximum = terra.maxHp;
    ff6::changeHp(party, rules, 0, FF6_HP_CHANGE_SUBTRACT | 3);  // 8 points
    CHECK(terra.hp == maximum - 8);
    ff6::changeHp(party, rules, 0, 2);  // 4 points
    CHECK(terra.hp == maximum - 4);
    ff6::changeHp(party, rules, 0, 7);  // 128 points, not above the maximum
    CHECK(terra.hp == maximum);
    terra.hp = 5;
    ff6::changeHp(party, rules, 0, FF6_HP_CHANGE_SUBTRACT | 7);  // never below 1
    CHECK(terra.hp == 1);
    terra.hp = 0;
    ff6::changeHp(party, rules, 0, FF6_HP_CHANGE_SUBTRACT | 3);  // a character that is down stays down
    CHECK(terra.hp == 0);
    ff6::changeHp(party, rules, 0, FF6_HP_CHANGE_SET_MAX);
    CHECK(terra.hp == maximum);
    terra.mp = 0;
    ff6::changeMp(party, rules, 0, FF6_HP_CHANGE_SET_MAX);
    CHECK(terra.mp == terra.maxMp);
}

void testEquipmentRemoval(const ff6::GameRules& rules) {
    ff6::PartyState party;
    ff6::setCharacterProperties(party, rules, 0, 0, 0);
    ff6::giveItem(party, 1);  // the knife that Terra has on goes back on top of this one
    ff6::removeEquipment(party, 0);
    for (const int item : party.characters[0].equipment) CHECK(item == FF6_ITEM_EMPTY);
    CHECK(party.inventory[0].item == 1 && party.inventory[0].quantity == 2);
    CHECK(party.inventory[1].item == 0x5A && party.inventory[2].item == 0x69 && party.inventory[3].item == 0x84);
}

void testEquipping(const ff6::GameRules& rules) {
    ff6::PartyState party;
    ff6::setCharacterProperties(party, rules, 0, 0, 0);  // Terra, with the knife and the buckler
    for (const int item : {0, 203, 233, 0x69}) ff6::giveItem(party, item);  // dirk, muscle belt, potion, leather hat
    const auto fits = [&](int slot) { return ff6::equippableItems(party, rules, 0, slot); };
    CHECK(fits(FF6_EQUIP_RIGHT_HAND) == std::vector<int>({0}));  // the left hand has a shield, so a weapon is fine
    CHECK(fits(FF6_EQUIP_LEFT_HAND).empty());  // a weapon in each hand needs the genji glove
    CHECK(fits(FF6_EQUIP_HEAD) == std::vector<int>({0x69}));
    CHECK(fits(FF6_EQUIP_BODY).empty());
    CHECK(fits(FF6_EQUIP_FIRST_RELIC) == std::vector<int>({203}));  // the potion fits nowhere
    party.characters[0].equipment[FF6_EQUIP_FIRST_RELIC + 1] = 209;  // the genji glove
    CHECK(fits(FF6_EQUIP_LEFT_HAND) == std::vector<int>({0}));  // with the glove, the dirk joins the knife
    party.characters[0].equipment[FF6_EQUIP_FIRST_RELIC + 1] = FF6_ITEM_EMPTY;
    CHECK(fits(FF6_EQUIP_LEFT_HAND).empty());  // and without it, it does not
    // The old item goes back to the inventory when a new one is put on.
    CHECK(ff6::equipItem(party, rules, 0, FF6_EQUIP_RIGHT_HAND, 0));
    CHECK(party.characters[0].equipment[FF6_EQUIP_RIGHT_HAND] == 0);
    CHECK(party.inventory[0].item == 1 && party.inventory[0].quantity == 1);  // the knife took the place of the dirk
    CHECK(!ff6::equipItem(party, rules, 0, FF6_EQUIP_RIGHT_HAND, 0));  // the dirk is not in the inventory anymore
    CHECK(ff6::equipItem(party, rules, 0, FF6_EQUIP_RIGHT_HAND, FF6_ITEM_EMPTY));
    CHECK(party.characters[0].equipment[FF6_EQUIP_RIGHT_HAND] == FF6_ITEM_EMPTY);

    // A relic that raised the maximum of hp takes the hp over it away when it is taken off.
    CHECK(ff6::equipItem(party, rules, 0, FF6_EQUIP_FIRST_RELIC, 203));
    party.characters[0].hp = ff6::calcStats(rules, party.characters[0]).maxHp;
    CHECK(party.characters[0].hp > party.characters[0].maxHp);
    CHECK(ff6::equipItem(party, rules, 0, FF6_EQUIP_FIRST_RELIC, FF6_ITEM_EMPTY));
    CHECK(party.characters[0].hp == party.characters[0].maxHp);

    // Optimum: the best of what the character can wear goes on, and what it wore goes back.
    ff6::PartyState optimum;
    ff6::setCharacterProperties(optimum, rules, 0, 0, 0);
    for (const int item : {0, 1, 0x69, 0x5A}) ff6::giveItem(optimum, item);  // dirk, knife, leather hat, and the buckler
    ff6::optimizeEquipment(optimum, rules, 0);
    const auto power = [&](int item) { return rules.item(item)[FF6_ITEM_PROP_ITEM_POWER]; };
    const int weapon = optimum.characters[0].equipment[FF6_EQUIP_RIGHT_HAND];
    CHECK(weapon != FF6_ITEM_EMPTY && (weapon == 0 || weapon == 1) && power(weapon) >= power(weapon == 0 ? 1 : 0));
    CHECK(optimum.characters[0].equipment[FF6_EQUIP_HEAD] == 0x69);
}

void testItemUse(const ff6::GameRules& rules) {
    ff6::PartyState party;
    ff6::setCharacterProperties(party, rules, 0, 0, 0);
    ff6::CharacterState& terra = party.characters[0];
    terra.equipment.fill(FF6_ITEM_EMPTY);
    for (const int item : {233, 233, 238, 239, 240, 246, 247}) ff6::giveItem(party, item);  // potion, elixir, megalixir, fenix down, bag, tent
    CHECK(ff6::itemUsableInMenu(rules, 233) && ff6::itemUsableInMenu(rules, 238) && ff6::itemUsableInMenu(rules, 240));
    CHECK(!ff6::itemUsableInMenu(rules, 239) && ff6::itemUsableInMenu(rules, 246) && !ff6::itemUsableInMenu(rules, 247));  // the tent leaves the menu
    CHECK(!ff6::itemUsableInMenu(rules, 1) && !ff6::itemUsableInMenu(rules, FF6_ITEM_EMPTY));  // a weapon, and nothing

    CHECK(!ff6::canUseItemOn(party, rules, 0, 233));  // nothing to restore
    terra.hp = 10;
    CHECK(ff6::useItemOn(party, rules, 0, 233) && terra.hp == terra.maxHp);  // 250 is more than she lacks
    CHECK(party.inventory[0].item == 233 && party.inventory[0].quantity == 1);
    terra.hp = 0;
    CHECK(!ff6::canUseItemOn(party, rules, 0, 233) && !ff6::canUseItemOn(party, rules, 0, 238));  // a dead character
    CHECK(ff6::useItemOn(party, rules, 0, 240) && terra.hp == (terra.maxHp * 2) >> 4);  // two sixteenths of the maximum
    CHECK(!ff6::canUseItemOn(party, rules, 0, 240));  // she is alive now
    terra.mp = 0;
    CHECK(ff6::useItemOn(party, rules, 0, 238) && terra.hp == terra.maxHp && terra.mp == terra.maxMp);  // the elixir
    CHECK(!ff6::useItemOn(party, rules, 0, 233));  // she is full of hp and the potion is of no use
    CHECK(!ff6::useItemOn(party, rules, 0, 99));  // and an item that she does not have
}

void testArrange() {
    ff6::PartyState party;
    std::vector<uint8_t> names(static_cast<size_t>(FF6_ITEM_COUNT) * FF6_ITEM_NAME_BYTES, 0xFF);
    names[5 * FF6_ITEM_NAME_BYTES] = 0xD9;
    names[6 * FF6_ITEM_NAME_BYTES] = 0xD8;
    names[8 * FF6_ITEM_NAME_BYTES] = 0xD9;
    names[7 * FF6_ITEM_NAME_BYTES] = 0x00;  // a symbol that the game does not sort: last
    party.inventory[1] = {5, 3};
    party.inventory[3] = {7, 1};
    party.inventory[4] = {8, 2};
    party.inventory[6] = {6, 9};
    ff6::arrangeItems(party, names);
    CHECK(party.inventory[0].item == 6 && party.inventory[0].quantity == 9);  // 0xD8 first
    CHECK(party.inventory[1].item == 5 && party.inventory[2].item == 8 && party.inventory[3].item == 7);  // 0xD9 in their order
    CHECK(party.inventory[4].item == FF6_ITEM_EMPTY && party.inventory[4].quantity == 0);
}

void testPoison(const ff6::GameRules& rules) {
    ff6::PartyState party;
    ff6::setCharacterProperties(party, rules, 0, 0, 0);
    ff6::CharacterState& terra = party.characters[0];
    terra.equipment.fill(FF6_ITEM_EMPTY);
    terra.hp = terra.maxHp = 320;
    const int maximum = ff6::calcStats(rules, terra).maxHp;
    CHECK(!ff6::poisonStep(party, rules, {0}) && terra.hp == 320);  // not poisoned: nothing happens
    terra.status = FF6_ST_POISON;
    CHECK(ff6::poisonStep(party, rules, {0}) && terra.hp == 320 - (maximum >> FF6_POISON_SHIFT));
    CHECK(!ff6::poisonStep(party, rules, {}) && terra.hp == 320 - (maximum >> FF6_POISON_SHIFT));  // a character out of the party is not touched
    terra.hp = 5;
    ff6::poisonStep(party, rules, {0});
    CHECK(terra.hp == 1);  // the poison does not kill
    terra.hp = 1;
    ff6::poisonStep(party, rules, {0});
    CHECK(terra.hp == 1);
    // The relic of recovery heals a fourth of the stamina at each step.
    terra.status = 0;
    terra.stamina = 40;
    terra.hp = 100;
    terra.equipment[FF6_EQUIP_FIRST_RELIC + 1] = FF6_ITEM_TINTINABAR;
    ff6::poisonStep(party, rules, {0});
    CHECK(terra.hp == 110);
    terra.status = FF6_ST_ZOMBIE;
    ff6::poisonStep(party, rules, {0});
    CHECK(terra.hp == 110);  // not the undead
    // The commands of the story.
    ff6::changeStatus(party, 0, FF6_EVENT_SET_STATUS, FF6_ST_POISON | FF6_ST_SLEEP);
    CHECK(terra.status == (FF6_ST_ZOMBIE | FF6_ST_POISON));  // sleep does not last
    ff6::changeStatus(party, 0, FF6_EVENT_AND_STATUS, 0xFFFF ^ FF6_ST_ZOMBIE);
    CHECK(terra.status == FF6_ST_POISON);
    ff6::changeStatus(party, 0, FF6_EVENT_TOGGLE_STATUS, FF6_ST_POISON | FF6_ST_BLIND);
    CHECK(terra.status == FF6_ST_BLIND);
    ff6::changeStatus(party, 0, FF6_EVENT_AND_STATUS, 0);
    CHECK(terra.status == 0);
    ff6::changeStatus(party, 0, FF6_EVENT_SET_STATUS, FF6_ST_MAGITEK);
    CHECK(terra.status == FF6_ST_MAGITEK);  // the armor lasts until the story takes it off
    ff6::changeStatus(party, 0, FF6_EVENT_AND_STATUS, 0);
    CHECK(terra.status == 0);
    // An item that takes the status away, which is of use only when the character has it.
    int antidote = -1;
    for (int item = 0; item < FF6_ITEM_COUNT && antidote < 0; ++item) {
        const uint8_t* prop = rules.item(item);
        if ((prop[FF6_ITEM_PROP_TYPE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_CONSUMABLE && (prop[FF6_ITEM_PROP_FLAGS] & FF6_ITEM_FLAG_REMOVE_STATUS) &&
            prop[FF6_ITEM_PROP_STATUS] == FF6_ST_POISON) {
            antidote = item;
        }
    }
    CHECK(antidote >= 0 && ff6::itemUsableInMenu(rules, antidote));
    ff6::giveItem(party, antidote);
    CHECK(!ff6::canUseItemOn(party, rules, 0, antidote));
    terra.status = FF6_ST_POISON | FF6_ST_BLIND;
    CHECK(ff6::useItemOn(party, rules, 0, antidote) && terra.status == FF6_ST_BLIND && ff6::itemQuantity(party, antidote) == 0);
    // The sleeping bag gives back all of the hp and mp and cures what lasts, but does not raise the dead.
    ff6::giveItem(party, FF6_ITEM_SLEEPING_BAG);
    terra.status = FF6_ST_POISON | FF6_ST_BLIND;
    terra.hp = 3;
    terra.mp = 0;
    CHECK(ff6::useItemOn(party, rules, 0, FF6_ITEM_SLEEPING_BAG) && terra.status == 0 && terra.hp == terra.maxHp && terra.mp == terra.maxMp);
    ff6::giveItem(party, FF6_ITEM_SLEEPING_BAG);
    CHECK(!ff6::canUseItemOn(party, rules, 0, FF6_ITEM_SLEEPING_BAG));  // nothing to give back
    terra.hp = 0;
    CHECK(!ff6::canUseItemOn(party, rules, 0, FF6_ITEM_SLEEPING_BAG));
}

void testShops(const ff6::GameRules& rules) {
    const std::vector<int> stock = ff6::shopStock(rules, 0);  // the weapons of Narshe
    CHECK(stock.size() == 7 && (rules.shop(0)[0] & FF6_SHOP_KIND_MASK) == 1);
    CHECK(ff6::shopStock(rules, -1).empty() && ff6::shopStock(rules, FF6_SHOP_COUNT).empty());
    const int price = rules.item(stock[0])[FF6_ITEM_PROP_PRICE] | (rules.item(stock[0])[FF6_ITEM_PROP_PRICE + 1] << 8);
    CHECK(price > 0 && ff6::shopPrice(rules, 0, stock[0], 0) == price && ff6::sellPrice(rules, stock[0]) == price / 2);
    // Every price modifier of the game, in the shops that have one.
    int seen = 0;
    for (int shop = 0; shop < FF6_SHOP_COUNT; ++shop) {
        const int mod = (rules.shop(shop)[0] >> FF6_SHOP_PRICE_SHIFT) & FF6_SHOP_PRICE_MASK;
        if (mod == 0) continue;
        const int item = ff6::shopStock(rules, shop).empty() ? 0 : ff6::shopStock(rules, shop)[0];
        const int base = rules.item(item)[FF6_ITEM_PROP_PRICE] | (rules.item(item)[FF6_ITEM_PROP_PRICE + 1] << 8);
        const int terra = ff6::shopPrice(rules, shop, item, 0), locke = ff6::shopPrice(rules, shop, item, 1);
        const int edgar = ff6::shopPrice(rules, shop, item, FF6_SHOPPER_EDGAR);
        seen |= 1 << mod;
        if (mod == FF6_PRICE_PLUS_50) CHECK(terra == base + base / 2);
        if (mod == FF6_PRICE_PLUS_100) CHECK(terra == base * 2);
        if (mod == FF6_PRICE_HALF) CHECK(terra == base / 2);
        if (mod == FF6_PRICE_HALF_FOR_FEMALE) CHECK(terra == base / 2 && locke == base + base / 2);
        if (mod == FF6_PRICE_HALF_FOR_MALE) CHECK(locke == base / 2 && terra == base + base / 2);
        if (mod == FF6_PRICE_HALF_FOR_EDGAR) CHECK(edgar == base / 2 && locke == base);
    }
    CHECK(seen != 0);  // the game has such shops

    ff6::PartyState party;
    party.gil = 1000;
    CHECK(ff6::buyItem(party, 233, 3, 50) && party.gil == 850 && ff6::itemQuantity(party, 233) == 3);
    CHECK(!ff6::buyItem(party, 233, 97, 1) && party.gil == 850);  // 100 would be too many
    CHECK(!ff6::buyItem(party, 233, 20, 50) && ff6::itemQuantity(party, 233) == 3);  // and 1000 is more than she has
    CHECK(!ff6::buyItem(party, 233, 0, 50) && !ff6::buyItem(party, FF6_ITEM_EMPTY, 1, 0));
    CHECK(ff6::sellItem(party, 233, 2, 25) && party.gil == 900 && ff6::itemQuantity(party, 233) == 1);
    CHECK(!ff6::sellItem(party, 233, 2, 25) && !ff6::sellItem(party, 7, 1, 25));  // not that many, and not that item
    CHECK(ff6::sellItem(party, 233, 1, 25) && party.inventory[0].item == FF6_ITEM_EMPTY);  // the place is free again
    party.gil = FF6_MAX_GIL - 10;
    ff6::giveItem(party, 233);
    CHECK(ff6::sellItem(party, 233, 1, 100) && party.gil == FF6_MAX_GIL);
}

void testAbilities(const ff6::GameRules& rules) {
    ff6::PartyState party;
    for (int i = 0; i < FF6_CHAR_COUNT; ++i) ff6::setCharacterProperties(party, rules, i, i, 0);
    // Terra knows what her level 3 teaches: cure at level 1 and fire at level 3, not poisona, which comes at level 6.
    CHECK(party.spells[0][45] && party.spells[0][0] && !party.spells[0][50]);
    CHECK(party.spells[6][1] && !party.spells[6][45]);  // Celes has ice, and cure only from level 4
    CHECK(!party.spells[1][45]);  // nobody else learns magic by level
    CHECK(party.swordtechs == 1 && party.blitzes == 1);  // dispatch, and pummel: Cyan and Sabin are at level 5
    ff6::addExperience(party, rules, 0, FF6_MAX_EXPERIENCE);  // Terra goes up to level 99 and learns everything
    CHECK(party.characters[0].level == FF6_MAX_LEVEL && party.spells[0][50] && party.spells[0][20]);  // poisona, ultima
    ff6::addExperience(party, rules, 2, ff6::experienceForLevel(rules, 35) - party.characters[2].experience);
    CHECK(party.characters[2].level == 35 && party.swordtechs == 0x3F);  // levels 1, 6, 12, 15, 24 and 34
    ff6::addExperience(party, rules, 5, ff6::experienceForLevel(rules, 35) - party.characters[5].experience);
    CHECK(party.blitzes == 0x3F);  // levels 1, 6, 10, 15, 23 and 30
}

void testEspers() {
    ff6::PartyState party;
    ff6::giveEsper(party, FF6_ESPER_FIRST_ID + 3);
    ff6::giveEsper(party, FF6_ESPER_FIRST_ID + FF6_ESPER_COUNT);  // not an esper
    ff6::giveEsper(party, 5);
    CHECK(party.espers[3]);
    int known = 0;
    for (const bool esper : party.espers) known += esper ? 1 : 0;
    CHECK(known == 1);
    ff6::takeEsper(party, FF6_ESPER_FIRST_ID + 3);
    CHECK(!party.espers[3]);
}

void testExperience(const ff6::GameRules& rules) {
    ff6::PartyState party;
    ff6::setCharacterProperties(party, rules, 0, 0, 0);
    ff6::CharacterState& terra = party.characters[0];
    terra.hp = 5;
    const int hp = terra.maxHp, mp = terra.maxMp;
    CHECK(ff6::experienceForLevel(rules, 4) == 8 * (4 + 8 + 14));
    ff6::addExperience(party, rules, 0, 100);  // 96 + 100 is not enough for level 4, which needs 208
    CHECK(terra.level == 3 && terra.experience == 196 && terra.maxHp == hp);
    ff6::addExperience(party, rules, 0, 12);
    CHECK(terra.level == 4 && terra.maxHp == hp + rules.levelHp[2] && terra.maxMp == mp + rules.levelMp[2]);
    CHECK(terra.hp == 5);  // a level does not heal
    ff6::addExperience(party, rules, 0, 100000000);  // all the levels at once, up to the limits
    CHECK(terra.level == FF6_MAX_LEVEL && terra.experience == FF6_MAX_EXPERIENCE);
    CHECK(terra.maxHp <= FF6_MAX_HP && terra.maxMp <= FF6_MAX_MP && terra.maxHp > hp);
    ff6::addExperience(party, rules, 0, -5);  // nothing is taken away
    CHECK(terra.experience == FF6_MAX_EXPERIENCE);
}

}  // namespace

// The rules come from the real data pack when FF6_PACK points to one.
void testParty() {
    testInventory();
    testEspers();
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::GameRules rules = ff6::loadGameRules(pack);
    CHECK(rules.charProp.size() == static_cast<size_t>(FF6_CHAR_PROP_COUNT) * FF6_CHAR_PROP_BYTES);
    CHECK(rules.levelExp.size() == FF6_LEVEL_STEPS && rules.levelExp[0] == 4 && rules.levelExp[FF6_LEVEL_STEPS - 1] == 11111);
    CHECK(rules.levelHp[0] == 11 && rules.levelMp[0] == 4);
    CHECK(rules.actorName(0) == "Terra" && rules.actorName(0x20) == "Wedge" && rules.actorName(0x21) == "Vicks");  // char_name
    testCharacters(rules);
    testStats(rules);
    testHpChanges(rules);
    testEquipmentRemoval(rules);
    testEquipping(rules);
    testItemUse(rules);
    testArrange();
    testPoison(rules);
    testShops(rules);
    testAbilities(rules);
    testExperience(rules);
}
