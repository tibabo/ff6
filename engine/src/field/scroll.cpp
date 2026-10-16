#include "field/scroll.hpp"

#include <algorithm>

namespace ff6 {
namespace {

int clampRange(int value, int low, int high) { return std::max(low, std::min(high, value)); }

int speedOf(uint8_t value) { return static_cast<int>(static_cast<int8_t>(value)) * (1 << FF6_PARALLAX_SPEED_SHIFT); }

}  // namespace

void Scroll::init() {
    x_.fill(0);
    y_.fill(0);
    autoX_.fill(0);
    autoY_.fill(0);
}

void Scroll::update(const MapData& map, int playerPixelX, int playerPixelY) {
    int cameraX = playerPixelX, cameraY = playerPixelY;
    if (map.limitWidth() != 0) {
        cameraX = clampRange(cameraX, FF6_CAMERA_MIN_X_TILE * FF6_TILE_SIZE,
                             (map.limitWidth() - FF6_CAMERA_MAX_MARGIN) * FF6_TILE_SIZE);
        cameraY = clampRange(cameraY, FF6_CAMERA_MIN_Y_TILE * FF6_TILE_SIZE,
                             (map.limitHeight() - FF6_CAMERA_MAX_MARGIN) * FF6_TILE_SIZE);
    }
    const int party = FF6_CAMERA_PARTY_TILE * FF6_TILE_SIZE;
    x_[FF6_BG1] = cameraX - party;
    y_[FF6_BG1] = cameraY - party;
    // bg2 and bg3: offset + multiplier * camera + automatic speed (see InitScrollPos and CalcScrollPos).
    for (int layer = FF6_BG2; layer <= FF6_BG3; ++layer) {
        const size_t slot = static_cast<size_t>(layer - FF6_BG2);
        autoX_[slot] += speedOf(map.parallax[slot * 2]);
        autoY_[slot] += speedOf(map.parallax[slot * 2 + 1]);
        const int multX = map.parallax[4 + slot * 2];
        const int multY = map.parallax[5 + slot * 2];
        x_[static_cast<size_t>(layer)] = map.layerOffsetX(layer) * FF6_TILE_SIZE +
            multX * (x_[FF6_BG1] + party) / FF6_PARALLAX_ONE - party + autoX_[slot] / FF6_SCROLL_UNITS_PER_PIXEL;
        y_[static_cast<size_t>(layer)] = map.layerOffsetY(layer) * FF6_TILE_SIZE +
            multY * (y_[FF6_BG1] + party) / FF6_PARALLAX_ONE - party + autoY_[slot] / FF6_SCROLL_UNITS_PER_PIXEL;
    }
}

}  // namespace ff6
