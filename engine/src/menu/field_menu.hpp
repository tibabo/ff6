#pragma once

#include <vector>

#include "input.hpp"
#include "menu/config_menu.hpp"
#include "menu/item_menu.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// A character as the menu shows it.
struct MenuCharacter {
    int id = 0;  // the character, 0 for Terra
    int level = 0, hp = 0, maxHp = 0, mp = 0, maxMp = 0;
};

// What the main menu shows of the game.
struct MenuStatus {
    std::vector<MenuCharacter> party;  // up to four, in the order of their places
    std::vector<MenuItem> items;  // the inventory, without its empty places
    long gil = 0, steps = 0;
    int hours = 0, minutes = 0;  // the time that the game has run
};

// The menu of the field (src/menu/field_menu.asm): the party, and the list of Item, Skills, Equip, Relic, Status,
// Config and Save, with the hand cursor.
class FieldMenu {
public:
    FieldMenu(MenuScreen& screen, Sound& sound)
        : screen_(screen), sound_(sound), configMenu_(screen, sound), itemMenu_(screen, sound) {}

    // Opens the menu; the cursor is where the player left it if the cursor memory of the config is on. The settings
    // must stay valid while the menu is open.
    void open(const MenuStatus& status, GameConfig& config);
    void update(const InputState& input);
    void draw();
    // True once the player has asked to leave the menu.
    bool closed() const { return closed_; }
    int cursor() const { return cursor_; }
    bool inConfig() const { return inConfig_; }
    const ConfigMenu& configMenu() const { return configMenu_; }
    bool inItems() const { return inItems_; }
    const ItemMenu& itemMenu() const { return itemMenu_; }

private:
    void moveCursor(int step);
    void drawCharacter(const MenuCharacter& character, int slot);

    MenuScreen& screen_;
    Sound& sound_;
    MenuStatus status_;
    ConfigMenu configMenu_;
    ItemMenu itemMenu_;
    GameConfig* config_ = nullptr;
    bool inConfig_ = false;  // the config menu is on the screen
    bool inItems_ = false;  // the item menu is
    int cursor_ = 0;
    bool closed_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
