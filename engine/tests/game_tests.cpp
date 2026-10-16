#include <cstdlib>
#include <utility>
#include <vector>

#include "check.hpp"
#include "test_helpers.hpp"
#include "tests.hpp"
#include "game.hpp"

// Plays the real data pack when FF6_PACK points to one (built by `node ff6.mjs build-pack`).
void testGame() {
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    test::RecordingRenderer renderer;
    ff6::Game game(ff6::loadCommonData(pack), pack, 0x1C, renderer);  // the Narshe inn
    CHECK(game.map().id == 0x1C && game.player().tileX() == 8 && game.player().tileY() == 45);
    CHECK(game.objects().npcs().size() == 1);  // the innkeeper

    game.draw();
    CHECK(renderer.frames == 1 && !renderer.drewOutsideFrame);
    // The field is composed on the CPU and shown as one full-screen image.
    CHECK(renderer.draws.size() == 1 && renderer.draws.front().destination.w == FF6_SCREEN_WIDTH);
    CHECK(renderer.updates.size() == 1);

    // Walk up to the counter and talk to the innkeeper: a dialogue with two choices opens.
    const ff6::InputState up{ff6::Up, false}, idle{FF6_NO_DIRECTION, false}, confirm{FF6_NO_DIRECTION, true};
    for (int tile = 0; tile < 5; ++tile) {
        game.tick(up);
        for (int frame = 1; frame < FF6_STEP_FRAMES; ++frame) game.tick(idle);
    }
    CHECK(game.player().tileY() == 40 && !game.dialogue().active());
    game.tick(confirm);
    for (int i = 0; i < 10; ++i) game.tick(idle);
    CHECK(game.dialogue().active());
    for (int i = 0; i < 400 && !game.dialogue().choosing(); ++i) game.tick(idle);
    CHECK(game.dialogue().choosing() && game.dialogue().choiceMarks().size() == 2);
    game.draw();
    CHECK(renderer.draws.size() == 2);  // the dialogue window is drawn over the field

    // The cursor moves and the last choice ends the conversation.
    game.tick({ff6::Down, false});
    CHECK(game.dialogue().cursor() == 1);
    game.tick(idle);
    game.tick(confirm);
    for (int i = 0; i < 10; ++i) game.tick(idle);
    CHECK(!game.dialogue().active());

    // Leaving through the door loads another map and puts the party there.
    ff6::Game outside(ff6::loadCommonData(pack), pack, 0x1C, renderer);
    const size_t before = renderer.textures.size();
    for (int tile = 0; tile < 2 && outside.map().id == 0x1C; ++tile) {
        outside.tick({ff6::Down, false});
        for (int frame = 1; frame < FF6_STEP_FRAMES; ++frame) outside.tick(idle);
    }
    CHECK(outside.map().id != 0x1C && renderer.textures.size() >= before);

    // The npcs of the Moogle cave wander about by themselves, whatever the party does.
    ff6::Game cave(ff6::loadCommonData(pack), pack, 52, renderer);
    std::vector<std::pair<int, int>> start;
    for (const ff6::Npc& npc : cave.objects().npcs()) start.push_back({npc.pixelX(), npc.pixelY()});
    for (int i = 0; i < 600; ++i) cave.tick(idle);
    int moved = 0;
    for (size_t i = 0; i < start.size(); ++i) {
        const ff6::Npc& npc = cave.objects().npcs()[i];
        if (npc.enabled && (npc.pixelX() != start[i].first || npc.pixelY() != start[i].second)) ++moved;
    }
    CHECK(moved >= 5);
    cave.draw();  // the moving sprites are drawn
}
