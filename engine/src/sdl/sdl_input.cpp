#include "sdl/sdl_input.hpp"

#include <cstdio>

#include "field/map.hpp"

#define FF6_STICK_DEAD_ZONE 16000  // SDL axis values range from -32768 to 32767

namespace ff6 {

SdlInput::~SdlInput() {
    for (auto& entry : controllers_) SDL_GameControllerClose(entry.second);
}

void SdlInput::handle(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        if (SDL_GameController* pad = SDL_GameControllerOpen(event.cdevice.which)) {
            controllers_[SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))] = pad;
            std::printf("Controller connected: %s\n", SDL_GameControllerName(pad));
        }
    } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
        auto it = controllers_.find(event.cdevice.which);
        if (it != controllers_.end()) {
            SDL_GameControllerClose(it->second);
            controllers_.erase(it);
            std::printf("Controller disconnected\n");
        }
    }
}

InputState SdlInput::sample() {
    InputState state;
    const auto edge = [](bool held, bool& before) {
        const bool pressed = held && !before;
        before = held;
        return pressed;
    };
    state.confirm = edge(buttonHeld({SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE, SDL_SCANCODE_KP_ENTER}, SDL_CONTROLLER_BUTTON_A),
                         confirmHeld_);
    state.cancel = edge(buttonHeld({SDL_SCANCODE_Z, SDL_SCANCODE_BACKSPACE}, SDL_CONTROLLER_BUTTON_B), cancelHeld_);
    state.menu = edge(buttonHeld({SDL_SCANCODE_X}, SDL_CONTROLLER_BUTTON_X), menuHeld_);
    state.start = edge(buttonHeld({SDL_SCANCODE_TAB}, SDL_CONTROLLER_BUTTON_START), startHeld_);
    state.direction = direction();
    state.held = heldButtons();
    return state;
}

// The field code tests right, left, up, then down.
int SdlInput::direction() const {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    bool pressed[FF6_DIRECTION_COUNT] = {keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W],
                                         keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D],
                                         keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S],
                                         keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]};
    for (const auto& entry : controllers_) {
        SDL_GameController* pad = entry.second;
        const Sint16 x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        const Sint16 y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        pressed[Up] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) || y < -FF6_STICK_DEAD_ZONE;
        pressed[Right] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > FF6_STICK_DEAD_ZONE;
        pressed[Down] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y > FF6_STICK_DEAD_ZONE;
        pressed[Left] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -FF6_STICK_DEAD_ZONE;
    }
    for (int direction : {Right, Left, Up, Down}) {
        if (pressed[direction]) return direction;
    }
    return FF6_NO_DIRECTION;
}

// Every button that is down, for the airship, which turns and climbs at the same time.
unsigned SdlInput::heldButtons() const {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    unsigned held = 0;
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) held |= FF6_HELD_UP;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) held |= FF6_HELD_RIGHT;
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) held |= FF6_HELD_DOWN;
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) held |= FF6_HELD_LEFT;
    if (buttonHeld({SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE, SDL_SCANCODE_KP_ENTER}, SDL_CONTROLLER_BUTTON_A)) held |= FF6_HELD_A;
    if (buttonHeld({SDL_SCANCODE_Q}, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) held |= FF6_HELD_L;
    if (buttonHeld({SDL_SCANCODE_E}, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) held |= FF6_HELD_R;
    if (buttonHeld({SDL_SCANCODE_C}, SDL_CONTROLLER_BUTTON_Y)) held |= FF6_HELD_Y;
    for (const auto& entry : controllers_) {
        SDL_GameController* pad = entry.second;
        const Sint16 x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        const Sint16 y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) || y < -FF6_STICK_DEAD_ZONE) held |= FF6_HELD_UP;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > FF6_STICK_DEAD_ZONE) held |= FF6_HELD_RIGHT;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y > FF6_STICK_DEAD_ZONE) held |= FF6_HELD_DOWN;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -FF6_STICK_DEAD_ZONE) held |= FF6_HELD_LEFT;
    }
    return held;
}

bool SdlInput::buttonHeld(const std::initializer_list<SDL_Scancode>& keys, SDL_GameControllerButton button) const {
    const Uint8* state = SDL_GetKeyboardState(nullptr);
    bool held = false;
    for (const SDL_Scancode key : keys) held |= state[key] != 0;
    for (const auto& entry : controllers_) held |= SDL_GameControllerGetButton(entry.second, button) != 0;
    return held;
}

}  // namespace ff6
