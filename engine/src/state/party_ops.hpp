#pragma once

#include "data/game_rules.hpp"
#include "state/party_state.hpp"

namespace ff6 {

// The characters that have joined, a bit for each (the event bits FF6_EVENT_BIT_CHAR_AVAILABLE and the next ones).
using CharacterMask = unsigned;

// CalcAverageLevel: the average level of the available characters, the default level when there are none.
int averageLevel(const PartyState& party, CharacterMask available);

// What a level adds to the experience: UpdateExp.
void updateExperience(const GameRules& rules, CharacterState& character);

// EventCmd_40 (without the object): the character becomes the actor, at the average level and with full hp and mp.
void setCharacterProperties(PartyState& party, const GameRules& rules, int slot, int actor, CharacterMask available);
// EventCmd_77: a character below the average level gets to it, with full hp and mp.
void normalizeLevel(PartyState& party, const GameRules& rules, int slot, CharacterMask available);

// The magic that Terra and Celes know at their level, and the swordtechs and blitzes that Cyan and Sabin have learned
// (UpdateAbilities).
void updateAbilities(PartyState& party, const GameRules& rules, int slot);
// EventCmd_86 and EventCmd_87: espers 0x36 to 0x50.
void giveEsper(PartyState& party, int id);
void takeEsper(PartyState& party, int id);

// GiveItem and EventCmd_81: one item more or less; a full inventory or a quantity of 99 loses the item.
void giveItem(PartyState& party, int item);
void takeItem(PartyState& party, int item);
void giveGil(PartyState& party, long amount);
// False, and nothing taken, when there is not enough gil.
bool takeGil(PartyState& party, long amount);
// EventCmd_8b and EventCmd_8c: `code` is the second byte of the command.
void changeHp(PartyState& party, const GameRules& rules, int slot, int code);
void changeMp(PartyState& party, const GameRules& rules, int slot, int code);
// EventCmd_8d: the equipment goes back to the inventory.
void removeEquipment(PartyState& party, int slot);
// The items of the inventory that the character can put in a slot of its equipment (GetValidEquip and
// CheckCanEquipItem), in the order of the inventory.
std::vector<int> equippableItems(const PartyState& party, const GameRules& rules, int character, int slot);
// Puts an item of the inventory in a slot, or nothing with FF6_ITEM_EMPTY, and the old item goes back to the inventory.
// False, and nothing changed, when the item is not there or the old one would have no place.
bool equipItem(PartyState& party, const GameRules& rules, int character, int slot, int item);

// Items that the menu can use: the ones that restore hp or mp, and the fenix down. The rest needs the status effects of
// the characters, a save point or a battle.
bool itemUsableInMenu(const GameRules& rules, int item);
// CheckCanUseItem: it has something to restore on this character (a character at 0 hp is dead: only a fenix down
// helps it).
bool canUseItemOn(const PartyState& party, const GameRules& rules, int character, int item);
// Restores what the item restores, and takes one from the inventory. False when it cannot be used.
bool useItemOn(PartyState& party, const GameRules& rules, int character, int item);

// What the character needs to be at level `level`.
long experienceForLevel(const GameRules& rules, int level);
// The experience of a battle: the character goes up as many levels as it earns, with the hp and mp of each.
void addExperience(PartyState& party, const GameRules& rules, int character, long amount);

// The items that a shop sells, in order.
std::vector<int> shopStock(const GameRules& rules, int shop);
// What an item costs in a shop (AdjustShopPrice): some shops change it with the sprite of the character in front.
int shopPrice(const GameRules& rules, int shop, int item, int leadSprite);
// What the shops pay for an item: half of its price.
int sellPrice(const GameRules& rules, int item);
// How many of an item the inventory holds.
int itemQuantity(const PartyState& party, int item);
// Pays and adds the items; false, and nothing done, when there is not enough gil, too many items or no place for them.
bool buyItem(PartyState& party, int item, int quantity, int unitPrice);
// Takes the items and pays for them; false when the party does not have that many.
bool sellItem(PartyState& party, int item, int quantity, int unitPrice);
void countStep(PartyState& party);

}  // namespace ff6
