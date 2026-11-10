#include "field/anim.hpp"

#include <algorithm>
#include <cstring>

namespace ff6 {

void MapAnim::init() {
    bgCounters_.fill(0);
    overridden_.fill(false);
    bg3Counter_ = 0;
}

void MapAnim::update(MapData& map, uint32_t frame) {
    if (frame % FF6_ANIM_PERIOD_FRAMES != 0) return;
    if (map.bgAnimEnabled()) {
        for (size_t tile = 0; tile < FF6_BG_ANIM_TILES; ++tile) {
            const BgAnimTile& anim = map.bgAnim[tile];
            bgCounters_[tile] = static_cast<uint16_t>(bgCounters_[tile] + (overridden_[tile] ? speeds_[tile] : anim.speed));
            const size_t index = (bgCounters_[tile] >> FF6_BG_ANIM_FRAME_SHIFT) % FF6_BG_ANIM_FRAMES;
            std::memcpy(&map.vram[FF6_BG_ANIM_VRAM + tile * FF6_BG_ANIM_TILE_BYTES], anim.frames[index].data(),
                        FF6_BG_ANIM_TILE_BYTES);
        }
    }
    if (map.bg3AnimIndex() != 0 && !map.bg3Anim.data.empty()) {
        const Bg3Anim& anim = map.bg3Anim;
        bg3Counter_ = static_cast<uint16_t>(bg3Counter_ + anim.speed);
        const size_t index = ((bg3Counter_ >> FF6_BG3_ANIM_FRAME_SHIFT) & FF6_BG3_ANIM_FRAME_MASK) / 2;
        const size_t offset = anim.frames[index];
        if (offset < anim.data.size()) {
            const size_t size = std::min({static_cast<size_t>(anim.size), anim.data.size() - offset, map.bg3Gfx.size()});
            std::memcpy(map.bg3Gfx.data(), anim.data.data() + offset, size);
        }
    }
}

}  // namespace ff6
