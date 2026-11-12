#include <algorithm>
#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/map.hpp"
#include "field/sprite_data.hpp"

void testSpriteData() {
    CHECK(ff6::stopFrame(ff6::Down) == 1 && ff6::stopFrame(ff6::Right) == 10);
    CHECK(ff6::moveFrame(ff6::Down, 0) == 1 && ff6::moveFrame(ff6::Down, 8) == 2);
    CHECK(ff6::actionFrame(0) == 0 && ff6::actionFrame(0x46) == 9 && ff6::actionFrame(0x2f) == FF6_FRAME_RIDING_LEFT + 1);
    CHECK(ff6::actionFrame(0x2c) >= 16 && ff6::actionFrame(0x2c) < FF6_FRAME_COUNT && ff6::actionFrame(0x20) != ff6::actionFrame(0x2c));
    CHECK(ff6::actionFrame(0x7f) == 1);  // a frame that is not in the sheet shows the one facing down
    CHECK(ff6::moveFrame(ff6::Down, 16) == 1 && ff6::moveFrame(ff6::Down, 24) == 0);  // the walk cycle

    // Graphics 0 with palette 1: index 3 is the second color, index 0 stays transparent.
    ff6::CommonData common = test::makeCommon();
    common.spriteGfx.assign(FF6_SPRITE_SHEET_BYTES, 0);
    common.spriteGfx[0] = 3;  // frame 0, top-left pixel
    common.spriteGfx[FF6_FRAME_HEIGHT * FF6_FRAME_WIDTH] = 3;  // frame 1, top-left pixel
    common.spritePalettes.assign(2 * FF6_SPRITE_PAL_COLORS, 0);
    common.spritePalettes[FF6_SPRITE_PAL_COLORS + 3] = 0x001F;
    const std::vector<uint8_t> strip = ff6::buildSpriteStrip(common, 0, 1);
    const int stripWidth = FF6_FRAME_WIDTH * FF6_FRAME_COUNT;
    CHECK(strip.size() == static_cast<size_t>(stripWidth) * FF6_FRAME_HEIGHT * FF6_BYTES_PER_PIXEL);
    CHECK(strip[0] == 255 && strip[1] == 0 && strip[3] == 255);
    CHECK(strip[4 * FF6_FRAME_WIDTH] == 255 && strip[4 * FF6_FRAME_WIDTH + 3] == 255);  // frame 1 starts one frame to the right
    CHECK(strip[4] == 0 && strip[7] == 0);
    // Out of range graphics or palettes give an empty strip instead of failing.
    const std::vector<uint8_t> missing = ff6::buildSpriteStrip(common, 5, 1);
    CHECK(missing.size() == strip.size() && missing[3] == 0);

    // Rows are copied to the screen; transparent pixels and anything off the screen are skipped.
    std::vector<uint8_t> screen(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL, 9);
    ff6::drawSpriteRows(screen.data(), strip, 0, 0, FF6_SPRITE_TOP_HEIGHT, 10, 20);
    const auto at = [&](int x, int y) { return &screen[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL]; };
    CHECK(at(10, 20)[0] == 255 && at(10, 20)[3] == 255);
    CHECK(at(11, 20)[0] == 9);  // transparent: the screen is untouched
    ff6::drawSpriteRows(screen.data(), strip, 0, 0, FF6_SPRITE_TOP_HEIGHT, -1, -1);  // partly off the screen: no crash
    ff6::drawSpriteRows(screen.data(), strip, 0, 0, FF6_SPRITE_TOP_HEIGHT, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT);
    CHECK(at(0, 0)[0] == 9);

    // A sprite of the vehicle graphics: word 0x6F20 is the tile 0x120 of the second table, mirrored. Its first character is at
    // the start of the vehicle graphics, so its top-left pixel is that of the first row of the first tile.
    common.vehicleGfx.assign(FF6_VEHICLE_GFX_BYTES, 0);
    common.vehicleGfx[0] = 0x80;  // plane 0 of the pixel 0 of the row 0
    common.spritePalettes[FF6_SPRITE_PAL_COLORS + 1] = 0x7C00;
    const std::vector<uint8_t> plain = ff6::buildVehicleSprite(common, 1, 0x2F20);
    const std::vector<uint8_t> mirrored = ff6::buildVehicleSprite(common, 1, 0x6F20);
    const int last = (FF6_VEHICLE_SPRITE_SIZE - 1) * FF6_BYTES_PER_PIXEL;
    CHECK(plain.size() == static_cast<size_t>(FF6_VEHICLE_SPRITE_SIZE) * FF6_VEHICLE_SPRITE_SIZE * FF6_BYTES_PER_PIXEL);
    CHECK(plain[0 + 2] == 255 && plain[3] == 255 && plain[last + 3] == 0);
    CHECK(mirrored[last + 2] == 255 && mirrored[last + 3] == 255 && mirrored[3] == 0);
    CHECK(ff6::buildVehicleSprite(common, 99, 0x2F20)[3] == 0);  // an unknown palette gives an empty sprite
    std::fill(screen.begin(), screen.end(), 9);
    ff6::drawVehicleSprite(screen.data(), plain, 30, 40);
    CHECK(at(30, 40)[2] == 255 && at(31, 40)[0] == 9);
    ff6::drawVehicleSprite(screen.data(), plain, -5, FF6_SCREEN_HEIGHT - 1);  // partly off the screen: no crash

    // The pieces of each vehicle, in the order of the sprites of the game.
    const ff6::VehicleLook none = ff6::vehicleLook(0, ff6::Down, 0, false);
    CHECK(none.pieces.empty());
    const ff6::VehicleLook armor = ff6::vehicleLook(FF6_VEHICLE_MAGITEK | FF6_VEHICLE_RIDER, ff6::Right, 8, true);
    CHECK(armor.pieces.size() == 5 && armor.pieces[0].tile == 0 && armor.pieces[0].y == -14 && armor.pieces[0].rows == FF6_SPRITE_TOP_HEIGHT);
    CHECK(armor.riderFrame == FF6_FRAME_RIDING_RIGHT + 1);
    CHECK(ff6::vehicleLook(FF6_VEHICLE_MAGITEK, ff6::Right, 8, true).pieces.size() == 4);  // the rider is hidden
    const ff6::VehicleLook still = ff6::vehicleLook(FF6_VEHICLE_MAGITEK | FF6_VEHICLE_RIDER, ff6::Left, 8, false);
    CHECK(still.riderFrame == FF6_FRAME_RIDING_LEFT);  // standing, the rider keeps the first pose
    CHECK(ff6::vehicleLook(FF6_VEHICLE_MAGITEK | FF6_VEHICLE_RIDER, ff6::Up, 8, true).riderFrame == ff6::moveFrame(ff6::Up, 8));
    const ff6::VehicleLook raft = ff6::vehicleLook(FF6_VEHICLE_RAFT | FF6_VEHICLE_RIDER, ff6::Down, 0, false);
    CHECK(raft.pieces.size() == 6 && raft.pieces[1].firstRow == FF6_SPRITE_TOP_HEIGHT && raft.pieces[1].rows == FF6_SPRITE_BOTTOM_HEIGHT);
    CHECK(raft.riderFrame == ff6::stopFrame(ff6::Down));
    for (int facing = 0; facing < FF6_DIRECTION_COUNT; ++facing) {
        for (int step = 0; step < FF6_WALK_POSE_COUNT; ++step) {
            // The chocobo has the same number of pieces whichever way it looks: the rider, and four sprites of it.
            const ff6::VehicleLook choco = ff6::vehicleLook(FF6_VEHICLE_CHOCOBO | FF6_VEHICLE_RIDER, facing, step * 8, true);
            const size_t expected = facing == ff6::Up || facing == ff6::Down ? 4 : 6;
            CHECK(choco.pieces.size() == expected);
            for (const ff6::VehiclePiece& piece : choco.pieces) CHECK(piece.tile != 0 || piece.rows > 0);
        }
    }
}
