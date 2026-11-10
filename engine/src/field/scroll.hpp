#pragma once

#include <array>
#include <cstdint>

#include "field/map.hpp"

namespace ff6 {

// Port of UpdateScroll/CalcScrollPos: where each bg layer is on the screen, in pixels.
class Scroll {
public:
    void init();
    // Call once per game frame with the pixel position of the party.
    void update(const MapData& map, int playerPixelX, int playerPixelY);

    int x(int layer) const { return x_[static_cast<size_t>(layer)]; }
    int y(int layer) const { return y_[static_cast<size_t>(layer)]; }
    // Event commands 0x5D to 0x72: a layer moves by `dx` and `dy` (1/256 pixel) more, for good until the map is loaded again.
    void drift(int layer, int dx, int dy) {
        driftX_[static_cast<size_t>(layer)] += dx;
        driftY_[static_cast<size_t>(layer)] += dy;
    }

private:
    std::array<int, FF6_LAYER_COUNT> x_{}, y_{};
    std::array<int32_t, 2> autoX_{}, autoY_{};  // bg2 and bg3 automatic scrolling, in 1/256 pixel
    std::array<int32_t, FF6_LAYER_COUNT> driftX_{}, driftY_{};
};

}  // namespace ff6
