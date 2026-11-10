#include "state/party_ops.hpp"

#include <algorithm>

#include "state/char_stats.hpp"

namespace ff6 {
namespace {

bool validSlot(int slot) { return slot >= 0 && slot < FF6_CHAR_COUNT; }

int levelModifier(const GameRules& rules, int actor) {
    static constexpr int kMods[] = FF6_CHAR_LEVEL_MODS;
    return kMods[(rules.actor(actor)[FF6_CHAR_PROP_FLAGS] & FF6_CHAR_LEVEL_MOD_MASK) >> FF6_CHAR_LEVEL_MOD_SHIFT];
}

InventorySlot* findItem(PartyState& party, int item) {
    for (InventorySlot& slot : party.inventory) {
        if (slot.item == item) return &slot;
    }
    return nullptr;
}

}  // namespace

int averageLevel(const PartyState& party, CharacterMask available) {
    int sum = 0, count = 0;
    for (int i = 0; i < FF6_PLAYABLE_COUNT; ++i) {
        if (!(available & (1u << i))) continue;
        sum += party.characters[static_cast<size_t>(i)].level;
        ++count;
    }
    return std::min(count ? sum / count : FF6_DEFAULT_LEVEL, FF6_MAX_LEVEL);
}

void updateExperience(const GameRules& rules, CharacterState& character) {
    long total = 0;
    for (int step = 0; step < character.level - 1; ++step) total += rules.levelExp[static_cast<size_t>(step)];
    character.experience = total << FF6_EXP_SHIFT;
}

void setCharacterPropertiesOnly(PartyState& party, const GameRules& rules, int slot, int actor, CharacterMask available);

void setCharacterProperties(PartyState& party, const GameRules& rules, int slot, int actor, CharacterMask available) {
    if (!validSlot(slot) || actor < 0 || actor >= FF6_CHAR_PROP_COUNT) return;
    setCharacterPropertiesOnly(party, rules, slot, actor, available);
    updateAbilities(party, rules, slot);
}

void updateAbilities(PartyState& party, const GameRules& rules, int slot) {
    if (!validSlot(slot)) return;
    const CharacterState& character = party.characters[static_cast<size_t>(slot)];
    if (character.actor == FF6_NATURAL_MAGIC_TERRA || character.actor == FF6_NATURAL_MAGIC_CELES) {
        const bool terra = character.actor == FF6_NATURAL_MAGIC_TERRA;
        const size_t base = terra ? 0 : static_cast<size_t>(FF6_NATURAL_MAGIC_PAIRS) * 2;
        for (int i = 0; i < FF6_NATURAL_MAGIC_PAIRS; ++i) {
            const int spell = rules.naturalMagic[base + static_cast<size_t>(i) * 2];
            const int level = rules.naturalMagic[base + static_cast<size_t>(i) * 2 + 1];
            if (level > character.level) break;  // the first that is above the level ends the list
            if (spell < FF6_SPELL_COUNT) party.spells[terra ? FF6_NATURAL_MAGIC_TERRA : FF6_NATURAL_MAGIC_CELES][static_cast<size_t>(spell)] = true;
        }
    } else if (character.actor == FF6_ACTOR_CYAN || character.actor == FF6_ACTOR_SABIN) {
        const std::vector<uint8_t>& levels = character.actor == FF6_ACTOR_CYAN ? rules.bushidoLevels : rules.blitzLevels;
        unsigned known = 0;
        for (size_t i = 0; i < levels.size() && levels[i] <= character.level; ++i) known |= 1u << i;
        (character.actor == FF6_ACTOR_CYAN ? party.swordtechs : party.blitzes) |= known;
    }
}

void giveEsper(PartyState& party, int id) {
    const int index = id - FF6_ESPER_FIRST_ID;
    if (index >= 0 && index < FF6_ESPER_COUNT) party.espers[static_cast<size_t>(index)] = true;
}

void takeEsper(PartyState& party, int id) {
    const int index = id - FF6_ESPER_FIRST_ID;
    if (index >= 0 && index < FF6_ESPER_COUNT) party.espers[static_cast<size_t>(index)] = false;
}

// The part of EventCmd_40 that is not about abilities.
void setCharacterPropertiesOnly(PartyState& party, const GameRules& rules, int slot, int actor, CharacterMask available) {
    const uint8_t* prop = rules.actor(actor);
    CharacterState& character = party.characters[static_cast<size_t>(slot)];
    character.actor = actor;
    character.vigor = prop[FF6_CHAR_PROP_VIGOR];
    character.speed = prop[FF6_CHAR_PROP_VIGOR + 1];
    character.stamina = prop[FF6_CHAR_PROP_VIGOR + 2];
    character.magicPower = prop[FF6_CHAR_PROP_VIGOR + 3];
    for (size_t i = 0; i < character.equipment.size(); ++i) character.equipment[i] = prop[FF6_CHAR_PROP_EQUIPMENT + i];
    const int level = averageLevel(party, available) + levelModifier(rules, actor);
    character.level = std::max(1, std::min(level, FF6_MAX_LEVEL));
    character.maxHp = prop[FF6_CHAR_PROP_HP];
    character.maxMp = prop[FF6_CHAR_PROP_MP];
    for (int step = 0; step < character.level - 1; ++step) {
        character.maxHp += rules.levelHp[static_cast<size_t>(step)];
        character.maxMp += rules.levelMp[static_cast<size_t>(step)];
    }
    character.hp = character.maxHp;
    character.mp = character.maxMp;
    updateExperience(rules, character);
}

void normalizeLevel(PartyState& party, const GameRules& rules, int slot, CharacterMask available) {
    if (!validSlot(slot)) return;
    const int average = averageLevel(party, available);
    CharacterState& character = party.characters[static_cast<size_t>(slot)];
    if (average >= character.level) {
        for (int step = character.level - 1; step < average - 1; ++step) {
            character.maxHp += rules.levelHp[static_cast<size_t>(step)];
            character.maxMp += rules.levelMp[static_cast<size_t>(step)];
        }
        character.maxHp = std::min(character.maxHp, FF6_MAX_HP);
        character.maxMp = std::min(character.maxMp, FF6_MAX_MP);
        character.level = average;
        character.hp = character.maxHp;
        character.mp = character.maxMp;
        updateAbilities(party, rules, slot);
    }
    updateExperience(rules, character);
}

void giveItem(PartyState& party, int item) {
    if (InventorySlot* have = findItem(party, item)) {
        have->quantity = std::min(have->quantity + 1, FF6_MAX_ITEM_QUANTITY);
    } else if (InventorySlot* empty = findItem(party, FF6_ITEM_EMPTY)) {
        empty->item = item;
        empty->quantity = 1;
    }
}

void takeItem(PartyState& party, int item) {
    InventorySlot* have = findItem(party, item);
    if (!have) return;
    if (--have->quantity <= 0) *have = InventorySlot();
}

void giveGil(PartyState& party, long amount) { party.gil = std::min(party.gil + amount, static_cast<long>(FF6_MAX_GIL)); }

bool takeGil(PartyState& party, long amount) {
    if (party.gil < amount) return false;
    party.gil -= amount;
    return true;
}

// Only the first values of the tables of the game are known: the others change nothing.
void changeHp(PartyState& party, const GameRules& rules, int slot, int code) {
    static constexpr int kChanges[] = FF6_HP_CHANGES;
    if (!validSlot(slot)) return;
    CharacterState& character = party.characters[static_cast<size_t>(slot)];
    const int maximum = calcStats(rules, character).maxHp;
    const int index = code & FF6_HP_CHANGE_SET_MAX;
    if (index == FF6_HP_CHANGE_SET_MAX) {
        character.hp = maximum;
    } else if (index < static_cast<int>(sizeof(kChanges) / sizeof(kChanges[0]))) {
        if (!(code & FF6_HP_CHANGE_SUBTRACT)) {
            character.hp = std::min(character.hp + kChanges[index], maximum);
        } else if (character.hp != 0) {
            character.hp = character.hp - kChanges[index] > 0 ? character.hp - kChanges[index] : 1;
        }
    }
}

// The game adds and takes 0 mp with the commands that it has: only the maximum changes anything.
void changeMp(PartyState& party, const GameRules& rules, int slot, int code) {
    if (!validSlot(slot)) return;
    CharacterState& character = party.characters[static_cast<size_t>(slot)];
    const int maximum = calcStats(rules, character).maxMp;
    if ((code & FF6_HP_CHANGE_SET_MAX) == FF6_HP_CHANGE_SET_MAX) {
        character.mp = maximum;
    } else if (!(code & FF6_HP_CHANGE_SUBTRACT)) {
        character.mp = std::min(character.mp, maximum);
    }
}

void removeEquipment(PartyState& party, int slot) {
    if (!validSlot(slot)) return;
    for (int& item : party.characters[static_cast<size_t>(slot)].equipment) {
        if (item == FF6_ITEM_EMPTY) continue;
        const int removed = item;
        item = FF6_ITEM_EMPTY;
        giveItem(party, removed);
    }
}

bool poisonStep(PartyState& party, const GameRules& rules, const std::vector<int>& members) {
    bool poisoned = false;
    for (const int slot : members) {
        if (!validSlot(slot)) continue;
        CharacterState& character = party.characters[static_cast<size_t>(slot)];
        const unsigned status = character.status | (character.hp == 0 ? FF6_ST_DEAD : 0u);
        const int maximum = calcStats(rules, character).maxHp;
        if (!(status & FF6_POISON_IMMUNE) &&
            (character.equipment[FF6_EQUIP_FIRST_RELIC] == FF6_ITEM_TINTINABAR || character.equipment[FF6_EQUIP_FIRST_RELIC + 1] == FF6_ITEM_TINTINABAR)) {
            character.hp = std::min(character.hp + (character.stamina >> 2), maximum);
        }
        if (!(character.status & FF6_ST_POISON) || character.hp == 0) continue;
        character.hp = std::max(1, character.hp - (maximum >> FF6_POISON_SHIFT));
        poisoned = true;
    }
    return poisoned;
}

void changeStatus(PartyState& party, int slot, int op, unsigned bits) {
    if (!validSlot(slot)) return;
    unsigned& status = party.characters[static_cast<size_t>(slot)].status;
    if (op == FF6_EVENT_AND_STATUS) {
        status &= bits | ~FF6_ST_PERSISTENT;
    } else if (op == FF6_EVENT_SET_STATUS) {
        status |= bits & FF6_ST_PERSISTENT;
    } else {
        status ^= bits & FF6_ST_PERSISTENT;
    }
}

void countStep(PartyState& party) { party.steps = std::min(party.steps + 1, static_cast<long>(FF6_MAX_STEPS)); }

std::vector<int> equippableItems(const PartyState& party, const GameRules& rules, int character, int slot) {
    std::vector<int> items;
    if (!validSlot(character) || slot < 0 || slot >= FF6_MENU_EQUIP_SLOTS) return items;
    const CharacterState& state = party.characters[static_cast<size_t>(character)];
    const CharacterStats stats = calcStats(rules, state);
    const unsigned mask = (1u << state.actor) | (stats.equipAnything ? FF6_EQUIP_ANYONE_BIT : 0u);
    const bool hand = slot == FF6_EQUIP_RIGHT_HAND || slot == FF6_EQUIP_LEFT_HAND;
    int otherType = -1;  // what the other hand holds
    if (hand) {
        const int other = state.equipment[slot == FF6_EQUIP_RIGHT_HAND ? FF6_EQUIP_LEFT_HAND : FF6_EQUIP_RIGHT_HAND];
        if (other != FF6_ITEM_EMPTY) otherType = rules.item(other)[FF6_ITEM_PROP_TYPE] & FF6_ITEM_PROP_TYPE_MASK;
    }
    std::array<bool, FF6_ITEM_COUNT> listed{};
    for (const InventorySlot& held : party.inventory) {
        if (held.item == FF6_ITEM_EMPTY || listed[static_cast<size_t>(held.item)]) continue;
        const uint8_t* prop = rules.item(held.item);
        const int type = prop[FF6_ITEM_PROP_TYPE] & FF6_ITEM_PROP_TYPE_MASK;
        const bool fits = hand ? type == FF6_ITEM_TYPE_WEAPON || type == FF6_ITEM_TYPE_SHIELD
                               : type == (slot == FF6_EQUIP_HEAD ? FF6_ITEM_TYPE_HELMET
                                          : slot == FF6_EQUIP_BODY ? FF6_ITEM_TYPE_ARMOR : FF6_ITEM_TYPE_RELIC);
        const unsigned wearers = prop[FF6_ITEM_PROP_EQUIP_CHARS] | (prop[FF6_ITEM_PROP_EQUIP_CHARS + 1] << 8);
        if (!fits || !(wearers & mask)) continue;
        // Two weapons only with the genji glove, and never two shields.
        if (hand && otherType >= 0 && (otherType == FF6_ITEM_TYPE_SHIELD || !stats.genjiGlove) && otherType == type) continue;
        listed[static_cast<size_t>(held.item)] = true;
        items.push_back(held.item);
    }
    return items;
}

// The statuses that last after a battle and that an item takes away.
static unsigned curedStatuses(const uint8_t* prop) {
    if (!(prop[FF6_ITEM_PROP_FLAGS] & FF6_ITEM_FLAG_REMOVE_STATUS)) return 0;
    return static_cast<unsigned>(prop[FF6_ITEM_PROP_STATUS] | (prop[FF6_ITEM_PROP_STATUS + 1] << 8)) & FF6_ST_PERSISTENT;
}

bool itemUsableInMenu(const GameRules& rules, int item) {
    if (item < 0 || item >= FF6_ITEM_COUNT || item == FF6_ITEM_MEGALIXIR || item == FF6_ITEM_SLEEPING_BAG ||
        item == FF6_ITEM_TENT || item == FF6_ITEM_WARP_STONE) {
        return false;
    }
    const uint8_t* prop = rules.item(item);
    if ((prop[FF6_ITEM_PROP_TYPE] & FF6_ITEM_PROP_TYPE_MASK) != FF6_ITEM_TYPE_CONSUMABLE) return false;
    if (!(prop[FF6_ITEM_PROP_TYPE] & FF6_ITEM_USAGE_MENU)) return false;
    const int flags = prop[FF6_ITEM_PROP_FLAGS];
    return !(flags & FF6_ITEM_FLAG_DAMAGE) && ((flags & (FF6_ITEM_FLAG_HP | FF6_ITEM_FLAG_MP)) || curedStatuses(prop) != 0);
}

bool canUseItemOn(const PartyState& party, const GameRules& rules, int character, int item) {
    if (!validSlot(character) || !itemUsableInMenu(rules, item)) return false;
    bool held = false;
    for (const InventorySlot& slot : party.inventory) held = held || slot.item == item;
    if (!held) return false;
    const CharacterState& state = party.characters[static_cast<size_t>(character)];
    const uint8_t* prop = rules.item(item);
    const int flags = prop[FF6_ITEM_PROP_FLAGS];
    const bool revives = (flags & FF6_ITEM_FLAG_REMOVE_STATUS) && (prop[FF6_ITEM_PROP_STATUS] & FF6_STATUS_DEAD);
    if (state.hp == 0) return revives;  // a character without hp is dead
    if (curedStatuses(prop) & state.status) return true;
    if (flags & FF6_ITEM_FLAG_REMOVE_STATUS) return false;
    const CharacterStats stats = calcStats(rules, state);
    return ((flags & FF6_ITEM_FLAG_HP) && state.hp < stats.maxHp) || ((flags & FF6_ITEM_FLAG_MP) && state.mp < stats.maxMp);
}

bool useItemOn(PartyState& party, const GameRules& rules, int character, int item) {
    if (!canUseItemOn(party, rules, character, item)) return false;
    CharacterState& state = party.characters[static_cast<size_t>(character)];
    const uint8_t* prop = rules.item(item);
    const int flags = prop[FF6_ITEM_PROP_FLAGS], power = prop[FF6_ITEM_PROP_POWER];
    const CharacterStats stats = calcStats(rules, state);
    const auto amount = [&](int maximum) {
        return flags & FF6_ITEM_FLAG_FRACTION ? (maximum * power) >> FF6_ITEM_FRACTION_SHIFT : power;
    };
    state.status &= ~curedStatuses(prop);
    if (flags & FF6_ITEM_FLAG_HP) state.hp = std::min(state.hp + amount(stats.maxHp), stats.maxHp);
    if (flags & FF6_ITEM_FLAG_MP) state.mp = std::min(state.mp + amount(stats.maxMp), stats.maxMp);
    takeItem(party, item);
    return true;
}

long experienceForLevel(const GameRules& rules, int level) {
    long total = 0;
    for (int step = 0; step < level - 1 && step < FF6_LEVEL_STEPS; ++step) total += rules.levelExp[static_cast<size_t>(step)];
    return total << FF6_EXP_SHIFT;
}

int addExperience(PartyState& party, const GameRules& rules, int character, long amount) {
    if (!validSlot(character) || amount <= 0) return 0;
    CharacterState& state = party.characters[static_cast<size_t>(character)];
    state.experience = std::min(state.experience + amount, static_cast<long>(FF6_MAX_EXPERIENCE));
    int gained = 0;
    while (state.level < FF6_MAX_LEVEL && state.experience >= experienceForLevel(rules, state.level + 1)) {
        int hp = rules.levelHp[static_cast<size_t>(state.level - 1)], mp = rules.levelMp[static_cast<size_t>(state.level - 1)];
        const int bonus = state.esper >= 0 ? rules.genju(state.esper)[FF6_GENJU_PROP_BYTES - 1] : FF6_GENJU_BONUS_NONE;
        // The esper raises the hp or mp gained by a tenth, three tenths, a half or all of it (at least 1), or a stat by 1 or 2.
        static constexpr int kPercent[] = {26, 78, 128};
        const auto more = [](int value, int percent) { return value + std::max((value * percent) >> 8, 1); };
        if (bonus <= 2) hp = more(hp, kPercent[bonus]);
        else if (bonus <= 5) mp = more(mp, kPercent[bonus - 3]);
        else if (bonus == 6) hp += hp;
        else if (bonus >= 9 && bonus <= 16) {
            const int kind = (bonus - 9) / 2, step = bonus % 2 ? 1 : 2;
            int& stat = kind == 0 ? state.vigor : kind == 1 ? state.speed : kind == 2 ? state.stamina : state.magicPower;
            stat = std::min(stat + step, FF6_GENJU_BONUS_STAT_MAX);
        }
        state.maxHp = std::min(state.maxHp + hp, FF6_MAX_HP);
        state.maxMp = std::min(state.maxMp + mp, FF6_MAX_MP);
        ++state.level;
        ++gained;
    }
    updateAbilities(party, rules, character);
    return gained;
}

std::vector<LearnedSpell> learnSpells(PartyState& party, const GameRules& rules, const std::vector<int>& characters, int points) {
    std::vector<LearnedSpell> learned;
    for (const int character : characters) {
        if (!validSlot(character) || points <= 0) continue;
        const CharacterState& state = party.characters[static_cast<size_t>(character)];
        if (state.actor >= FF6_ACTOR_GOGO) continue;
        std::array<bool, FF6_SPELL_COUNT> reached{};
        const auto teach = [&](int rate, int spell) {
            if (rate == 0 || spell < 0 || spell >= FF6_SPELL_COUNT || party.spells[static_cast<size_t>(character)][static_cast<size_t>(spell)]) return;
            uint8_t& progress = party.spellLearn[static_cast<size_t>(character)][static_cast<size_t>(spell)];
            const int total = progress + rate * points;
            if (total >= 100) {
                reached[static_cast<size_t>(spell)] = true;
                progress = 0;
            } else {
                progress = static_cast<uint8_t>(total);
            }
        };
        for (const int id : state.equipment) {
            const int item = id & 0xFF;
            if (item != FF6_ITEM_EMPTY) teach(rules.item(item)[FF6_EQUIP_ITEM_SPELL_RATE], rules.item(item)[FF6_EQUIP_ITEM_SPELL]);
        }
        if (state.esper >= 0) {
            const uint8_t* esper = rules.genju(state.esper);
            for (int i = 0; i < FF6_GENJU_SPELLS; ++i) teach(esper[i * 2], esper[i * 2 + 1]);
        }
        for (int spell = FF6_SPELL_COUNT - 1; spell >= 0; --spell) {
            if (!reached[static_cast<size_t>(spell)]) continue;
            party.spells[static_cast<size_t>(character)][static_cast<size_t>(spell)] = true;
            learned.push_back({character, spell});
        }
    }
    return learned;
}

bool equipItem(PartyState& party, const GameRules& rules, int character, int slot, int item) {
    if (!validSlot(character) || slot < 0 || slot >= FF6_MENU_EQUIP_SLOTS) return false;
    CharacterState& state = party.characters[static_cast<size_t>(character)];
    const int old = state.equipment[static_cast<size_t>(slot)];
    InventorySlot* taken = item == FF6_ITEM_EMPTY ? nullptr : findItem(party, item);
    if (item != FF6_ITEM_EMPTY && !taken) return false;
    if (old != FF6_ITEM_EMPTY) {  // the old item needs a place: its own, an empty one, or the one that the new item frees
        const InventorySlot* same = findItem(party, old);
        const bool room = (same && same->quantity < FF6_MAX_ITEM_QUANTITY) || findItem(party, FF6_ITEM_EMPTY) ||
                          (taken && taken->quantity == 1);
        if (!room) return false;
    }
    if (item != FF6_ITEM_EMPTY) takeItem(party, item);
    if (old != FF6_ITEM_EMPTY) giveItem(party, old);
    state.equipment[static_cast<size_t>(slot)] = item;
    const CharacterStats stats = calcStats(rules, state);  // a relic that raised the maximum may have been taken off
    state.hp = std::min(state.hp, stats.maxHp);
    state.mp = std::min(state.mp, stats.maxMp);
    return true;
}

void optimizeEquipment(PartyState& party, const GameRules& rules, int character) {
    if (!validSlot(character)) return;
    for (int slot = FF6_EQUIP_RIGHT_HAND; slot <= FF6_EQUIP_BODY; ++slot) equipItem(party, rules, character, slot, FF6_ITEM_EMPTY);
    static constexpr int kImpItems[] = FF6_IMP_ITEMS;
    // The best of the items that fit a slot (type -1 for any): by attack or defense power, the first of the inventory among equals.
    const auto best = [&](int slot, int type, bool twoHanded) {
        std::vector<int> items;
        for (const int item : equippableItems(party, rules, character, slot)) {
            const bool imp = std::find(std::begin(kImpItems), std::end(kImpItems), item) != std::end(kImpItems);
            if (!imp && (type < 0 || (rules.item(item)[FF6_ITEM_PROP_TYPE] & FF6_ITEM_PROP_TYPE_MASK) == type)) items.push_back(item);
        }
        std::stable_sort(items.begin(), items.end(), [&](int a, int b) {
            return rules.item(a)[FF6_ITEM_PROP_ITEM_POWER] > rules.item(b)[FF6_ITEM_PROP_ITEM_POWER];
        });
        if (twoHanded) {
            for (const int item : items) {
                if (rules.item(item)[FF6_ITEM_PROP_WEAPON_FLAGS] & FF6_ITEM_TWO_HANDED) return item;
            }
        }
        return items.empty() ? FF6_ITEM_EMPTY : items.front();
    };
    const CharacterStats stats = calcStats(rules, party.characters[static_cast<size_t>(character)]);
    if (stats.relic4 & FF6_RELIC4_GAUNTLET) {
        equipItem(party, rules, character, FF6_EQUIP_RIGHT_HAND, best(FF6_EQUIP_RIGHT_HAND, FF6_ITEM_TYPE_WEAPON, true));
    } else {
        equipItem(party, rules, character, FF6_EQUIP_RIGHT_HAND, best(FF6_EQUIP_RIGHT_HAND, FF6_ITEM_TYPE_WEAPON, false));
        const bool second = stats.genjiGlove;
        equipItem(party, rules, character, FF6_EQUIP_LEFT_HAND, best(FF6_EQUIP_LEFT_HAND, second ? FF6_ITEM_TYPE_WEAPON : FF6_ITEM_TYPE_SHIELD, false));
    }
    equipItem(party, rules, character, FF6_EQUIP_HEAD, best(FF6_EQUIP_HEAD, -1, false));
    equipItem(party, rules, character, FF6_EQUIP_BODY, best(FF6_EQUIP_BODY, -1, false));
}

std::vector<int> shopStock(const GameRules& rules, int shop) {
    std::vector<int> stock;
    if (shop < 0 || shop >= FF6_SHOP_COUNT) return stock;
    const uint8_t* prop = rules.shop(shop);
    for (int i = 1; i <= FF6_SHOP_ITEMS; ++i) {
        if (prop[i] != FF6_ITEM_EMPTY) stock.push_back(prop[i]);
    }
    return stock;
}

int shopPrice(const GameRules& rules, int shop, int item, int leadSprite) {
    if (shop < 0 || shop >= FF6_SHOP_COUNT || item < 0 || item >= FF6_ITEM_COUNT) return 0;
    const uint8_t* prop = rules.item(item);
    const int price = prop[FF6_ITEM_PROP_PRICE] | (prop[FF6_ITEM_PROP_PRICE + 1] << 8);
    static constexpr int kFemale[] = FF6_SHOPPERS_FEMALE;
    bool female = false;
    for (const int sprite : kFemale) female = female || sprite == leadSprite;
    const int plus50 = price + price / 2, half = price / 2;
    switch ((rules.shop(shop)[0] >> FF6_SHOP_PRICE_SHIFT) & FF6_SHOP_PRICE_MASK) {
        case FF6_PRICE_PLUS_50: return plus50;
        case FF6_PRICE_PLUS_100: return price * 2;
        case FF6_PRICE_HALF: return half;
        case FF6_PRICE_HALF_FOR_FEMALE: return female ? half : plus50;
        case FF6_PRICE_HALF_FOR_MALE: return female ? plus50 : half;
        case FF6_PRICE_HALF_FOR_EDGAR: return leadSprite == FF6_SHOPPER_EDGAR ? half : price;
        default: return price;
    }
}

int sellPrice(const GameRules& rules, int item) {
    if (item < 0 || item >= FF6_ITEM_COUNT) return 0;
    const uint8_t* prop = rules.item(item);
    return (prop[FF6_ITEM_PROP_PRICE] | (prop[FF6_ITEM_PROP_PRICE + 1] << 8)) / 2;
}

int itemQuantity(const PartyState& party, int item) {
    for (const InventorySlot& slot : party.inventory) {
        if (slot.item == item) return slot.quantity;
    }
    return 0;
}

bool buyItem(PartyState& party, int item, int quantity, int unitPrice) {
    if (item < 0 || item >= FF6_ITEM_COUNT - 1 || quantity < 1 || unitPrice < 0) return false;
    const long cost = static_cast<long>(unitPrice) * quantity;
    const int held = itemQuantity(party, item);
    if (party.gil < cost || held + quantity > FF6_MAX_ITEM_QUANTITY) return false;
    InventorySlot* slot = findItem(party, item);
    if (!slot) slot = findItem(party, FF6_ITEM_EMPTY);
    if (!slot) return false;
    slot->item = item;
    slot->quantity = held + quantity;
    party.gil -= cost;
    return true;
}

bool sellItem(PartyState& party, int item, int quantity, int unitPrice) {
    InventorySlot* slot = findItem(party, item);
    if (!slot || item == FF6_ITEM_EMPTY || quantity < 1 || slot->quantity < quantity || unitPrice < 0) return false;
    slot->quantity -= quantity;
    if (slot->quantity == 0) *slot = InventorySlot();
    party.gil = std::min(party.gil + static_cast<long>(unitPrice) * quantity, static_cast<long>(FF6_MAX_GIL));
    return true;
}

}  // namespace ff6
