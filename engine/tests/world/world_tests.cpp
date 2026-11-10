#include <algorithm>
#include <cstdlib>
#include <memory>
#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "battle/encounters.hpp"
#include "world/world_screen.hpp"
#include "cutscene/train.hpp"
#include "data/battle_data.hpp"

namespace {

constexpr int kBlockedTile = 1;
constexpr int kForestTile = 2;
constexpr int kBattleTile = 3;

// A world with level ground, a wall, a forest and a place for the battles; the characters hold their own number.
ff6::WorldData makeWorld() {
    ff6::WorldData data;
    for (ff6::WorldMapData& world : data.worlds) {
        world.tilemap.assign(FF6_WORLD_TILEMAP_BYTES, 0);
        world.formation.assign(FF6_WORLD_FORMATION_BYTES, 0);
        for (size_t i = 0; i < world.formation.size(); ++i) world.formation[i] = static_cast<uint8_t>(i);
        world.chr.assign(FF6_WORLD_CHR_BYTES, 0);
        world.palette.assign(FF6_WORLD_PALETTE_COLORS, 0);
        world.spritePalette.assign(FF6_WORLD_PALETTE_COLORS, 0);
        world.tileProps.assign(FF6_WORLD_TILE_PROPS, 0x0004);
        world.tileProps[kBlockedTile] = 0x0014;
        world.tileProps[kForestTile] = 0x0024;
        world.tileProps[kBattleTile] = 0x0244;
        world.minimap.assign(FF6_WORLD_MINIMAP_BYTES, 0);
    }
    data.miscSprites.assign(FF6_WORLD_MISC_SPRITE_BYTES, 0);
    data.battleGroups.assign(FF6_WORLD_BATTLE_GROUP_BYTES, 0);
    data.battleRates.assign(FF6_WORLD_BATTLE_RATE_BYTES, 0);
    return data;
}

void setTile(ff6::WorldData& data, int world, int x, int y, int tile) {
    data.worlds[static_cast<size_t>(world)].tilemap[static_cast<size_t>(y * FF6_WORLD_SIZE + x)] = static_cast<uint8_t>(tile);
}

struct Harness {
    ff6::WorldData data;
    ff6::CommonData common;
    ff6::NoSound sound;
    std::vector<int> setBits;
    std::vector<int> bits;
    int steps = 0;
    int battle = -1;
    ff6::WorldHooks hooks;
    std::unique_ptr<ff6::WorldScreen> screen;

    explicit Harness(ff6::WorldData world, std::vector<uint8_t> script = {}) : data(std::move(world)), common(test::makeCommon(std::move(script))) {
        common.spriteGfx.assign(FF6_SPRITE_SHEET_BYTES, 0);
        common.spritePalettes.assign(FF6_SPRITE_PAL_COLORS, 0);
        hooks.bit = [this](int id) { return std::find(bits.begin(), bits.end(), id) != bits.end(); };
        hooks.setBit = [this](int id, bool value) {
            if (value) bits.push_back(id);
            setBits.push_back(id);
        };
        hooks.step = [this] { ++steps; };
        hooks.encounter = [this](int, int tileBackground, int, int, int* background) {
            *background = tileBackground;
            return battle;
        };
        screen = std::make_unique<ff6::WorldScreen>(data, common, sound, hooks);
    }
    void run(int frames, int direction = FF6_NO_DIRECTION) {
        ff6::InputState input;
        input.direction = direction;
        for (int i = 0; i < frames; ++i) screen->tick(input);
    }
};

}  // namespace

