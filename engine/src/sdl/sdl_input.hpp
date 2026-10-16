#pragma once

#include <SDL.h>

#include <map>

#include "input.hpp"

namespace ff6 {

// Keyboard and game controllers (hot-pluggable), read through SDL2.
class SdlInput {
public:
    SdlInput() = default;
    SdlInput(const SdlInput&) = delete;
    SdlInput& operator=(const SdlInput&) = delete;
    ~SdlInput();

    // Feed every SDL event here so that controllers can be plugged and unplugged.
    void handle(const SDL_Event& event);
    // Call once per game tick: it tracks the confirm button edge.
    InputState sample();

private:
    int direction() const;
    bool confirmPressed();

    std::map<SDL_JoystickID, SDL_GameController*> controllers_;
    bool confirmHeld_ = false;
};

}  // namespace ff6
