#pragma once

#include "constants.hpp"

namespace ff6 {

// The direction of a menu repeats while it is held (UpdateCtrlMenu): at once when it is pressed, then after a delay
// at a fixed rate.
class DirectionRepeat {
public:
    void reset() { held_ = FF6_NO_DIRECTION; }

    // The direction to act on this frame, or FF6_NO_DIRECTION.
    int update(int direction) {
        if (direction != held_) {
            held_ = direction;
            delay_ = FF6_MENU_REPEAT_DELAY;
            rate_ = FF6_MENU_REPEAT_RATE;
            return direction;
        }
        if (direction == FF6_NO_DIRECTION || (delay_ != 0 && --delay_ != 0)) return FF6_NO_DIRECTION;
        if (--rate_ != 0) return FF6_NO_DIRECTION;
        rate_ = FF6_MENU_REPEAT_RATE;
        return direction;
    }

private:
    int held_ = FF6_NO_DIRECTION;
    int delay_ = 0, rate_ = 0;
};

}  // namespace ff6
