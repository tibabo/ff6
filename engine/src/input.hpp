#pragma once

#include "constants.hpp"

namespace ff6 {

// What the player asks for during one game tick, whatever device it comes from.
struct InputState {
    int direction = FF6_NO_DIRECTION;  // Facing value, or FF6_NO_DIRECTION
    bool confirm = false;              // true only on the tick the button goes down
};

}  // namespace ff6
