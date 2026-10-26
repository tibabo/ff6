#pragma once

#include "constants.hpp"

namespace ff6 {

// What the player asks for during one game tick, whatever device it comes from.
struct InputState {
    int direction = FF6_NO_DIRECTION;  // Facing value, or FF6_NO_DIRECTION
    bool confirm = false;              // true only on the tick the button goes down
    bool cancel = false;               // the same for the cancel button of the menus
    bool menu = false;                 // and for the button that opens the menu
};

}  // namespace ff6
