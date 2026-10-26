#pragma once

#include "input.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// A character as the menus show it. Until the data of the characters is ported, the statistics are placeholders.
struct MenuCharacter {
    int id = 0;  // the character, 0 for Terra
    int level = 0, hp = 0, maxHp = 0, mp = 0, maxMp = 0;
    int vigor = 0, speed = 0, stamina = 0, magicPower = 0, attackPower = 0, defense = 0;
    int evade = 0, magicDefense = 0, magicEvade = 0;
    long experience = 0, nextLevel = 0;  // what the character has, and what it needs for the next level
    int equipment[FF6_MENU_EQUIP_SLOTS] = {FF6_ITEM_EMPTY, FF6_ITEM_EMPTY, FF6_ITEM_EMPTY,
                                           FF6_ITEM_EMPTY, FF6_ITEM_EMPTY, FF6_ITEM_EMPTY};  // right hand to second relic
};

enum class CharacterScreen { Status, Equip, Relic, Skills };

// The screens of one character (src/menu/status.asm, equip.asm and skills.asm), which are only shown: the equipment
// cannot be changed yet, and the lists of spells and other skills are not ported.
class CharacterMenu {
public:
    CharacterMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    void open(CharacterScreen which, const MenuCharacter& character);
    void update(const InputState& input);
    void draw();
    // True once the player has asked to go back.
    bool closed() const { return closed_; }
    CharacterScreen which() const { return which_; }
    int cursor() const { return cursor_; }

private:
    int rows() const;
    void drawName(int x, int row);
    void drawStatus();
    void drawEquip();
    void drawSkills();

    MenuScreen& screen_;
    Sound& sound_;
    CharacterScreen which_ = CharacterScreen::Status;
    MenuCharacter character_;
    int cursor_ = 0;
    bool closed_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
