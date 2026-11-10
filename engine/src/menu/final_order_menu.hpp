#pragma once

#include <vector>

#include "input.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// The menu that orders the characters for the end of the game (src/menu/final_order.asm): the ones that are in the parties
// on the left, the order that the player chooses on the right, Reset to start over and End (or Start) to finish, which
// puts the ones that were not chosen after the others. The positions are close to the game's, not exact.
class FinalOrderMenu {
public:
    static constexpr int kSlots = 12;
    FinalOrderMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    void open(const std::vector<int>& characters);
    void update(const InputState& input);
    void draw();
    bool closed() const { return closed_; }
    // The characters in the order chosen.
    const std::vector<int>& order() const { return order_; }
    int cursor() const { return cursor_; }

private:
    void finish();

    MenuScreen& screen_;
    Sound& sound_;
    std::vector<int> available_, order_;
    std::vector<bool> used_;
    int cursor_ = 1;  // 0 is Reset, 1 to 12 the characters, 13 is End
    bool closed_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
