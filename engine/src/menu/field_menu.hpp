#pragma once

#include <vector>

#include "input.hpp"
#include "menu/character_menu.hpp"
#include "menu/config_menu.hpp"
#include "menu/item_menu.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

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
        : screen_(screen), sound_(sound), configMenu_(screen, sound), itemMenu_(screen, sound),
          characterMenu_(screen, sound) {}

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
    // The hand is on the characters, to choose the one of Skills, Equip, Relic or Status; or the screen of one is open.
    bool choosingCharacter() const { return choosing_; }
    bool inCharacter() const { return inCharacter_; }
    int selectedCharacter() const { return selected_; }
    const CharacterMenu& characterMenu() const { return characterMenu_; }
    // The save option was confirmed; true once, until the next time.
    bool takeSaveRequest() {
        const bool requested = saveRequested_;
        saveRequested_ = false;
        return requested;
    }
    bool confirmingSave() const { return confirmingSave_; }

private:
    void moveCursor(int step);
    void drawMain(bool handOnOptions, bool showOptions);
    void drawCharacter(const MenuCharacter& character, int slot);

    MenuScreen& screen_;
    Sound& sound_;
    MenuStatus status_;
    ConfigMenu configMenu_;
    ItemMenu itemMenu_;
    CharacterMenu characterMenu_;
    GameConfig* config_ = nullptr;
    bool inConfig_ = false;  // the config menu is on the screen
    bool inItems_ = false;  // the item menu is
    bool choosing_ = false;  // the hand is on the characters
    bool inCharacter_ = false;  // the screen of the character that was chosen is open
    int selected_ = 0;  // the slot of the character
    bool confirmingSave_ = false;  // the question of the save is asked
    bool saveYes_ = true;  // the answer where the hand is
    bool saveRequested_ = false;
    int cursor_ = 0;
    bool closed_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
