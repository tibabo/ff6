#pragma once

#include <array>
#include <cstdint>

#include "constants.hpp"

namespace ff6 {

// The settings of the config menu, in the bytes where the game keeps them ($1d4d, $1d4e and $1d54 of the save).
struct GameConfig {
    // bits 0-2: battle speed - 1, bit 3: wait mode, bits 4-6: message speed - 1, bit 7: short command set
    uint8_t battle = FF6_CONFIG_BATTLE_DEFAULT;
    // bits 0-3: window pattern, bit 4: empty on reequip, bit 5: mono, bit 6: cursor memory, bit 7: gauge off
    uint8_t display = 0;
    // bits 0-2: order of the magic, bits 3-5: font color, bit 7: multiple controllers
    uint8_t controller = 0;
    uint16_t fontColor = FF6_CONFIG_FONT_COLOR_DEFAULT;  // BGR555
    // The colors 1 to 7 of each pattern of window; the game fills them with the colors of the ROM.
    std::array<std::array<uint16_t, FF6_MENU_WINDOW_COLORS>, FF6_MENU_WINDOW_COUNT> windowColors{};

    int windowPattern() const { return display & FF6_CONFIG_WINDOW_MASK; }
    int magicOrder() const { return controller & FF6_CONFIG_ORDER_MASK; }
    // 0 when the font color is being edited, else the number of the color of the window.
    int colorSlot() const { return (controller & FF6_CONFIG_COLOR_MASK) >> FF6_CONFIG_COLOR_SHIFT; }
    // The color being edited.
    uint16_t& editedColor() {
        return colorSlot() == 0 ? fontColor : windowColors[static_cast<size_t>(windowPattern())][static_cast<size_t>(colorSlot() - 1)];
    }

    int battleSpeed() const { return battle & FF6_CONFIG_BATTLE_SPEED_MASK; }
    int messageSpeed() const { return (battle & FF6_CONFIG_MESSAGE_SPEED_MASK) >> FF6_CONFIG_MESSAGE_SPEED_SHIFT; }
    bool cursorMemory() const { return (display & FF6_CONFIG_CURSOR_MEMORY) != 0; }
};

}  // namespace ff6
