#pragma once

#include <vector>

#include "input.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// A place of the inventory.
struct MenuItem {
    int id = 0;  // the item, from 0 to FF6_ITEM_COUNT - 1
    int quantity = 0;
};

// The item menu (src/menu/item.asm): the options Use, Arrange and Rare at the top, and the list of the inventory with
// the hand cursor. Only the browsing is ported: the items cannot be used yet.
class ItemMenu {
public:
    ItemMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    void open(const std::vector<MenuItem>& items);
    void update(const InputState& input);
    void draw();
    // True once the player has asked to go back.
    bool closed() const { return closed_; }
    bool inList() const { return inList_; }
    // The place of the inventory where the cursor is.
    int selected() const { return selected_; }
    int firstRow() const { return first_; }
    int option() const { return option_; }

private:
    void move(int step);

    MenuScreen& screen_;
    Sound& sound_;
    std::vector<MenuItem> items_;
    bool closed_ = false;
    bool inList_ = false;  // the hand is on the list, not on the options
    int option_ = 0;
    int selected_ = 0;  // place in the inventory
    int first_ = 0;  // place of the first row of the list
    DirectionRepeat repeat_;
};

}  // namespace ff6
