#pragma once

#include <array>
#include <functional>
#include <vector>

#include "input.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// The names of what a character can do in one category, in the codes of the small font.
using SkillList = std::vector<std::vector<uint8_t>>;

// A character as the menus show it.
struct MenuCharacter {
    int id = 0;  // the character, 0 for Terra
    int level = 0, hp = 0, maxHp = 0, mp = 0, maxMp = 0;
    int vigor = 0, speed = 0, stamina = 0, magicPower = 0, attackPower = 0, defense = 0;
    int evade = 0, magicDefense = 0, magicEvade = 0;
    long experience = 0, nextLevel = 0;  // what the character has, and what it needs for the next level
    int equipment[FF6_MENU_EQUIP_SLOTS] = {FF6_ITEM_EMPTY, FF6_ITEM_EMPTY, FF6_ITEM_EMPTY,
                                           FF6_ITEM_EMPTY, FF6_ITEM_EMPTY, FF6_ITEM_EMPTY};  // right hand to second relic
    std::array<SkillList, FF6_MENU_SKILL_LISTS> skills;  // espers, magic, swordtechs, blitzes, lores, rages and dances
    std::vector<int> espers;  // the number of each esper of skills[0]
    int equippedEsper = -1;  // the esper that the character carries
};

enum class CharacterScreen { Status, Equip, Relic, Skills };

// What the Equip and Relic screens ask of the game.
struct EquipHooks {
    std::function<std::vector<int>(int character, int slot)> choices;  // the items of the inventory that fit the slot
    std::function<MenuCharacter(int character, int slot, int item)> preview;  // the character with the item on
    std::function<MenuCharacter(int character, int slot, int item)> equip;  // does it, and gives the character back
    std::function<MenuCharacter(int character, int esper)> equipEsper;  // carries the esper (or lets it go), and gives the character back
};

// The screens of one character (src/menu/status.asm, equip.asm and skills.asm): the equipment can be changed, and the
// names of the spells, espers, swordtechs and blitzes can be browsed. Nothing can be cast from them yet.
class CharacterMenu {
public:
    CharacterMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    void open(CharacterScreen which, const MenuCharacter& character, const EquipHooks* hooks = nullptr);
    void update(const InputState& input);
    void draw();
    // True once the player has asked to go back.
    bool closed() const { return closed_; }
    CharacterScreen which() const { return which_; }
    int cursor() const { return cursor_; }
    // The character as the screen shows it, with the equipment that was changed.
    const MenuCharacter& character() const { return character_; }
    // The hand is on a list of items that fit the slot: the first choice takes the item off.
    bool picking() const { return picking_; }
    const std::vector<int>& choices() const { return choices_; }
    int pick() const { return pickCursor_; }
    // The hand is on the names of one category of the Skills screen.
    bool listing() const { return listing_; }
    int listCursor() const { return listCursor_; }

private:
    int rows() const;
    int slot() const;
    void startPick();
    void updatePick(int direction, const InputState& input);
    void updatePreview();
    void drawName(int x, int row);
    void drawStatus();
    void drawEquip();
    void drawSkills();

    MenuScreen& screen_;
    Sound& sound_;
    CharacterScreen which_ = CharacterScreen::Status;
    MenuCharacter character_;
    const EquipHooks* hooks_ = nullptr;
    MenuCharacter preview_;  // the character with the item that the hand is on
    std::vector<int> choices_;
    int pickCursor_ = 0, pickScroll_ = 0;
    bool picking_ = false;
    bool listing_ = false;
    int listCursor_ = 0, listScroll_ = 0;
    int cursor_ = 0;
    bool closed_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
