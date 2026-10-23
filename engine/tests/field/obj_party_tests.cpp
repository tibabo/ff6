#include <array>
#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/obj.hpp"

namespace {

constexpr int kTile = FF6_OBJ_TILE_SUBPIXELS;

}  // namespace

void testObjParty() {
    {  // a new game: Terra leads the party, the object numbers lead to the right objects
        test::ObjScene scene({}, {test::npcBytes(4, 4, 0, 1, 0, ff6::Down)});
        scene.objects.resetCharacters();
        ff6::Objects& objects = scene.objects;
        CHECK(objects.lead() == &objects.characters()[0] && objects.activeParty() == 1);
        CHECK(objects.object(0) == &objects.characters()[0] && objects.object(FF6_OBJ_PARTY_FIRST) == objects.lead());
        CHECK(objects.object(FF6_OBJ_PARTY_FIRST + 1) == nullptr);  // an empty place of the party
        CHECK(objects.object(FF6_OBJ_CAMERA) == &objects.camera());
        CHECK(objects.object(FF6_FIRST_NPC_OBJECT) == &scene.npc() && objects.object(0x35) == nullptr);
        CHECK(objects.characters()[5].prop.gfx == 5 && !objects.characters()[5].enabled);
        objects.enterMap(objects.mapId(), 6, 7, ff6::Left);
        const ff6::Npc& lead = *objects.lead();
        CHECK(lead.posX == 6 * kTile && lead.posY == 7 * kTile && lead.facing == ff6::Left && lead.visible);
        CHECK(lead.movement == FF6_OBJ_MOVE_USER && objects.occupied(6, 7) && !objects.occupied(5, 7));
    }
    {  // the party grows, the first character shows and the others hide on its tile
        test::ObjScene scene({}, {});
        ff6::Objects& objects = scene.objects;
        objects.resetCharacters();
        objects.enterMap(objects.mapId(), 3, 3, ff6::Down);
        objects.addToParty(1, 1);
        objects.addToParty(2, 1);
        objects.addToParty(5, 2);  // another party
        CHECK(objects.partySlots()[0] == 0 && objects.partySlots()[1] == 1 && objects.partySlots()[2] == 2);
        CHECK(objects.partySlots()[3] == -1);
        CHECK(objects.object(FF6_OBJ_PARTY_FIRST + 1) == &objects.characters()[1]);
        objects.characters()[1].enabled = true;
        objects.updateParty();
        CHECK(objects.characters()[1].posX == 3 * kTile && !objects.characters()[1].visible);
        CHECK(objects.characters()[0].visible && objects.characters()[1].map == objects.mapId());
        CHECK(objects.characters()[5].map == -1);  // not in the active party
        // The other party takes over; its first character leads.
        objects.addToParty(0, 0);
        objects.activateParty(2);
        CHECK(objects.lead() == &objects.characters()[5] && objects.lead()->visible);
        CHECK(objects.lead()->posX == 3 * kTile);
    }
    {  // the places of the party are set by the event, and the others leave it
        test::ObjScene scene({}, {});
        ff6::Objects& objects = scene.objects;
        objects.resetCharacters();
        objects.addToParty(1, 1);
        objects.setPartySlots({4, 1, -1, -1});
        CHECK(objects.lead() == &objects.characters()[4] && objects.characters()[0].party == 0);
        CHECK(objects.characters()[4].party == 1 && objects.characters()[1].partySlot == 1);
        objects.setPartyMap(1, 77);
        CHECK(objects.characters()[1].map == 77 && objects.characters()[4].map == 77 && objects.characters()[0].map == -1);
    }
    {  // creating a character puts it on the current map
        test::ObjScene scene({}, {});
        scene.objects.resetCharacters();
        scene.objects.command(FF6_EVENT_OBJ_CREATE, scene.objects.characters()[3], 0);
        CHECK(scene.objects.characters()[3].enabled && scene.objects.characters()[3].map == scene.objects.mapId());
    }
    {  // a character that is not in the party runs its own script, and holds its tile
        test::ObjScene scene({0x85, 0xFF}, {});
        ff6::Npc& character = scene.placeCharacter(3, 2, 2);
        scene.objects.startScript(character, 0);
        scene.run(100);
        CHECK(character.tileX() == 4 && character.tileY() == 2 && character.movement == FF6_OBJ_MOVE_NONE);
        CHECK(scene.objects.occupied(4, 2));
        test::ObjScene other({0x85, 0xFF}, {});
        ff6::Npc& walker = other.placeCharacter(3, 2, 2);
        other.placeCharacter(4, 3, 2);
        other.objects.startScript(walker, 0);
        other.run(100);
        CHECK(walker.tileX() == 2);  // the other character is in the way
    }
    {  // the camera goes through everything and scrolls back to the party
        test::ObjScene scene({0x85, 0xD7, 0xFF}, {test::npcBytes(3, 2, 0, 1, 0, ff6::Down)});
        scene.objects.resetCharacters();
        scene.objects.enterMap(scene.objects.mapId(), 9, 8, ff6::Down);
        ff6::Npc& camera = scene.objects.camera();
        camera.posX = 2 * kTile;
        camera.posY = 2 * kTile;
        scene.objects.startScript(camera, 0);
        scene.run(30);
        CHECK(camera.movement == FF6_OBJ_MOVE_SCRIPT && camera.tileX() > 2);  // it walks over the npc
        scene.run(60);
        CHECK(camera.movement == FF6_OBJ_MOVE_NONE && camera.posX == 9 * kTile && camera.posY == 8 * kTile);
    }
    {  // the party leaves a slot to the camera: npcs after it keep their place in the list
        test::ObjScene scene({0x85, 0xFF}, {test::npcBytes(2, 2, 0, 1, 0, ff6::Up)});
        scene.objects.resetCharacters();
        scene.objects.enterMap(scene.objects.mapId(), 8, 8, ff6::Down);
        scene.objects.startScript(scene.npc(), 0);
        scene.run(100);
        CHECK(scene.npc().tileX() == 4);
    }
    {  // a game can start with a party of its own: the characters exist, the first one leads and shows
        test::ObjScene scene({}, {});
        ff6::Objects& objects = scene.objects;
        objects.resetCharacters();
        objects.startParty({4, 1, 5, 13, 2});  // a fifth character does not fit
        objects.enterMap(objects.mapId(), 6, 7, ff6::Right);
        CHECK(objects.lead() == &objects.characters()[4] && objects.lead()->visible && objects.lead()->posX == 6 * kTile);
        CHECK((objects.partySlots() == std::array<int, FF6_PARTY_SIZE>({4, 1, 5, 13})));
        CHECK(objects.characters()[0].party == 0 && !objects.characters()[0].enabled && objects.characters()[2].party == 0);
        for (int object = FF6_OBJ_PARTY_FIRST; object < FF6_OBJ_PARTY_FIRST + FF6_PARTY_SIZE; ++object) {
            CHECK(objects.object(object) != nullptr && objects.acts(*objects.object(object)));
        }
        CHECK(!objects.characters()[1].visible && objects.characters()[1].posX == 6 * kTile);  // they wait on its tile
    }
    {  // each character has the sprite palette that the events give it
        test::ObjScene scene({}, {});
        scene.objects.resetCharacters();
        const auto& characters = scene.objects.characters();
        CHECK(characters[0].prop.palette == 2 && characters[1].prop.palette == 1 && characters[4].prop.palette == 0);
        CHECK(characters[3].prop.palette == 4 && characters[10].prop.palette == 5 && characters[7].prop.palette == 3);
    }
}
