#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/entrance.hpp"

void testEntrance() {
    ff6::MapData map = test::makeMap();
    // Short entrance at (5,6) to map 0x14 facing down, arriving at (7,8).
    map.shortEntrances = {5, 6, 0x14, 0x20, 7, 8};
    // Long entrances: horizontal at (2,3) over 5 tiles to map 5 (sets the parent map), vertical at (9,4) over 3 tiles
    // back to the parent map.
    map.longEntrances = {2, 3, 4, 0x05, 0x06, 1, 1, 9, 4, 0x82, 0xFF, 0x01, 0, 0};

    ff6::EntranceTarget target;
    CHECK(ff6::checkEntrances(map, 5, 6, &target));
    CHECK(target.map == 0x14 && target.x == 7 && target.y == 8 && target.facing == ff6::Down);
    CHECK(!target.setParent && !target.toParent);
    CHECK(!ff6::checkEntrances(map, 6, 6, &target));
    CHECK(!ff6::checkEntrances(map, 5, 7, &target));

    for (int x = 2; x <= 6; ++x) CHECK(ff6::checkEntrances(map, x, 3, &target));
    CHECK(!ff6::checkEntrances(map, 1, 3, &target) && !ff6::checkEntrances(map, 7, 3, &target));
    CHECK(!ff6::checkEntrances(map, 4, 4, &target));
    CHECK(ff6::checkEntrances(map, 4, 3, &target));
    CHECK(target.map == 5 && target.setParent && !target.toParent && target.x == 1 && target.y == 1);

    for (int y = 4; y <= 6; ++y) CHECK(ff6::checkEntrances(map, 9, y, &target));
    CHECK(!ff6::checkEntrances(map, 9, 7, &target) && !ff6::checkEntrances(map, 9, 3, &target));
    CHECK(ff6::checkEntrances(map, 9, 5, &target) && target.toParent);
}
