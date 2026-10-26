#pragma once

#include <cstdint>
#include <vector>

#include "menu/menu_config.hpp"
#include "menu/menu_data.hpp"

namespace ff6 {

// The RGBA tiles of the windows, in the pattern and the colors that the config has chosen (LoadWindowGfx).
std::vector<uint8_t> buildWindowTiles(const MenuData& menu, const GameConfig& config);

}  // namespace ff6