void testWorld() {
    {  // The perspective: every line has its own scale, the first the largest, and nothing turns
        Harness h(makeWorld());
        h.screen->enter(0, 100, 100, ff6::Down, {});
        std::vector<uint8_t> image(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
        h.screen->draw(image.data());
        const ff6::Ppu& ppu = h.screen->ppu();
        CHECK(ppu.lines[0].m7a == 0x9000 / 0x8F && ppu.lines[0].m7d == ppu.lines[0].m7a);
        CHECK(ppu.lines[FF6_SCREEN_HEIGHT - 1].m7a == 0x9000 / 0xFF);
        CHECK(ppu.lines[10].m7b == 0 && ppu.lines[10].m7c == 0 && (ppu.lines[10].bgMode & 7) == 7);
        for (int y = 1; y < FF6_SCREEN_HEIGHT; ++y) CHECK(ppu.lines[static_cast<size_t>(y)].m7a <= ppu.lines[static_cast<size_t>(y - 1)].m7a);
        // The center is the party, and the scroll puts it in the middle of the screen.
        CHECK(ppu.m7x == 100 * 16 + 8 && ppu.m7y == 100 * 16 - 2);
        CHECK(ppu.m7hofs == ppu.m7x - 0x80 && ppu.m7vofs == ppu.m7y - 0x70);
        // The video memory holds the 64 x 64 tiles around the party, each where its coordinates give modulo 64: the low
        // byte of a word is the map, the high byte the graphics.
        setTile(h.data, 0, 100, 100, 5);
        h.screen->enter(0, 100, 100, ff6::Down, {});
        const size_t at = static_cast<size_t>(((100 * 2) & 127) * 128 + ((100 * 2) & 127));
        CHECK((ppu.vram[at] & 0xFF) == 20 && (ppu.vram[at + 1] & 0xFF) == 21 && (ppu.vram[at + 128] & 0xFF) == 22 && (ppu.vram[at + 129] & 0xFF) == 23);
        CHECK((ppu.vram[0] >> 8) == h.data.worlds[0].chr[0]);
        // The edges of the screen are hidden by the window of the game, and the screen is lit.
        CHECK(image[3] == 255);
    }

    {  // A step takes 16 frames and ends on the next tile; a wall stops it
        ff6::WorldData world = makeWorld();
        setTile(world, 0, 11, 10, kBlockedTile);
        Harness h(std::move(world));
        h.screen->enter(0, 10, 10, ff6::Down, {});
        h.run(1, ff6::Right);
        CHECK(!h.screen->moving() && h.screen->facing() == ff6::Right && h.screen->tileX() == 10 && h.steps == 0);
        h.run(1, ff6::Left);
        CHECK(h.screen->moving() && h.screen->facing() == ff6::Left && h.steps == 1);
        h.run(14);
        CHECK(h.screen->moving() && !h.screen->aligned());
        h.run(1);
        CHECK(h.screen->tileX() == 9 && h.screen->tileY() == 10 && h.screen->aligned());
        h.run(1, ff6::Up);  // the next step starts as soon as the party is on the tile
        CHECK(h.screen->moving() && h.steps == 2);
        h.run(16);
        CHECK(h.screen->tileX() == 9 && h.screen->tileY() == 9);
        CHECK(h.screen->pixelX() == 9 * 16 && h.screen->pixelY() == 9 * 16);
    }

    {  // The menu opens when the party stands still, not in the middle of a step
        Harness h(makeWorld());
        h.screen->enter(0, 10, 10, ff6::Down, {});
        ff6::InputState press;
        press.menu = true;
        h.screen->tick(press);
        CHECK(h.screen->takeMenuRequest() && !h.screen->takeMenuRequest());
        h.run(1, ff6::Up);
        h.screen->tick(press);
        CHECK(!h.screen->takeMenuRequest());
    }

    {  // An entrance is reached by walking on its tile, and tells where it leads
        ff6::WorldData world = makeWorld();
        world.worlds[0].shortEntrances = {10, 9, 0x75, 0x2E, 7, 8};  // map 0x75, set the parent, facing down
        Harness h(std::move(world));
        h.screen->enter(0, 10, 10, ff6::Down, {});
        ff6::EntranceTarget target;
        CHECK(!h.screen->takeEntrance(&target));
        h.run(16, ff6::Up);
        CHECK(!h.screen->takeEntrance(&target));
        h.run(1);
        CHECK(h.screen->takeEntrance(&target) && !h.screen->takeEntrance(&target));
        CHECK(target.map == 0x75 && target.x == 7 && target.y == 8 && target.setParent && target.facing == ff6::Down && !target.toParent);
    }

    {  // A random battle zooms the map in for a while and then starts, once the tile has been reached
        ff6::WorldData world = makeWorld();
        setTile(world, 0, 10, 9, kBattleTile);
        Harness h(std::move(world));
        h.battle = 0x0105;
        h.screen->enter(0, 10, 10, ff6::Down, {});
        h.run(16, ff6::Up);
        CHECK(!h.screen->zooming());
        h.run(1);
        CHECK(h.screen->zooming());
        int group = 0, background = 0;
        CHECK(!h.screen->takeBattle(&group, &background));
        h.run(FF6_WORLD_ZOOM_STEPS - 1);
        CHECK(h.screen->zooming());
        h.run(1);
        CHECK(!h.screen->zooming() && h.screen->takeBattle(&group, &background) && group == 0x0105 && background == 2);
        // With the bit that stops them, nothing happens.
        Harness quiet(makeWorld());
        setTile(quiet.data, 0, 10, 9, kBattleTile);
        quiet.battle = 3;
        quiet.bits = {FF6_EVENT_BIT_NO_ENCOUNTERS};
        quiet.screen->enter(0, 10, 10, ff6::Down, {});
        quiet.run(17, ff6::Up);
        CHECK(!quiet.screen->zooming());
    }

    {  // The map changes with the events: a rectangle of tiles once its bit is set
        ff6::WorldData world = makeWorld();
        world.worlds[0].mods.push_back({7, 3, 4, 2, 1, {9, 8}});
        Harness without(world);
        without.screen->enter(0, 10, 10, ff6::Down, {});
        CHECK(without.screen->tileAt(3, 4) == 0 && without.screen->tileAt(4, 4) == 0);
        Harness with(world);
        with.bits = {7};
        with.screen->enter(0, 10, 10, ff6::Down, {});
        CHECK(with.screen->tileAt(3, 4) == 9 && with.screen->tileAt(4, 4) == 8 && with.screen->tileAt(5, 4) == 0);
        CHECK(with.screen->propAt(3, 4) == 0x0004);
    }

    {  // An event on a tile: a jump on a bit, a bit set, and another map
        ff6::WorldData world = makeWorld();
        world.worlds[0].eventTriggers = {10, 9, 0, 0, 0};
        const std::vector<uint8_t> script = {0xB0, 0x07, 0x00, 0x0F, 0x00, 0x00,  // when bit 7 is set, go to the end
                                              0xC8, 0x08, 0x00,  // set bit 8
                                              0xD2, 0x75, 0x2E, 5, 6, 0x80,  // load map 0x75 at 5, 6
                                              0xFF};
        Harness h(world, script);
        h.screen->enter(0, 10, 10, ff6::Down, {});
        h.run(16, ff6::Up);
        h.run(3);
        ff6::EntranceTarget target;
        CHECK(h.screen->takeEntrance(&target) && target.map == 0x75 && target.x == 5 && target.y == 6 && target.setParent);
        CHECK(std::find(h.setBits.begin(), h.setBits.end(), 8) != h.setBits.end());
        Harness blocked(world, script);
        blocked.bits = {7};
        blocked.screen->enter(0, 10, 10, ff6::Down, {});
        blocked.run(16, ff6::Up);
        blocked.run(5);
        CHECK(!blocked.screen->takeEntrance(&target) && !blocked.screen->eventRunning() && blocked.setBits.empty());
    }

    {  // A script walks along a diagonal: 2 tiles across and 1 up in 16 frames
        ff6::WorldData world = makeWorld();
        world.worlds[0].eventTriggers = {10, 9, 0, 0, 0};
        Harness h(world, {0xA5, 0xFF});
        h.screen->enter(0, 10, 10, ff6::Down, {});
        h.run(16, ff6::Up);
        h.run(1);
        CHECK(h.screen->eventRunning() && h.screen->moving());
        h.run(16);
        CHECK(h.screen->tileX() == 12 && h.screen->tileY() == 8 && h.screen->facing() == ff6::Right);
    }

    {  // The character shows through the trees: its lower half goes behind the map
        ff6::WorldData world = makeWorld();
        setTile(world, 0, 10, 9, kForestTile);
        Harness h(std::move(world));
        h.screen->enter(0, 10, 10, ff6::Down, {});
        std::vector<uint8_t> image(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
        h.screen->draw(image.data());
        const ff6::Ppu& ppu = h.screen->ppu();
        CHECK(ppu.sprites.size() == FF6_WORLD_SPRITE_COUNT && ppu.sprites[24].attr == (1 << 4));
        h.run(16, ff6::Up);
        h.run(1);  // arrived in the forest, and not leaving it
        h.screen->draw(image.data());
        CHECK(ppu.sprites[20].attr == (1 << 4) && ppu.sprites[24].attr == (4 << 1) && ppu.sprites[25].attr == (4 << 1));
        CHECK(ppu.sprites[20].x == 120 && ppu.sprites[21].x == 128 && ppu.sprites[20].y == 104 && ppu.sprites[24].y == 120);
    }

    {  // The water moves a pixel at a time, and Start hides the mini-map
        ff6::WorldData world = makeWorld();
        world.worlds[0].chr[0x44 * 64] = 0x55;
        Harness h(std::move(world));
        h.screen->enter(0, 10, 10, ff6::Down, {});
        const ff6::Ppu& ppu = h.screen->ppu();
        CHECK((ppu.vram[0x44 * 64] >> 8) == 0x55 && (ppu.vram[0x45 * 64 + 7] >> 8) == 0);
        h.run(8);  // the first row is the first one to turn
        CHECK((ppu.vram[0x44 * 64] >> 8) == 0 && (ppu.vram[0x45 * 64 + 7] >> 8) == 0x55);
        std::vector<uint8_t> image(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
        h.screen->draw(image.data());
        CHECK(ppu.sprites[5].y == 0x9C && ppu.sprites[0].y == 0x9B + (10 * 16 >> 6));
        ff6::InputState start;
        start.start = true;
        h.screen->tick(start);
        h.screen->draw(image.data());
        CHECK(ppu.sprites[5].y == 0xE0 && ppu.sprites[0].y == 0xE0);
    }

    {  // The random battles of the world come from the table of the sector that the map was entered in
        ff6::WorldData world = makeWorld();
        world.battleGroups[0 * 4 + 0] = 2;  // the first sector of the first world, on grass
        world.battleRates[0] = 0;  // normal everywhere
        world.battleRates[1] = 0xC0;  // sector 1: no battles on grass
        ff6::BattleData battle;
        battle.rngTable.assign(FF6_RNG_BYTES, 0);
        battle.randGroups.assign(static_cast<size_t>(FF6_BATTLE_GROUP_COUNT) * FF6_RAND_GROUP_BATTLES * 2, 0);
        battle.randGroups[2 * FF6_RAND_GROUP_BATTLES * 2] = 0x34;
        battle.randGroups[2 * FF6_RAND_GROUP_BATTLES * 2 + 1] = 0x12;
        ff6::Encounters walk;
        int background = -1;
        CHECK(walk.stepWorld(battle, world, 0, 0, 5, 5, &background) == -1 && background == 0);  // the counter has to build up
        CHECK(walk.stepWorld(battle, world, 0, 0, 5, 5, &background) == 0x1234);
        CHECK(ff6::Encounters().stepWorld(battle, world, 0, 5, 5, 5, &background) == -1 && background == 5);  // each tile has its background
        ff6::Encounters other;
        for (int i = 0; i < 4; ++i) CHECK(other.stepWorld(battle, world, 0, 0, 32, 0, &background) == -1);  // sector 1 has none
        world.battleGroups[0] = FF6_WORLD_VELDT_GROUP;  // the formations of the veldt are not ported
        ff6::Encounters veldt;
        for (int i = 0; i < 4; ++i) CHECK(veldt.stepWorld(battle, world, 0, 0, 0, 0, &background) == -1);
        // With battles in the list, the veldt brings them back, one byte of the list after the other.
        ff6::PartyState party;
        party.veldtBattles[17] = party.veldtBattles[18] = true;
        ff6::Encounters fights;
        CHECK(fights.stepWorld(battle, world, 0, 0, 0, 0, &background, &party) == -1);  // the counter builds up
        CHECK(fights.stepWorld(battle, world, 0, 0, 0, 0, &background, &party) == 17 && party.veldtPointer == 2);
    }

    {  // The airship: the party gets on board, flies at the pace of the controls, and lands where the ground allows it
        ff6::WorldData world = makeWorld();
        world.worlds[0].tileProps[4] = 0x0006;  // water: no landing
        setTile(world, 0, 50, 50, 4);
        std::vector<uint8_t> script(0x80, 0xFF);
        const uint8_t deck[] = {0xC7, 0x05, 0x06, 0xD2, 0x0B, 0x32, 0x10, 0x08, 0x80, 0xFF};  // the deck of the airship
        std::copy(deck, deck + sizeof(deck), script.begin() + 0x68);
        const uint8_t magicite[] = {0xD2, 0x11, 0x36, 0x11, 0x08, 0x01, 0xFF};  // the scene after Doom Gaze: the airship flies on
        std::copy(magicite, magicite + sizeof(magicite), script.begin() + 0x74);
        Harness h(world, script);
        const ff6::WorldScreen::Lead lead{0, 0};
        std::vector<uint8_t> image(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
        const ff6::Ppu& ppu = h.screen->ppu();
        const auto hold = [&](unsigned buttons, int frames) {
            ff6::InputState input;
            input.held = buttons;
            for (int i = 0; i < frames; ++i) h.screen->tick(input);
        };

        // On the tile of the parked airship, the confirm button takes off: the screen fades out, the camera rises
        // along the table, and the flight begins over the same tile.
        h.screen->enter(0, 10, 10, ff6::Down, lead);
        h.screen->setAirship(10, 10);
        ff6::InputState confirm;
        confirm.confirm = true;
        h.screen->tick(confirm);
        CHECK(!h.screen->flying());  // the airship has to be there
        h.bits.push_back(FF6_EVENT_BIT_AIRSHIP_VISIBLE);
        h.screen->tick(confirm);
        CHECK(h.screen->flying());
        for (int i = 0; i < 20; ++i) h.screen->tick({});
        h.screen->draw(image.data());
        CHECK(ppu.lines[0].bgMode == 7 && ppu.lines[FF6_SCREEN_HEIGHT - 1].bgMode == 7);  // the map fills the screen at first
        for (int i = 0; i < 40; ++i) h.screen->tick({});
        h.screen->draw(image.data());
        CHECK(h.screen->flightTileX() == 10 && h.screen->flightTileY() == 10 && h.screen->flightAltitude() == 0);
        CHECK(ppu.lines[0].bgMode == 2 && ppu.lines[FF6_SCREEN_HEIGHT - 1].bgMode == 7);  // the sky above the horizon
        CHECK(ppu.lines[FF6_SCREEN_HEIGHT - 1].m7b == 0 && ppu.lines[FF6_SCREEN_HEIGHT - 1].m7a == ppu.lines[FF6_SCREEN_HEIGHT - 1].m7d);
        CHECK(ppu.lines[FF6_SCREEN_HEIGHT - 1].m7a < ppu.lines[FF6_SCREEN_HEIGHT - 80].m7a);  // the farther, the smaller the map

        // Flying: the engines push the airship north, the turn changes the heading, the climb the altitude.
        h.screen->enterFlying(0, 100, 100, ff6::Down, lead, 0, 0);
        CHECK(h.screen->flying() && h.screen->flightAngle() == 0);
        hold(FF6_HELD_A, 40);
        CHECK(h.screen->flightTileX() == 100 && h.screen->flightTileY() < 90);
        hold(0, 20);  // the engines slow down by themselves
        const int stopped = h.screen->flightTileY();
        hold(0, 20);
        CHECK(h.screen->flightTileY() == stopped);
        hold(FF6_HELD_LEFT, 10);
        CHECK(h.screen->flightAngle() > 0 && h.screen->flightAngle() < 90);
        h.screen->enterFlying(0, 100, 100, ff6::Down, lead, 90, 0);  // 90 degrees: west
        hold(FF6_HELD_A, 40);
        CHECK(h.screen->flightTileX() < 90 && h.screen->flightTileY() == 100);
        h.screen->draw(image.data());
        CHECK(ppu.lines[FF6_SCREEN_HEIGHT - 1].m7a == 0 && ppu.lines[FF6_SCREEN_HEIGHT - 1].m7b != 0 &&
              ppu.lines[FF6_SCREEN_HEIGHT - 1].m7c == -ppu.lines[FF6_SCREEN_HEIGHT - 1].m7b);  // the map is turned a quarter
        h.screen->enterFlying(0, 100, 100, ff6::Down, lead, 0, 0);
        hold(FF6_HELD_DOWN, 10);
        const int climbed = h.screen->flightAltitude();
        CHECK(climbed > 0);
        hold(FF6_HELD_DOWN, 300);
        CHECK(h.screen->flightAltitude() <= 0x7E00 && h.screen->flightAltitude() > climbed);
        hold(FF6_HELD_UP, 300);
        CHECK(h.screen->flightAltitude() < 0x200);  // and the ground stops it

        // Landing: the water does not allow it, the grass does and the airship stays where it is.
        h.screen->enterFlying(0, 50, 50, ff6::Down, lead, 0, 0x2000);
        ff6::InputState land;
        land.cancel = true;
        h.screen->tick(land);
        hold(0, 200);
        CHECK(h.screen->flying() && h.screen->flightAltitude() == 0x2000);
        h.screen->enterFlying(0, 60, 60, ff6::Down, lead, 0, 0x2000);
        h.screen->tick(land);
        for (int i = 0; i < 300 && h.screen->flying(); ++i) h.screen->tick({});
        CHECK(!h.screen->flying() && h.screen->tileX() == 60 && h.screen->tileY() == 60);
        CHECK(h.screen->airshipX() == 60 && h.screen->airshipY() == 60);
        for (int i = 0; i < 20; ++i) h.screen->tick({});
        h.screen->draw(image.data());
        CHECK(ppu.sprites[20].y != 0xE0);  // the party is in front of the airship

        // The deck: a script of the airship, which sets where it stays and takes the party to another map; the heading
        // and the altitude are kept for the way back.
        h.screen->enterFlying(0, 70, 70, ff6::Down, lead, 45, 0x1800);
        ff6::InputState menu;
        menu.menu = true;
        h.screen->tick(menu);
        for (int i = 0; i < 3; ++i) h.screen->tick({});
        ff6::EntranceTarget target;
        CHECK(h.screen->takeEntrance(&target) && target.map == 0x0B && target.x == 0x10 && target.y == 8 && target.setParent);
        CHECK(h.screen->keepsFlying() && h.screen->flightAngle() == 45 && h.screen->flightAltitude() == 0x1800);
        CHECK(h.screen->airshipX() == 5 && h.screen->airshipY() == 6);

        // The Y button moves the airship sideways, without turning it, while the engines stop.
        h.screen->enterFlying(0, 100, 100, ff6::Down, lead, 0, 0);
        hold(FF6_HELD_A, 20);
        hold(FF6_HELD_Y | FF6_HELD_LEFT, 40);
        const int strafed = h.screen->flightTileX();
        CHECK(h.screen->flightAngle() == 0 && strafed < 100 && h.screen->flightTileY() < 100);
        h.screen->enterFlying(0, 100, 100, ff6::Down, lead, 0, 0);
        hold(FF6_HELD_Y | FF6_HELD_LEFT, 40);
        CHECK(h.screen->flightTileX() < 100 && h.screen->flightTileX() > 90 && h.screen->flightTileY() == 100);

        // A turn tilts the sky: each column and each band of lines scrolls by its own amount.
        h.screen->enterFlying(0, 100, 100, ff6::Down, lead, 0, 0);
        h.screen->draw(image.data());
        CHECK(!ppu.lines[0].bg2Columns);
        hold(FF6_HELD_A, 10);
        hold(FF6_HELD_A | FF6_HELD_LEFT, 20);
        h.screen->draw(image.data());
        CHECK(ppu.lines[0].bg2Columns && ppu.lines[0].bg2ColumnVofs[32] != ppu.lines[0].bg2ColumnVofs[1]);

        // Doom Gaze: only in the world of ruin, with the engines on, over one of its squares.
        h.screen->setDoomGaze(true, 25, 25, 1, 1);
        h.screen->enterFlying(0, 100, 100, ff6::Down, lead, 0, 0);
        h.screen->setDoomGaze(true, 25, 25, 1, 1);
        hold(FF6_HELD_A, 1);
        CHECK(!h.screen->takeDoomGaze());
        h.screen->enterFlying(1, 100, 100, ff6::Down, lead, 0, 0);
        h.screen->setDoomGaze(true, 25, 25, 1, 1);
        hold(0, 3);
        CHECK(!h.screen->takeDoomGaze());
        hold(FF6_HELD_A, 1);
        CHECK(h.screen->takeDoomGaze() && !h.screen->takeDoomGaze());
        h.screen->enterFlying(1, 100, 100, ff6::Down, lead, 0, 0);
        h.screen->setDoomGaze(false, 25, 25, 1, 1);
        hold(FF6_HELD_A, 3);
        CHECK(!h.screen->takeDoomGaze());

        // The scene that follows it takes the party to a map but the airship flies on.
        h.screen->enterFlying(1, 70, 70, ff6::Down, lead, 0, 0x1000);
        h.screen->startVehicleEvent(0x74);
        for (int i = 0; i < 3; ++i) h.screen->tick({});
        CHECK(h.screen->takeEntrance(&target) && target.map == 0x11 && h.screen->keepsFlying() && h.screen->airshipX() != 70);
    }

    {  // The chocobo: it runs at the pace of a walk, stops at the tiles that it cannot cross and the party gets off where it is
        ff6::WorldData world = makeWorld();
        world.worlds[0].tileProps[5] = 0x0005;  // the chocobo does not go there
        for (int x = 90; x < 110; ++x) setTile(world, 0, x, 90, 5);
        Harness h(world);
        const ff6::WorldScreen::Lead lead{0, 0};
        const auto hold = [&](unsigned buttons, int frames) {
            ff6::InputState input;
            input.held = buttons;
            for (int i = 0; i < frames; ++i) h.screen->tick(input);
        };
        h.screen->enterRiding(0, 100, 100, ff6::Up, lead);
        CHECK(h.screen->riding() && h.screen->flying() && h.screen->flightAngle() == 0);
        hold(FF6_HELD_UP, 20);
        CHECK(h.screen->flightTileY() == 99 || h.screen->flightTileY() == 98);  // 20 frames, a pixel each at most
        hold(FF6_HELD_UP, 300);
        CHECK(h.screen->flightTileY() == 91);  // the wall stops it, and it does not go through
        const int stuck = h.screen->flightTileY();
        hold(FF6_HELD_UP | FF6_HELD_LEFT, 40);  // it turns, and slides along the wall or leaves it
        CHECK(h.screen->flightAngle() > 0 && h.screen->flightTileY() >= stuck - 0 && h.screen->flightTileY() > 90);
        h.screen->enterRiding(0, 100, 100, ff6::Down, lead);
        CHECK(h.screen->flightAngle() == 180);
        ff6::InputState land;
        land.cancel = true;
        h.screen->tick(land);
        for (int i = 0; i < 40; ++i) h.screen->tick({});
        CHECK(!h.screen->flying() && h.screen->tileX() == 100 && h.screen->tileY() == 100);
        CHECK(h.screen->airshipX() == 0 && h.screen->airshipY() == 0);  // the chocobo is not the airship
    }

    {  // A script that an event of the field hands to the airship: the camera and the heading, the battle, the loading of a map
        std::vector<uint8_t> script(0x40, 0xFF);
        const uint8_t story[] = {0xC0, 0x20, 0xC1, 0xB4, 0x00, 0xC2, 0xB4, 0x00, 0xC6, 0x00, 0x08, 0xC5, 0x00, 0x10, 0xD1, 0xDA, 0xE0, 0x01,
                                 0xCA, 0x14, 0x23, 0xD2, 0xAF, 0x04, 0x2C, 0x10, 0x00, 0xFF};
        std::copy(story, story + sizeof(story), script.begin() + 0x10);
        Harness h(makeWorld(), script);
        const ff6::WorldScreen::Lead lead{0, 0};
        h.screen->enterFlying(0, 70, 70, ff6::Down, lead, 90, 0);
        h.screen->startStoryScript(0x10);
        for (int i = 0; i < 4; ++i) h.screen->tick({});
        CHECK(h.screen->flightAngle() == 180 && h.screen->flightAltitude() == 0x1000);
        std::vector<uint8_t> image(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
        h.screen->draw(image.data());
        CHECK(h.screen->ppu().sprites[20].y == 0xE0);  // the airship is hidden
        ff6::InputState left;
        left.held = FF6_HELD_LEFT;
        for (int i = 0; i < 4; ++i) h.screen->tick(left);  // the arrows are shown: the left is the choice
        CHECK(std::find(h.bits.begin(), h.bits.end(), FF6_EVENT_BIT_TRENCH_LEFT) != h.bits.end());
        int group = -1, background = -1;
        for (int i = 0; i < 60 && !h.screen->takeScriptBattle(&group, &background); ++i) h.screen->tick({});
        CHECK(group == 0x14 && background == 0x23);
        CHECK(h.screen->flightTileY() > 70);  // it moved south, whichever way the camera turned
        ff6::EntranceTarget target;
        uint32_t resume = 0;
        for (int i = 0; i < 4 && !h.screen->takeEntrance(&target); ++i) h.screen->tick({});
        CHECK(target.map == 175 && target.x == 0x2C && target.y == 0x10);
        CHECK(h.screen->takeResume(&resume) && resume == 0x10 + 27 && !h.screen->storyScriptRunning());
    }

    if (const char* pack = std::getenv("FF6_PACK")) {  // The ride of the magitek train, on the data of the game
        const ff6::WorldData world = ff6::loadWorldData(pack);
        const ff6::BattleData battle = ff6::loadBattleData(pack);
        ff6::NoSound sound;
        ff6::TrainRide ride(world.train, battle.rngTable, 0, sound);
        CHECK(ride.brightness() == 0 && ride.step() == 0 && !ride.finished());
        for (int i = 0; i < 8; ++i) ride.tick({});  // two steps of four frames
        CHECK(ride.step() == 2 && ride.brightness() == 2);
        int lit = 0;
        for (int y = FF6_TRAIN_BUFFER_Y; y < FF6_TRAIN_BUFFER_Y + FF6_TRAIN_BUFFER; ++y) {
            for (int x = FF6_TRAIN_BUFFER_X; x < FF6_TRAIN_BUFFER_X + FF6_TRAIN_BUFFER; ++x) lit += ride.cell(x, y) != 0;
        }
        CHECK(lit > 400);  // the tunnel and its rails
        std::vector<uint8_t> image(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
        ride.draw(image.data());
        // The way: the first battle comes before the 33rd step, the ride waits for it and goes on after it, the course ends
        // with a fade out.
        int battles = 0, group = 0, background = 0, bosses = 0;
        bool second = false;
        for (int i = 0; i < 20000 && !ride.finished(); ++i) {
            ride.tick({});
            if (ride.takeBattle(&group, &second, &background)) {
                if (battles == 0) {
                    CHECK(group == FF6_TRAIN_BATTLE_GROUP_1 && background == FF6_TRAIN_BATTLE_BG && ride.step() == 33);
                    const int waiting = ride.step();
                    for (int k = 0; k < 20; ++k) ride.tick({});
                    CHECK(ride.step() == waiting);  // it waits for the battle
                }
                ++battles;
                bosses += group == FF6_TRAIN_BOSS_GROUP;
                ride.resume();
            }
        }
        CHECK(ride.finished() && battles >= 3 && bosses == 1);
    }
}
