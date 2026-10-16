#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/scroll.hpp"

void testScroll() {
    ff6::MapData map = test::makeMap();
    map.parallax = {0, 0, 0, 0, 16, 16, 16, 16};  // every layer follows the camera
    ff6::Scroll scroll;
    scroll.init();
    scroll.update(map, 200, 300);
    CHECK(scroll.x(FF6_BG1) == 200 - 7 * FF6_TILE_SIZE && scroll.y(FF6_BG1) == 300 - 7 * FF6_TILE_SIZE);
    CHECK(scroll.x(FF6_BG2) == scroll.x(FF6_BG1) && scroll.y(FF6_BG3) == scroll.y(FF6_BG1));

    // A multiplier of 8 makes a layer move at half the speed of the camera.
    map.parallax[4] = 8;
    scroll.update(map, 200, 300);
    CHECK(scroll.x(FF6_BG2) == 8 * 200 / 16 - 7 * FF6_TILE_SIZE);
    scroll.update(map, 232, 300);
    CHECK(scroll.x(FF6_BG2) == 8 * 232 / 16 - 7 * FF6_TILE_SIZE);

    // The offset of the layer is in tiles; automatic speeds are in 1/256 pixel per frame (value * 16).
    map.parallax = {0, 0, 8, 0xF8, 16, 16, 16, 16};  // 0xF8 is -8
    map.prop[FF6_MAP_PROP_BG2_OFFSET_X] = 2;
    scroll.init();
    scroll.update(map, 200, 300);
    CHECK(scroll.x(FF6_BG2) == 2 * FF6_TILE_SIZE + scroll.x(FF6_BG1));
    scroll.update(map, 200, 300);  // bg3 has moved 2 * 128 / 256 = 1 pixel right and (negative) 1 up
    CHECK(scroll.x(FF6_BG3) == scroll.x(FF6_BG1) + 1);
    CHECK(scroll.y(FF6_BG3) == scroll.y(FF6_BG1) - 1);

    // Maps with a playable size keep the camera inside it.
    map.parallax = {0, 0, 0, 0, 16, 16, 16, 16};
    map.prop[FF6_MAP_PROP_LIMIT_WIDTH] = 20;
    map.prop[FF6_MAP_PROP_LIMIT_HEIGHT] = 24;
    scroll.init();
    scroll.update(map, 0, 0);
    CHECK(scroll.x(FF6_BG1) == (FF6_CAMERA_MIN_X_TILE - FF6_CAMERA_PARTY_TILE) * FF6_TILE_SIZE && scroll.y(FF6_BG1) == 0);
    scroll.update(map, 5000, 5000);
    CHECK(scroll.x(FF6_BG1) == (20 - FF6_CAMERA_MAX_MARGIN - FF6_CAMERA_PARTY_TILE) * FF6_TILE_SIZE);
    CHECK(scroll.y(FF6_BG1) == (24 - FF6_CAMERA_MAX_MARGIN - FF6_CAMERA_PARTY_TILE) * FF6_TILE_SIZE);
}
