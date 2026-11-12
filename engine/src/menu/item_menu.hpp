#pragma once

#include <functional>
#include <vector>

#include "input.hpp"
#include "menu/character_menu.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// A place of the inventory.
struct MenuItem {
    int id = 0;  // the item, from 0 to FF6_ITEM_COUNT - 1
    int quantity = 0;
};

// What the item menu asks of the game.
struct ItemHooks {
    std::function<bool(int item)> usable;  // the menu can use this kind of item
    std::function<bool(int item, int character)> canUse;  // and it has something to do on this character
    std::function<bool(int item, int character)> use;  // does it and takes the item from the inventory
    std::function<std::vector<MenuItem>()> items;  // the inventory as it is now
    std::function<std::vector<MenuCharacter>()> party;  // the party as it is now
    std::function<bool(int item)> leave;  // the tent and the warp stone: does it if it can be used here, and takes the item
    std::function<void(int first, int second)> swap;  // the items of two places of the list change places
    std::function<void()> arrange;  // puts the inventory in the order of the symbols of the items
    std::function<std::vector<int>()> rareItems;  // the rare items that the party has, by number
};

// The item menu (src/menu/item.asm): the options Use, Arrange and Rare at the top, the description of the item that the
// hand is on, and the list of the inventory. Use asks for a character. Arrange and Rare are not ported.
class ItemMenu {
public:
    ItemMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    // `party` and `hooks` (which must stay valid while the menu is open) are what Use needs.
    void open(const std::vector<MenuItem>& items, const std::vector<MenuCharacter>& party = {}, const ItemHooks* hooks = nullptr);
    void update(const InputState& input);
    void draw();
    // True once the player has asked to go back.
    bool closed() const { return closed_; }
    bool inList() const { return inList_; }
    // The hand is on the characters, to use the item that was chosen.
    bool targeting() const { return targeting_; }
    int target() const { return target_; }
    const std::vector<MenuCharacter>& party() const { return party_; }
    const std::vector<MenuItem>& items() const { return items_; }
    // The place of the inventory where the cursor is.
    int selected() const { return selected_; }
    int firstRow() const { return first_; }
    int option() const { return option_; }
    // The hand is on the rare items (two columns, by the number of the place), or the first pick of a move is made.
    bool inRare() const { return inRare_; }
    int rareCursor() const { return rareCursor_; }
    int picked() const { return picked_; }
    // The tent or the warp stone was used: the menu is over, and the game goes on with the item that left it.
    int leaveItem() const { return leaveItem_; }

private:
    void move(int step);
    void updateTarget(int direction, const InputState& input);
    void drawTarget();
    void drawDescription();
    void drawText(const std::vector<uint8_t>& text);
    void drawRare();
    void updateRare(int direction, const InputState& input);
    void drawCount(int count);
    void refresh();

    MenuScreen& screen_;
    Sound& sound_;
    std::vector<MenuItem> items_;
    std::vector<MenuCharacter> party_;
    const ItemHooks* hooks_ = nullptr;
    bool closed_ = false;
    bool inList_ = false;  // the hand is on the list, not on the options
    bool targeting_ = false;
    bool inRare_ = false;
    int rareCursor_ = 0, picked_ = -1;
    int target_ = 0;  // the place of the character in the party
    int option_ = 0;
    int leaveItem_ = -1;
    int selected_ = 0;  // place in the inventory
    int first_ = 0;  // place of the first row of the list
    DirectionRepeat repeat_;
};

}  // namespace ff6
