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
    // Event commands 0x64 and 0x65: the frame an animated tile is at, and its speed in place of the map's.
    void setFrame(size_t tile, int frame) { if (tile < bgCounters_.size()) bgCounters_[tile] = static_cast<uint16_t>((frame & 3) << FF6_BG_ANIM_FRAME_SHIFT); }
    void setSpeed(size_t tile, int speed) {
        if (tile < speeds_.size()) {
            speeds_[tile] = static_cast<uint16_t>(speed);
            overridden_[tile] = true;
        }
    }

private:
    std::array<uint16_t, FF6_BG_ANIM_TILES> bgCounters_{}, speeds_{};
    std::array<bool, FF6_BG_ANIM_TILES> overridden_{};
    uint16_t bg3Counter_ = 0;
};

}  // namespace ff6
