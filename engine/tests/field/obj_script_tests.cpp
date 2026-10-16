#include <algorithm>
#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/obj.hpp"
#include "field/sprite_data.hpp"

namespace {

constexpr int kTile = FF6_OBJ_TILE_SUBPIXELS;

// Starts the script at offset 0 on the first npc and runs until it ends. Returns the frames that it took.
int runToEnd(test::ObjScene& scene, int limit = 800) {
    scene.objects.startScript(scene.npc(), 0);
    int frames = 0;
    while (scene.npc().movement != FF6_OBJ_MOVE_NONE && frames < limit) {
        scene.run(1);
        ++frames;
    }
    return frames;
}

test::ObjScene sceneAt(std::vector<uint8_t> script, int x, int y, int facing = ff6::Up) {
    return test::ObjScene(std::move(script), {test::npcBytes(x, y, 0, 1, 0, facing)});
}

}  // namespace

void testObjScript() {
    {  // walking: two tiles right, then a turn; the npc stands still on the tile at the end
        test::ObjScene scene = sceneAt({0x85, 0xCE, 0xFF}, 2, 2);
        const int frames = runToEnd(scene);
        const ff6::Npc& npc = scene.npc();
        CHECK(npc.tileX() == 4 && npc.tileY() == 2 && npc.aligned() && npc.speedX == 0 && npc.speedY == 0);
        CHECK(npc.facing == ff6::Down && npc.frame == ff6::stopFrame(ff6::Down) && npc.scriptPtr == -1);
        CHECK(frames > 2 * FF6_TILE_SIZE && frames < 2 * FF6_TILE_SIZE + 24);
    }
    {  // every direction and length: left 1, up 3, down 1
        test::ObjScene scene = sceneAt({0x83, 0x88, 0x82, 0xFF}, 5, 5);
        runToEnd(scene);
        CHECK(scene.npc().tileX() == 4 && scene.npc().tileY() == 3 && scene.npc().facing == ff6::Down);
    }
    {  // diagonal steps move one tile along both axes
        test::ObjScene scene = sceneAt({0xA0, 0xFF}, 3, 3);
        runToEnd(scene);
        CHECK(scene.npc().tileX() == 4 && scene.npc().tileY() == 2 && scene.npc().facing == ff6::Right);
        test::ObjScene other = sceneAt({0xA2, 0xFF}, 3, 3);
        runToEnd(other);
        CHECK(other.npc().tileX() == 2 && other.npc().tileY() == 4 && other.npc().facing == ff6::Left);
    }
    {  // a speed command: the fastest speed crosses a tile in a couple of frames
        test::ObjScene scene = sceneAt({0xC5, 0x81, 0xFF}, 2, 2);
        const int frames = runToEnd(scene);
        CHECK(scene.npc().tileX() == 3 && scene.npc().speed == 5 && frames <= 14);
    }
    {  // frames: a command below 0x80 shows that frame, and walking without animation keeps it
        test::ObjScene scene = sceneAt({0x03, 0xC7, 0x81, 0xFF}, 2, 2);
        scene.objects.startScript(scene.npc(), 0);
        bool kept = true;
        for (int i = 0; i < 60; ++i) {
            scene.run(1);
            if (i >= 4 && scene.npc().frame != 3) kept = false;
        }
        CHECK(kept && !scene.npc().walkAnim && scene.npc().tileX() == 3);
        test::ObjScene animated = sceneAt({0xC7, 0xC6, 0x81, 0xFF}, 2, 2);
        animated.objects.startScript(animated.npc(), 0);
        bool changed = false;
        for (int i = 0; i < 60; ++i) {
            animated.run(1);
            if (!animated.npc().aligned() && animated.npc().frame != ff6::stopFrame(animated.npc().facing)) changed = true;
        }
        CHECK(changed && animated.npc().walkAnim);
    }
    {  // a pause lasts the given number of actions, one every 4 frames
        test::ObjScene scene = sceneAt({0xE0, 0x03, 0xCE, 0xFF}, 2, 2, ff6::Up);
        scene.objects.startScript(scene.npc(), 0);
        int turnedAt = -1;
        for (int i = 1; i <= 60 && turnedAt < 0; ++i) {
            scene.run(1);
            if (scene.npc().facing == ff6::Down) turnedAt = i;
        }
        CHECK(turnedAt >= 16 && turnedAt <= 24);
    }
    {  // event bits of the three groups of 256, set and clear
        test::ObjScene scene = sceneAt({0xE1, 0x05, 0xE2, 0x07, 0xE3, 0x09, 0xE5, 0x07, 0xE4, 0x0A, 0xFF}, 2, 2);
        scene.bits.set(0x0A, true);
        runToEnd(scene);
        CHECK(scene.bits.get(5) && !scene.bits.get(0x107) && scene.bits.get(0x209) && !scene.bits.get(0x0A));
        test::ObjScene other = sceneAt({0xE2, 0x07, 0xE6, 0x03, 0xFF}, 2, 2);
        other.bits.set(0x203, true);
        runToEnd(other);
        CHECK(other.bits.get(0x107) && !other.bits.get(0x203));
    }
    {  // branches are relative to the command that holds them
        test::ObjScene forward = sceneAt({0xFD, 0x04, 0xCE, 0xFF, 0xCD, 0xFF}, 2, 2);  // skips the turn down
        runToEnd(forward);
        CHECK(forward.npc().facing == ff6::Right);
        test::ObjScene backward = sceneAt({0xFD, 0x04, 0xCD, 0xFF, 0xFC, 0x02}, 2, 2);  // back to the turn right
        runToEnd(backward);
        CHECK(backward.npc().facing == ff6::Right);
        test::ObjScene random = sceneAt({0xFB, 0x03, 0xFF, 0xFF}, 2, 2);  // both outcomes reach an end
        CHECK(runToEnd(random) < 40);
    }
    {  // an object holds its tile: the walker waits for it, and goes on when it is gone or may pass through
        test::ObjScene scene({0x81, 0xFF}, {test::npcBytes(2, 2, 0, 1, 0, ff6::Up), test::npcBytes(3, 2, 1, 1, 0, ff6::Up)});
        scene.objects.startScript(scene.npc(), 0);
        scene.run(100);
        CHECK(scene.npc().tileX() == 2 && scene.npc().movement == FF6_OBJ_MOVE_SCRIPT && scene.npc().steps == 1);
        CHECK(scene.npc().speedX == 0);
        scene.npc(1).visible = false;
        scene.run(40);
        CHECK(scene.npc().tileX() == 3 && scene.npc().movement == FF6_OBJ_MOVE_NONE);
        test::ObjScene ghost({0x81, 0xFF}, {test::npcBytes(2, 2, 0, 1, 0, ff6::Up), test::npcBytes(3, 2, 1, 1, 0, ff6::Up)});
        ghost.npc().passThrough = true;
        ghost.objects.startScript(ghost.npc(), 0);
        ghost.run(60);
        CHECK(ghost.npc().tileX() == 3 && ghost.npc().movement == FF6_OBJ_MOVE_NONE);
        // The party holds its tile too.
        test::ObjScene party = sceneAt({0x81, 0xFF}, 2, 2);
        party.placeCharacter(0, 3, 2);
        party.objects.startScript(party.npc(), 0);
        party.run(60);
        CHECK(party.npc().tileX() == 2);
    }
    {  // show, hide and the position
        test::ObjScene hide = sceneAt({0xD1, 0xFF}, 2, 2);
        runToEnd(hide);
        CHECK(!hide.npc().visible && hide.npc().enabled && !hide.objects.occupied(2, 2));
        test::ObjScene show = sceneAt({0xD0, 0xFF}, 2, 2);
        show.npc().visible = false;
        runToEnd(show);
        CHECK(show.npc().visible);
        test::ObjScene place = sceneAt({0xD5, 0x07, 0x09, 0xCE, 0xFF}, 2, 2);
        runToEnd(place);
        CHECK(place.npc().posX == 7 * kTile && place.npc().posY == 9 * kTile && place.npc().facing == ff6::Down);
    }
    {  // a jump lifts the sprite and comes down again
        test::ObjScene scene = sceneAt({0xDC, 0xE0, 0x08, 0xFF}, 2, 2);
        scene.objects.startScript(scene.npc(), 0);
        int highest = 0;
        for (int i = 0; i < 60; ++i) {
            scene.run(1);
            highest = std::max(highest, scene.npc().jumpHeight);
        }
        CHECK(highest == 0x0B && scene.npc().jumpHeight == 0 && scene.npc().jump == 0);
        test::ObjScene high = sceneAt({0xDD, 0xE0, 0x10, 0xFF}, 2, 2);
        high.objects.startScript(high.npc(), 0);
        int top = 0;
        for (int i = 0; i < 80; ++i) {
            high.run(1);
            top = std::max(top, high.npc().jumpHeight);
        }
        CHECK(top == 0x29);
    }
    {  // a script may start an event, once none is running
        test::ObjScene scene = sceneAt({0xF9, 0x34, 0x12, 0x00, 0xFF}, 2, 2);
        scene.objects.startScript(scene.npc(), 0);
        scene.run(20, true);
        uint32_t offset = 0;
        CHECK(!scene.objects.takeEventRequest(&offset) && scene.npc().movement == FF6_OBJ_MOVE_SCRIPT);
        scene.run(8, false);
        CHECK(scene.objects.takeEventRequest(&offset) && offset == 0x1234);
        CHECK(!scene.objects.takeEventRequest(&offset));
        scene.run(8, false);
        CHECK(scene.npc().movement == FF6_OBJ_MOVE_NONE);
    }
    {  // sprite priority, and commands that are only skipped
        test::ObjScene scene = sceneAt({0xC8, 0x02, 0xC9, 0x20, 0xFF}, 2, 2);
        runToEnd(scene);
        CHECK(scene.npc().priority == FF6_OBJ_PRIORITY_ABOVE && scene.npc().topIsAbove() && scene.npc().bottomIsAbove());
        scene.npc().priority = FF6_OBJ_PRIORITY_TOP_ABOVE;
        CHECK(scene.npc().topIsAbove() && !scene.npc().bottomIsAbove());
        scene.npc().priority = FF6_OBJ_PRIORITY_BELOW;
        scene.npc().topAbove = scene.npc().bottomAbove = true;
        CHECK(!scene.npc().topIsAbove() && !scene.npc().bottomIsAbove());
    }
    {  // an unknown command or the end of the script stops the npc instead of running off
        test::ObjScene unknown = sceneAt({0xB5, 0xFF}, 2, 2);
        runToEnd(unknown);
        CHECK(unknown.npc().movement == FF6_OBJ_MOVE_NONE && unknown.npc().scriptPtr == -1);
        test::ObjScene cut = sceneAt({0xCC}, 2, 2);  // no end command
        runToEnd(cut);
        CHECK(cut.npc().movement == FF6_OBJ_MOVE_NONE);
    }
    {  // a loop that never waits does not freeze the game
        test::ObjScene scene = sceneAt({0xFC, 0x00}, 2, 2);
        scene.objects.startScript(scene.npc(), 0);
        scene.run(10);
        CHECK(scene.npc().movement == FF6_OBJ_MOVE_SCRIPT);
    }
    {  // only the first 22 objects of the list get a turn: the party and the camera take the first two places
        std::vector<std::vector<uint8_t>> npcs;
        for (int i = 0; i < 24; ++i) npcs.push_back(test::npcBytes((i / 16) * 8, i % 16, i, 1, 0, ff6::Up));
        test::ObjScene scene({0x81, 0xFF}, npcs);
        for (ff6::Npc& npc : scene.objects.npcs()) scene.objects.startScript(npc, 0);
        scene.run(100);
        int moved = 0;
        for (size_t i = 0; i < 24; ++i) {
            if (scene.npc(i).tileX() == static_cast<int>(i / 16) * 8 + 1) ++moved;
        }
        CHECK(moved == 22);
        CHECK(scene.npc(21).tileX() == 1 + 8 * (21 / 16) && scene.npc(22).tileX() == 8 * (22 / 16));
    }
}
