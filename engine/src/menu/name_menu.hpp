#pragma once

#include <string>

#include "input.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// The menu that names a character (src/menu/name_change.asm): a grid of letters, up to six of them in the name, A adds the
// letter under the hand, B takes the last one away and Start is done. The positions are close to the game's, not exact.
class NameMenu {
public:
    static constexpr int kMaxLength = 6;
    NameMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    void open(int character, const std::string& current);
    void update(const InputState& input);
    void draw();
    bool closed() const { return closed_; }
    int character() const { return character_; }
    const std::string& name() const { return name_; }

private:
    MenuScreen& screen_;
    Sound& sound_;
    int character_ = 0, row_ = 0, column_ = 0;
    std::string name_;
    bool closed_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
