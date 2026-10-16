#pragma once

#include <cstdint>

#include "field/map.hpp"

namespace ff6 {

// What the PPU does for one bg layer: draws the visible FF6_SCREEN_WIDTH x FF6_SCREEN_HEIGHT part of the layer,
// scrolled by (scrollX, scrollY) pixels and wrapped around its size, into RGBA buffers of that size. Pixels with
// the tile priority bit set go to `high`, the others to `low`; bg3 goes entirely to `high` when it is in front of
// everything. Pixels that are not drawn are left untouched.
void renderBg(const MapData& map, int layer, int scrollX, int scrollY, uint8_t* low, uint8_t* high);

}  // namespace ff6
