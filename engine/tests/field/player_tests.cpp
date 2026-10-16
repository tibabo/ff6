#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/player.hpp"

namespace {

void testCollisionRules() {
    ff6::MapData map = test::makeMap();
    CHECK(ff6::Player::canMove(map, 5, 5, ff6::Right, 1));

    test::setProp(map, 6, 5, 0xFFF7);  // impassable destination
    CHECK(!ff6::Player::canMove(map, 5, 5, ff6::Right, 1));
    test::setProp(map, 6, 5, 0x0F07);  // counter
    CHECK(!ff6::Player::canMove(map, 5, 5, ff6::Right, 1));

    test::setProp(map, 5, 5, 0x0E01);  // right bit cleared on the current tile
    CHECK(!ff6::Player::canMove(map, 5, 5, ff6::Right, 1));
    CHECK(ff6::Player::canMove(map, 5, 5, ff6::Left, 1));
    CHECK(ff6::Player::canMove(map, 5, 5, ff6::Up, 1));

    map = test::makeMap();
    test::setProp(map, 6, 5, 0x0F02);  // upper tile to lower tile
    CHECK(!ff6::Player::canMove(map, 5, 5, ff6::Right, 1));
    test::setProp(map, 6, 5, 0x0F03);  // transition tile accepts both levels
    CHECK(ff6::Player::canMove(map, 5, 5, ff6::Right, 1));

    map = test::makeMap();
    test::setProp(map, 5, 5, 0x0F04);  // bridge
    test::setProp(map, 6, 5, 0x0F02);
    CHECK(!ff6::Player::canMove(map, 5, 5, ff6::Right, 1));
    CHECK(ff6::Player::canMove(map, 5, 5, ff6::Right, 2));
    test::setProp(map, 6, 5, 0x0F01);
    CHECK(!ff6::Player::canMove(map, 5, 5, ff6::Right, 2));
}

void testMovementAndFrames() {
    const ff6::Objects nobody;
    ff6::MapData map = test::makeMap();
    ff6::Player player;
    player.place(map, 4, 4, ff6::Down);
    CHECK(player.frameIndex() == 1);
    CHECK(player.update(map, ff6::Right, nobody));
    CHECK(player.pixelX() == 65 && player.moving());
    int frames = 1;
    while (player.moving()) {
        player.update(map, FF6_NO_DIRECTION, nobody);
        ++frames;
    }
    CHECK(frames == FF6_STEP_FRAMES);
    CHECK(player.pixelX() == 80 && player.tileX() == 5);
    CHECK(player.facing() == ff6::Right && player.frameIndex() == 10);

    player.update(map, ff6::Left, nobody);
    player.update(map, ff6::Left, nobody);
    CHECK(player.frameIndex() == 7 || player.frameIndex() == 8 || player.frameIndex() == 6);

    test::setProp(map, 7, 4, 0xFFF7);
    player.place(map, 6, 4, ff6::Down);
    CHECK(!player.update(map, ff6::Right, nobody));
    CHECK(player.facing() == ff6::Right && !player.moving());

    map = test::makeMap();
    test::setProp(map, 5, 4, 0x0F09);  // top half drawn above the priority-1 background
    player.place(map, 4, 4, ff6::Down);
    CHECK(!player.topAbove());
    player.update(map, ff6::Right, nobody);
    while (player.moving()) player.update(map, FF6_NO_DIRECTION, nobody);
    player.update(map, FF6_NO_DIRECTION, nobody);
    CHECK(player.topAbove() && !player.bottomAbove());
}

}  // namespace

void testPlayer() {
    testCollisionRules();
    testMovementAndFrames();
}
