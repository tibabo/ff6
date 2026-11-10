#pragma once

#include <functional>
#include <vector>

#include "input.hpp"
#include "menu/character_menu.hpp"
#include "menu/item_menu.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// The bout that an item brings: the monster that it faces, the prize, and whether Shadow comes instead.
struct ColosseumMatch {
    int monster = 0;
    int prize = FF6_ITEM_EMPTY;
    bool prizeHidden = false;
    bool shadow = false;
};

// What the colosseum asks of the game.
struct ColosseumHooks {
    std::function<std::vector<MenuItem>()> items;  // the inventory
    std::function<std::vector<MenuCharacter>()> party;
    std::function<ColosseumMatch(int item)> match;
    std::function<std::vector<uint8_t>(int monster)> monsterName;
};

enum class ColosseumMode { Items, Challenger };

// The menu of the colosseum (src/menu/colosseum.asm): the item to wager, then the character that fights. The positions of the
// texts are close to the game's, not exact; the preview of the monster is a name.
class ColosseumMenu {
public:
    ColosseumMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    void open(const ColosseumHooks& hooks);
    void update(const InputState& input);
    void draw();
    bool closed() const { return closed_; }
    // Whether a bout was chosen when the menu closed, and with what.
    bool chosen() const { return chosen_; }
    int item() const { return item_; }
    int character() const { return character_; }
    const ColosseumMatch& match() const { return match_; }
    ColosseumMode mode() const { return mode_; }
    int selected() const { return selected_; }

private:
    void drawItems();
    void drawChallenger();

    MenuScreen& screen_;
    Sound& sound_;
    ColosseumHooks hooks_;
    ColosseumMode mode_ = ColosseumMode::Items;
    std::vector<MenuItem> items_;
    std::vector<MenuCharacter> party_;
    ColosseumMatch match_;
    int selected_ = 0, first_ = 0, target_ = 0;
    int item_ = FF6_ITEM_EMPTY, character_ = -1;
    bool closed_ = false, chosen_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
