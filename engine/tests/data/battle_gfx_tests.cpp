#include <cstdlib>

#include "../check.hpp"
#include "../tests.hpp"
#include "data/battle_gfx.hpp"

// With the real pack: every monster has a picture of its own size, with something in it.
void testBattleGfx() {
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::BattleGfx gfx = ff6::loadBattleGfx(pack);
    int largest = 0;
    for (int id = 0; id < FF6_MONSTER_GFX_COUNT; ++id) {
        const ff6::BattleImage& image = gfx.monster(id);
        CHECK(image.width > 0 && image.height > 0 && image.width <= 128 && image.height <= 128);
        CHECK(image.rgba.size() == static_cast<size_t>(image.width) * image.height * FF6_BYTES_PER_PIXEL);
        size_t visible = 0;
        for (size_t i = FF6_ALPHA_OFFSET; i < image.rgba.size(); i += FF6_BYTES_PER_PIXEL) visible += image.rgba[i] != 0;
        CHECK(visible > 0 || image.width == FF6_MENU_TILE_SIZE);  // two monsters are a single blank tile
        largest = std::max(largest, std::max(image.width, image.height));
    }
    CHECK(largest == 128);  // the large stencils
    CHECK(&gfx.monster(0) == &gfx.monster(0));  // decoded once
    CHECK(gfx.monster(-1).rgba.empty() && gfx.monster(FF6_MONSTER_GFX_COUNT).rgba.empty());
    CHECK(gfx.monster(0).width == 64 || gfx.monster(0).width < 64);  // the Guard is a small one

    // The backgrounds: the battlefield of every one of them has colors, and none for a number that does not exist.
    for (int bg = 0; bg < FF6_BG_COUNT - 3; ++bg) {
        const ff6::BattleImage& field = gfx.background(bg);
        CHECK(field.width == FF6_SCREEN_WIDTH && field.height == FF6_BG_ROWS * FF6_MENU_TILE_SIZE);
        size_t lit = 0;
        for (size_t i = 0; i < field.rgba.size(); i += FF6_BYTES_PER_PIXEL) lit += field.rgba[i] || field.rgba[i + 1] || field.rgba[i + 2];
        CHECK(lit > 0);
    }
    CHECK(gfx.background(-1).rgba.empty() && gfx.background(FF6_BG_COUNT).rgba.empty());
    // The colors of the raft, the waterfall and Cyan's dream run: each turn of the palette is another picture, and it comes back.
    for (const int bg : {FF6_BG_RAFT, FF6_BG_WATERFALL, FF6_BG_CYANS_DREAM}) CHECK(gfx.background(bg, 0).rgba != gfx.background(bg, 1).rgba);
    CHECK(ff6::BattleGfx::backgroundStep(FF6_BG_WATERFALL, 4) == 1 && ff6::BattleGfx::backgroundStep(FF6_BG_RAFT, 0) == 3 &&
          ff6::BattleGfx::backgroundStep(0, 99) == 0);

    // The statuses: a sprite for each at four frames, the shadow of the characters, and a palette that the statuses can change.
    for (int kind = 1; kind <= FF6_STATUS_SPRITE_SHADOW; ++kind) {
        for (int frame = 0; frame < 4; ++frame) {
            const ff6::BattleImage& sprite = gfx.statusSprite(kind, frame);
            size_t visible = 0;
            for (size_t i = FF6_ALPHA_OFFSET; i < sprite.rgba.size(); i += FF6_BYTES_PER_PIXEL) visible += sprite.rgba[i] != 0;
            CHECK(sprite.width == 16 && sprite.height == 16 && visible > 4);
        }
    }
    CHECK(gfx.statusSprite(0, 0).rgba.empty() && gfx.statusSprite(1, 4).rgba.empty());
    std::array<uint16_t, 16> stone = gfx.characterPalette(0);
    CHECK(stone[1] != 0 || stone[2] != 0);
    const ff6::BattleImage plain = gfx.characterWith(0, FF6_CHAR_POSE_FIGHT, stone);
    CHECK(plain.rgba == gfx.character(0, FF6_CHAR_POSE_FIGHT).rgba);
    stone[6] = 0x7C00;
    CHECK(gfx.characterWith(0, FF6_CHAR_POSE_FIGHT, stone).rgba != plain.rgba);
    const ff6::BattleImage outline = gfx.characterWith(0, FF6_CHAR_POSE_FIGHT, gfx.characterPalette(0), true);
    size_t outlined = 0, whole = 0;
    for (size_t i = FF6_ALPHA_OFFSET; i < plain.rgba.size(); i += FF6_BYTES_PER_PIXEL) {
        outlined += outline.rgba[i] != 0;
        whole += plain.rgba[i] != 0;
    }
    CHECK(outlined > 0 && outlined < whole);

    // The characters: every one of them has a picture in each pose that the screen uses.
    for (int actor = 0; actor < 14; ++actor) {
        for (const int pose : {FF6_CHAR_POSE_FIGHT, FF6_CHAR_POSE_READY, FF6_CHAR_POSE_NEAR_FATAL, FF6_CHAR_POSE_DEAD}) {
            const ff6::BattleImage& image = gfx.character(actor, pose);
            CHECK(image.width == FF6_CHAR_WIDTH && image.height == FF6_CHAR_HEIGHT);
            size_t visible = 0;
            for (size_t i = FF6_ALPHA_OFFSET; i < image.rgba.size(); i += FF6_BYTES_PER_PIXEL) visible += image.rgba[i] != 0;
            CHECK(visible > 20);
        }
    }
    CHECK(gfx.character(-1, 0).rgba.empty() && gfx.character(FF6_CHAR_GFX_COUNT, 0).rgba.empty() && gfx.character(0, FF6_CHAR_ACTION_COUNT).rgba.empty());
    CHECK(gfx.character(0, FF6_CHAR_POSE_FIGHT).rgba != gfx.character(1, FF6_CHAR_POSE_FIGHT).rgba);  // Terra is not Locke

    // The tile decoder: a plane of the first pixel only, in the color that its bits choose.
    uint8_t colors[16][FF6_BYTES_PER_PIXEL] = {};
    for (int i = 1; i < 16; ++i) colors[i][0] = static_cast<uint8_t>(i), colors[i][FF6_ALPHA_OFFSET] = 255;
    uint8_t tile[FF6_TILE_4BPP_BYTES] = {};
    tile[0] = 0x80;  // plane 0
    tile[17] = 0x80;  // plane 3 of the first row
    std::vector<uint8_t> out(static_cast<size_t>(8) * 8 * FF6_BYTES_PER_PIXEL, 0);
    ff6::decodeTile(tile, false, colors, out.data(), 8, 0, 0);
    CHECK(out[0] == 9 && out[FF6_ALPHA_OFFSET] == 255 && out[FF6_BYTES_PER_PIXEL + FF6_ALPHA_OFFSET] == 0);
    uint8_t three[FF6_TILE_3BPP_BYTES] = {};
    three[16] = 0x80;  // the third plane of the first row, in the byte after the first two planes
    std::fill(out.begin(), out.end(), 0);
    ff6::decodeTile(three, true, colors, out.data(), 8, 0, 0);
    CHECK(out[0] == 4);
}
