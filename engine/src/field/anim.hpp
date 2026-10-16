#pragma once

#include <array>
#include <cstdint>

#include "field/map.hpp"

namespace ff6 {

// Port of UpdateBGAnim: animated tile graphics of bg1/bg2 and bg3, advanced at half the frame rate.
class MapAnim {
public:
    void init();
    // Call once per game frame; it changes the graphics held by the map.
    void update(MapData& map, uint32_t frame);

private:
    std::array<uint16_t, FF6_BG_ANIM_TILES> bgCounters_{};
    uint16_t bg3Counter_ = 0;
};

}  // namespace ff6
