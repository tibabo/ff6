#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/obj.hpp"
#include "field/player.hpp"
#include "field/sprite_data.hpp"

namespace {

constexpr int kTile = FF6_OBJ_TILE_SUBPIXELS;

}  // namespace

void testObj() {
    std::vector<uint8_t> bytes = test::npcBytes(8, 38, 768, 54, 1, ff6::Down);
    bytes[8] |= 2 << FF6_NPC_LAYER_PRIORITY_SHIFT;
    const ff6::NpcProp prop = ff6::decodeNpcProp(bytes.data());
    CHECK(prop.eventOffset == 0x2D2A7 && prop.palette == 1 && prop.switchId == 768);
    CHECK(prop.x == 8 && prop.y == 38 && prop.gfx == 54 && prop.facing == ff6::Down);
    CHECK(!prop.special && !prop.noTurn && prop.animFrame == 0 && prop.layerPriority == FF6_OBJ_PRIORITY_ABOVE);
    CHECK(prop.speed == FF6_OBJ_DEFAULT_SPEED && prop.movement == FF6_OBJ_MOVE_NONE && prop.vehicle == 0);
    bytes[7] |= 0x80;  // the two top bits of the movement byte are the vehicle: 2 is the Magitek armor
    CHECK(ff6::decodeNpcProp(bytes.data()).vehicle == FF6_VEHICLE_MAGITEK);
    bytes[7] = 0x40;
    CHECK(ff6::decodeNpcProp(bytes.data()).vehicle == FF6_VEHICLE_CHOCOBO);

    // Every property item is an object, numbered from 0x10: also one that its switch hides, and one with its own graphics.
    ff6::MapData map = test::makeMap();
    for (const std::vector<uint8_t>& npc : {test::npcBytes(3, 3, 5, 1, 0, ff6::Up), test::npcBytes(6, 6, 6, 2, 0, ff6::Up)}) {
        map.npcProps.insert(map.npcProps.end(), npc.begin(), npc.end());
    }
    std::vector<uint8_t> special = test::npcBytes(9, 9, 5, 3, 0, ff6::Up);
    special[4] |= FF6_NPC_SPECIAL_FLAG;
    map.npcProps.insert(map.npcProps.end(), special.begin(), special.end());
    ff6::EventBits bits;
    bits.init(test::makeCommon());
    bits.set(FF6_NPC_SWITCH_EVENT_BIT + 5, true);
    ff6::Objects objects;
    objects.init(map, bits);
    CHECK(objects.npcs().size() == 3);
    CHECK(objects.npcByObject(0x10) == &objects.npcs()[0] && objects.npcByObject(0x12) == &objects.npcs()[2]);
    CHECK(objects.npcByObject(0x13) == nullptr && objects.npcByObject(0x0F) == nullptr);
    CHECK(objects.npcs()[0].visible && !objects.npcs()[1].enabled && !objects.npcs()[1].visible);
    CHECK(objects.occupied(3, 3) && !objects.occupied(6, 6) && !objects.occupied(9, 9));
    {  // InitNPCMap: an npc of high sprite priority (a save point) is drawn but the party walks onto it and cannot talk to it
        ff6::MapData points = test::makeMap();
        std::vector<uint8_t> point = test::npcBytes(4, 4, 5, 1, 0, ff6::Up);
        point[7] |= FF6_NPC_ORDER_HIGH << FF6_NPC_ORDER_SHIFT;
        points.npcProps.insert(points.npcProps.end(), point.begin(), point.end());
        ff6::Objects field;
        field.init(points, bits);
        CHECK(field.npcs()[0].blocks() && !field.npcs()[0].solid() && !field.occupied(4, 4));
    }
    CHECK(objects.npcs()[0].frame == ff6::stopFrame(ff6::Up) && objects.npcs()[0].posX == 3 * kTile);

    // The party faces the npc, also across a counter tile but not across a wall.
    CHECK(objects.npcFacing(map, 3, 4, ff6::Up) == &objects.npcs()[0]);
    CHECK(objects.npcFacing(map, 4, 3, ff6::Up) == nullptr);
    test::setProp(map, 3, 4, 0x0F07);  // counter
    CHECK(objects.npcFacing(map, 3, 5, ff6::Up) == &objects.npcs()[0]);
    test::setProp(map, 3, 4, 0xFFF7);  // wall
    CHECK(objects.npcFacing(map, 3, 5, ff6::Up) == nullptr);

    // An npc blocks the party.
    test::setProp(map, 3, 4, 0x0F01);
    ff6::Player player;
    player.place(map, 3, 4, ff6::Up);
    CHECK(!player.update(map, ff6::Up, objects));
    CHECK(player.update(map, ff6::Right, objects));

    {  // an npc without movement stays where it is
        test::ObjScene scene({}, {test::npcBytes(8, 8, 0, 1, 0, ff6::Down)});
        scene.run(300);
        CHECK(scene.npc().posX == 8 * kTile && scene.npc().posY == 8 * kTile);
    }
    {  // a wandering npc walks from tile to tile: speeds are set exactly while it is between tiles
        test::ObjScene scene({}, {test::npcBytes(8, 8, 0, 1, 0, ff6::Down, FF6_OBJ_MOVE_RANDOM)});
        bool movedBetweenTiles = false, walkFrame = false, consistent = true;
        for (int i = 0; i < 600; ++i) {
            scene.run(1);
            const ff6::Npc& npc = scene.npc();
            const bool stopped = npc.speedX == 0 && npc.speedY == 0;
            if (npc.aligned() != stopped) consistent = false;
            if (!npc.aligned()) {
                movedBetweenTiles = true;
                if (npc.frame != ff6::stopFrame(npc.facing)) walkFrame = true;
                // The npc holds the tile that it leaves and the one that it walks onto.
                if (!scene.objects.occupied(npc.tileX(), npc.tileY())) consistent = false;
            }
        }
        CHECK(consistent && movedBetweenTiles && walkFrame);
    }
    {  // a wandering npc never walks onto its neighbour or onto the party
        test::ObjScene scene({}, {test::npcBytes(4, 4, 0, 1, 0, ff6::Down, FF6_OBJ_MOVE_RANDOM),
                                  test::npcBytes(5, 4, 1, 1, 0, ff6::Down)});
        scene.placeCharacter(0, 4, 5);
        bool trespassed = false, moved = false;
        for (int i = 0; i < 800; ++i) {
            scene.run(1);
            const ff6::Npc& npc = scene.npc();
            const int width = npc.posX % kTile != 0 ? 2 : 1, height = npc.posY % kTile != 0 ? 2 : 1;
            for (int dx = 0; dx < width; ++dx) {
                for (int dy = 0; dy < height; ++dy) {
                    const int x = npc.tileX() + dx, y = npc.tileY() + dy;
                    if ((x == 5 && y == 4) || (x == 4 && y == 5)) trespassed = true;
                }
            }
            if (npc.posX != 4 * kTile || npc.posY != 4 * kTile) moved = true;
        }
        CHECK(!trespassed && moved);
        CHECK(scene.npc(1).posX == 5 * kTile && scene.npc(1).posY == 4 * kTile);  // the other one never moves
    }
    {  // talking stops an npc until the event is over, then it goes back to what it did
        test::ObjScene scene({}, {test::npcBytes(8, 8, 0, 1, 0, ff6::Down, FF6_OBJ_MOVE_RANDOM)});
        ff6::Npc& npc = scene.npc();
        scene.objects.activate(npc, ff6::Right);
        CHECK(npc.facing == ff6::Left && npc.frame == ff6::stopFrame(ff6::Left) && npc.movement == FF6_OBJ_MOVE_ACTIVATED);
        scene.run(100, true);
        CHECK(npc.posX == 8 * kTile && npc.posY == 8 * kTile && npc.movement == FF6_OBJ_MOVE_ACTIVATED);
        scene.run(8, false);
        CHECK(npc.movement == FF6_OBJ_MOVE_RANDOM);
        // An npc that does not turn keeps its facing.
        std::vector<uint8_t> stubborn = test::npcBytes(2, 2, 0, 1, 0, ff6::Up);
        stubborn[8] |= FF6_NPC_NO_TURN;
        test::ObjScene other({}, {stubborn});
        other.objects.activate(other.npc(), ff6::Right);
        CHECK(other.npc().facing == ff6::Up);
    }
    {  // the speed of an npc decides how fast it crosses a tile
        for (int speed = 0; speed <= 3; ++speed) {
            test::ObjScene scene({0x81, 0xFF}, {test::npcBytes(2, 2, 0, 1, 0, ff6::Down, 0, speed)});  // one tile right
            scene.objects.startScript(scene.npc(), 0);
            int frames = 0;
            while (scene.npc().movement != FF6_OBJ_MOVE_NONE && frames < 400) {
                scene.run(1);
                ++frames;
            }
            const int crossing = (FF6_TILE_SIZE * 4) >> speed;  // 0.25, 0.5, 1 and 2 pixels per frame
            CHECK(scene.npc().tileX() == 3 && scene.npc().tileY() == 2 && frames >= crossing && frames <= crossing + 10);
        }
    }
    {  // commands change the npc: delete, create, show, hide, graphics, palette, event, passability
        test::ObjScene scene({}, {test::npcBytes(2, 2, 0, 1, 0, ff6::Down)});
        ff6::Npc& npc = scene.npc();
        scene.objects.command(FF6_EVENT_OBJ_HIDE, npc, 0);
        CHECK(npc.enabled && !npc.visible && !scene.objects.occupied(2, 2));
        scene.objects.command(FF6_EVENT_OBJ_SHOW, npc, 0);
        CHECK(npc.visible && scene.objects.occupied(2, 2));
        scene.objects.command(FF6_EVENT_OBJ_DELETE, npc, 0);
        CHECK(!npc.enabled && !npc.visible && !scene.objects.occupied(2, 2));
        scene.objects.command(FF6_EVENT_OBJ_SHOW, npc, 0);  // a deleted object cannot be shown
        CHECK(!npc.visible);
        scene.objects.command(FF6_EVENT_OBJ_CREATE, npc, 0);
        CHECK(npc.enabled && !npc.visible);
        scene.objects.command(FF6_EVENT_OBJ_SHOW, npc, 0);
        scene.objects.command(FF6_EVENT_OBJ_GFX, npc, 9);
        scene.objects.command(FF6_EVENT_OBJ_PAL, npc, 12);
        scene.objects.command(FF6_EVENT_OBJ_EVENT, npc, 0x1234);
        scene.objects.command(FF6_EVENT_OBJ_PASS_ON, npc, 0);
        CHECK(npc.prop.gfx == 9 && npc.prop.palette == 4 && npc.eventOffset == 0x1234 && npc.passThrough);
        scene.objects.command(FF6_EVENT_OBJ_VEHICLE, npc, FF6_VEHICLE_RAFT | FF6_VEHICLE_RIDER);
        CHECK(npc.vehicle == (FF6_VEHICLE_RAFT | FF6_VEHICLE_RIDER));
        scene.objects.command(FF6_EVENT_OBJ_VEHICLE, npc, FF6_VEHICLE_MAGITEK);  // the event sets the type and the rider
        CHECK(npc.vehicle == FF6_VEHICLE_MAGITEK);
        scene.objects.command(FF6_EVENT_OBJ_PASS_OFF, npc, 0);
        CHECK(!npc.passThrough);
    }
}
