#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/anim.hpp"

void testAnim() {
    ff6::MapData map = test::makeMap();
    map.prop[FF6_MAP_PROP_ANIMATION] = 1;  // tile animation on, no bg3 animation
    for (size_t frame = 0; frame < FF6_BG_ANIM_FRAMES; ++frame) map.bgAnim[0].frames[frame].fill(static_cast<uint8_t>(0x10 + frame));
    map.bgAnim[0].speed = 0x200;  // one frame per update
    ff6::MapAnim anim;
    anim.init();
    anim.update(map, 1);  // animations run at half the frame rate
    CHECK(map.vram[FF6_BG_ANIM_VRAM] == 0);
    anim.update(map, 2);
    CHECK(map.vram[FF6_BG_ANIM_VRAM] == 0x11 && map.vram[FF6_BG_ANIM_VRAM + FF6_BG_ANIM_TILE_BYTES - 1] == 0x11);
    anim.update(map, 4);
    CHECK(map.vram[FF6_BG_ANIM_VRAM] == 0x12);
    CHECK(map.vram[FF6_BG_ANIM_VRAM + FF6_BG_ANIM_TILE_BYTES] == map.bgAnim[1].frames[2][0]);  // next block, other speed
    anim.update(map, 6);
    anim.update(map, 8);  // the frame number wraps after four frames
    CHECK(map.vram[FF6_BG_ANIM_VRAM] == 0x10);

    map.prop[FF6_MAP_PROP_ANIMATION] = 0;
    anim.init();
    map.vram[FF6_BG_ANIM_VRAM] = 0x55;
    anim.update(map, 2);
    CHECK(map.vram[FF6_BG_ANIM_VRAM] == 0x55);  // no animation, nothing changes

    // bg3: the frame table gives offsets into the animation graphics, and `size` bytes are copied.
    ff6::MapData bg3 = test::makeMap();
    bg3.prop[FF6_MAP_PROP_ANIMATION] = 1 << FF6_BG3_ANIM_SHIFT;
    bg3.bg3Anim.data = {0, 0, 0, 0, 9, 8, 7, 6, 5};
    bg3.bg3Anim.speed = 0x200;  // (counter >> 8) & 0x0E is 2 after one update: the second frame
    bg3.bg3Anim.size = 4;
    bg3.bg3Anim.frames = {0, 4, 1};
    anim.init();
    anim.update(bg3, 2);
    CHECK(bg3.bg3Gfx[0] == 9 && bg3.bg3Gfx[3] == 6 && bg3.bg3Gfx[4] == 0);
}
