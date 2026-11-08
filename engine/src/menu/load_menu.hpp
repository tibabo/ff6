#pragma once

#include "input.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// The menu that follows the title screen: a new game, or a saved one. The game has its own menu to pick a file, which
// reads the memory of the cartridge; here the save is one file, so there are two choices.
class LoadMenu {
public:
    LoadMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    // `canContinue`: there is a saved game to go on with.
    void open(bool canContinue);
    void update(const InputState& input);
    void draw();
    bool closed() const { return closed_; }
    bool continueChosen() const { return option_ == 1; }

private:
    MenuScreen& screen_;
    Sound& sound_;
    bool canContinue_ = false;
    bool closed_ = false;
    int option_ = 0;
    DirectionRepeat repeat_;
};

}  // namespace ff6
