#include <array>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include "audio/sound_program.hpp"
#include "check.hpp"
#include "test_helpers.hpp"
#include "tests.hpp"
#include "game.hpp"

namespace {

// The real sound program, with a log of what the game sent to it.
struct LoggedSound : ff6::Sound {
    explicit LoggedSound(ff6::SoundProgram& program) : program(program) {}
    ff6::SoundProgram& program;
    std::vector<std::array<int, 3>> commands;
    void command(uint8_t command, uint8_t arg1, uint8_t arg2) override {
        commands.push_back({command, arg1, arg2});
        program.command(command, arg1, arg2);
    }
    uint8_t port(int index) const override { return program.port(index); }
    int previousSong() const override { return program.previousSong(); }
};

}  // namespace

// Plays the real data pack when FF6_PACK points to one (built by `node ff6.mjs build-pack`).
void testGame() {
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    test::RecordingRenderer renderer;
    test::RecordingSound sound;
    ff6::Game game(ff6::loadCommonData(pack), pack, 0x1C, renderer, sound);  // the Narshe inn
    CHECK(game.map().id == 0x1C && game.player().tileX() == 8 && game.player().tileY() == 45);
    CHECK(game.objects().npcs().size() == 1);  // the innkeeper

    game.draw();
    CHECK(renderer.frames == 1 && !renderer.drewOutsideFrame);
    // The field is composed on the CPU and shown as one full-screen image.
    CHECK(renderer.draws.size() == 1 && renderer.draws.front().destination.w == FF6_SCREEN_WIDTH);
    CHECK(renderer.updates.size() == 1);

    // Walk up to the counter and talk to the innkeeper: a dialogue with two choices opens.
    const ff6::InputState up{ff6::Up, false}, idle{FF6_NO_DIRECTION, false}, confirm{FF6_NO_DIRECTION, true};
    // The startup event of Narshe, which the party is in when the game starts, plays the song of the town.
    for (int i = 0; i < 5; ++i) game.tick(idle);
    CHECK(sound.commands.size() == 1 && sound.commands[0].command == FF6_SOUND_PLAY_SONG && sound.commands[0].arg1 == 0x17);
    for (int tile = 0; tile < 5; ++tile) {
        game.tick(up);
        for (int frame = 1; frame < FF6_STEP_FRAMES; ++frame) game.tick(idle);
    }
    CHECK(game.player().tileY() == 40 && !game.dialogue().active());
    CHECK(game.party().steps == 5);  // one for each tile that the party walked
    CHECK(game.party().characters[0].level == FF6_DEFAULT_LEVEL && game.party().gil == 0);
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

    // Leaving through the door fades the screen out, loads another map, and fades in again.
    ff6::Game outside(ff6::loadCommonData(pack), pack, 0x1C, renderer, sound);
    const size_t before = renderer.textures.size();
    for (int i = 0; i < 40; ++i) outside.tick(idle);
    CHECK(outside.brightness() == FF6_BRIGHTNESS_FULL && !outside.fading());  // the screen fades in at the start
    int darkest = FF6_BRIGHTNESS_FULL;
    for (int tile = 0; tile < 2 && outside.map().id == 0x1C; ++tile) {
        outside.tick({ff6::Down, false});
        for (int frame = 1; frame < FF6_STEP_FRAMES; ++frame) outside.tick(idle);
        darkest = std::min(darkest, outside.brightness());
    }
    for (int i = 0; i < 60; ++i) {
        outside.tick(idle);
        darkest = std::min(darkest, outside.brightness());
    }
    CHECK(outside.map().id != 0x1C && renderer.textures.size() >= before);
    CHECK(darkest <= 2 * FF6_BRIGHTNESS_STEP && outside.brightness() == FF6_BRIGHTNESS_FULL);

    // The npcs of the Moogle cave wander about by themselves, whatever the party does.
    ff6::Game cave(ff6::loadCommonData(pack), pack, 52, renderer, sound);
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

    {  // a closed door opens when the party walks into it, and the sound of the door plays
        int doorMap = -1, doorX = 0, doorY = 0;
        for (uint32_t id = FF6_FIRST_FIELD_MAP; id < 200 && doorMap < 0; ++id) {
            if (!ff6::mapExists(pack, id)) continue;
            const ff6::MapData map = ff6::loadMap(pack, id);
            const ff6::MapLayout& layout = map.layers[FF6_BG1];
            for (int y = 2; y + 1 < layout.height && doorMap < 0; ++y) {
                for (int x = 0; x < layout.width; ++x) {
                    const auto at = [&](int cellX, int cellY) { return layout.cells[static_cast<size_t>(cellY) * layout.width + cellX]; };
                    const uint8_t floor = map.propAt(x, y + 1) & FF6_PROP_LOW_MASK;
                    if (at(x, y) == FF6_DOOR_SMALL_1 && at(x, y - 1) == 0x05 && (floor & FF6_TILE_TYPE_MASK) != FF6_TILE_IMPASSABLE) {
                        doorMap = static_cast<int>(id);
                        doorX = x;
                        doorY = y;
                        break;
                    }
                }
            }
        }
        CHECK(doorMap >= 0);
        if (doorMap >= 0) {
            test::RecordingSound doorSound;
            ff6::Game doors(ff6::loadCommonData(pack), pack, static_cast<uint32_t>(doorMap), renderer, doorSound);
            for (int i = 0; i < 600; ++i) doors.tick(idle);  // the startup event is over
            doors.placeParty(doorX, doorY + 1);
            const auto cell = [&](int x, int y) { return doors.map().layers[FF6_BG1].cells[static_cast<size_t>(y) * doors.map().layers[FF6_BG1].width + x]; };
            CHECK(cell(doorX, doorY) == FF6_DOOR_SMALL_1);
            doorSound.commands.clear();
            doors.tick(up);
            CHECK(cell(doorX, doorY - 1) == 0x04 && cell(doorX, doorY) == 0x14);
            CHECK(doors.player().tileY() == doorY + 1);  // the party does not move on that frame
            CHECK(doorSound.commands.size() == 1 && doorSound.commands[0].command == FF6_SOUND_GAME_SFX &&
                  doorSound.commands[0].arg1 == FF6_SFX_DOOR_OPEN);
        }
    }
    // Sleeping at the inn plays another song, and the song of the town comes back once it is over; with Terra alone and
    // with a party of four.
    for (const std::vector<int>& party : {std::vector<int>{}, std::vector<int>{0, 1, 4, 5}}) {
        const ff6::AudioData audio = ff6::loadAudioData(pack);
        ff6::SoundProgram program(audio);
        LoggedSound logged(program);
        ff6::Game inn(ff6::loadCommonData(pack), pack, 0x1C, renderer, logged, party);
        inn.party().gil = 1000;  // the inn takes its price
        for (int object = 0x31; object < 0x35; ++object) {
            CHECK((inn.objects().object(object) != nullptr) == (party.size() == 4 || object == 0x31));
        }
        std::array<int16_t, 2 * 1024> samples{};
        bool doorOpened = false;
        const auto step = [&](const ff6::InputState& input) {
            inn.tick(input);
            for (int i = 0; i < 9; ++i) program.render(samples.data(), samples.size() / 2);  // about a 60th of a second
            const ff6::MapLayout& layout = inn.map().layers[FF6_BG1];
            if (inn.map().id == 0x1C && layout.cells[static_cast<size_t>(0x24) * layout.width + 4] == 0x04) doorOpened = true;
        };
        for (int tile = 0; tile < 5; ++tile) {
            step(up);
            for (int frame = 1; frame < FF6_STEP_FRAMES; ++frame) step(idle);
        }
        step(confirm);
        for (int i = 0; i < 400 && !inn.dialogue().choosing(); ++i) step(idle);
        step(confirm);  // the first choice: stay the night
        for (int i = 0; i < 20000; ++i) step(i % 40 == 0 ? confirm : idle);
        const auto& commands = logged.commands;
        const std::vector<std::array<int, 3>> expected = {
            {FF6_SOUND_PLAY_SONG, 0x17, 0xFF}, {FF6_SOUND_SET_SONG_VOLUME, 0x60, 0x00},
            {FF6_SOUND_PAUSE_SONG, 0x38, 0xFF}, {FF6_SOUND_PLAY_SONG, 0x17, 0x00}, {FF6_SOUND_SET_SONG_VOLUME, 0x20, 0xFF}};
        CHECK(commands == expected);  // the town song, a fade out, Nighty Night, then the town song fading in
        CHECK(doorOpened);  // the event opens the door of the room, with the tiles of the open door
        CHECK(inn.party().gil < 1000 && inn.party().gil > 0);
        CHECK(program.currentSong() == 0x17);
    }

    {  // the command that moves the party loads the same map again: the party is elsewhere and the screen fades in
        ff6::CommonData data = ff6::loadCommonData(pack);
        const uint32_t startup = ff6::loadMap(pack, 0x1C).startupEvent;
        const std::vector<uint8_t> script = {FF6_EVENT_MOVE_PARTY, 6, 44, 0xFE};
        std::copy(script.begin(), script.end(), data.eventScript.begin() + startup);
        ff6::Game moved(std::move(data), pack, 0x1C, renderer, sound);
        for (int i = 0; i < 60; ++i) moved.tick(idle);
        CHECK(moved.map().id == 0x1C && moved.player().tileX() == 6 && moved.player().tileY() == 44);
        CHECK(moved.objects().npcs().size() == 1 && moved.brightness() == FF6_BRIGHTNESS_FULL);
        moved.tick({ff6::Right, false});  // the party is in the hands of the player again
        CHECK(moved.player().moving());
    }

    {  // the shop command: the screen goes to the shop, the party buys something, and the field comes back
        ff6::CommonData data = ff6::loadCommonData(pack);
        const uint32_t startup = ff6::loadMap(pack, 0x1C).startupEvent;
        const std::vector<uint8_t> script = {FF6_EVENT_SHOP, 0, 0xFE};  // the weapons of Narshe
        std::copy(script.begin(), script.end(), data.eventScript.begin() + startup);
        ff6::Game shopper(std::move(data), pack, 0x1C, renderer, sound);
        shopper.party().gil = 5000;
        for (int i = 0; i < 300 && !(shopper.menuPhase() == ff6::MenuPhase::Open && !shopper.fading()); ++i) shopper.tick(idle);
        CHECK(shopper.shopOpen() && shopper.shop().mode() == ff6::ShopMode::Options);
        shopper.draw();
        const int first = ff6::shopStock(shopper.rules(), 0)[0];
        const int price = ff6::shopPrice(shopper.rules(), 0, first, 0);
        shopper.tick(confirm);  // buy
        shopper.tick(idle);
        shopper.tick(confirm);  // the first weapon
        shopper.tick(idle);
        shopper.tick(confirm);  // one of it
        shopper.tick(idle);
        CHECK(shopper.party().gil == 5000 - price && ff6::itemQuantity(shopper.party(), first) == 1);
        ff6::InputState back;
        back.cancel = true;
        shopper.tick(back);  // to the options
        shopper.tick(idle);
        shopper.tick({ff6::Right, false});
        shopper.tick(idle);
        shopper.tick({ff6::Right, false});
        shopper.tick(idle);
        shopper.tick(confirm);  // exit
        CHECK(shopper.shop().closed());
        for (int i = 0; i < 300 && shopper.menuPhase() != ff6::MenuPhase::Closed; ++i) shopper.tick(idle);
        for (int i = 0; i < 40; ++i) shopper.tick(idle);
        CHECK(!shopper.shopOpen() && shopper.menuPhase() == ff6::MenuPhase::Closed && shopper.brightness() == FF6_BRIGHTNESS_FULL);
    }

    {  // the menu button fades the field out and the menu in; the cancel button brings the field back
        ff6::Game menuGame(ff6::loadCommonData(pack), pack, 0x1C, renderer, sound, {0, 1});
        for (int i = 0; i < 80; ++i) menuGame.tick(idle);
        CHECK(menuGame.menuPhase() == ff6::MenuPhase::Closed && menuGame.brightness() == FF6_BRIGHTNESS_FULL);
        const int tileY = menuGame.player().tileY();
        ff6::InputState press;
        press.menu = true;
        menuGame.tick(press);
        CHECK(menuGame.menuPhase() == ff6::MenuPhase::FadingOut);
        for (int i = 0; i < 100 && menuGame.menuPhase() != ff6::MenuPhase::Open; ++i) menuGame.tick(idle);
        CHECK(menuGame.menuPhase() == ff6::MenuPhase::Open);
        for (int i = 0; i < 30; ++i) menuGame.tick(idle);
        CHECK(menuGame.brightness() == FF6_BRIGHTNESS_FULL && !menuGame.menu().closed());
        const size_t frames = renderer.frames;
        menuGame.draw();
        CHECK(renderer.frames == frames + 1);
        menuGame.tick({ff6::Down, false});  // the party does not walk while the menu is open
        ff6::InputState cancel;
        cancel.cancel = true;
        menuGame.tick(cancel);
        CHECK(menuGame.menu().closed() && menuGame.menuPhase() == ff6::MenuPhase::Leaving);
        for (int i = 0; i < 100 && menuGame.menuPhase() != ff6::MenuPhase::Closed; ++i) menuGame.tick(idle);
        for (int i = 0; i < 30; ++i) menuGame.tick(idle);
        CHECK(menuGame.menuPhase() == ff6::MenuPhase::Closed && menuGame.brightness() == FF6_BRIGHTNESS_FULL);
        CHECK(menuGame.player().tileY() == tileY && !menuGame.player().moving());
    }

    {  // a saved game goes on with the same map, party and story, also after it went through JSON
        ff6::Game origin(ff6::loadCommonData(pack), pack, 0x1C, renderer, sound, {0, 1, 4, 5});
        for (int i = 0; i < 40; ++i) origin.tick(idle);
        origin.tick(up);
        for (int frame = 1; frame < FF6_STEP_FRAMES; ++frame) origin.tick(idle);
        const ff6::SaveState saved = ff6::saveFromJson(ff6::saveToJson(origin.saveState()));
        CHECK(saved.map == 0x1C && saved.x == 8 && saved.y == 44 && saved.activeParty == 1);
        ff6::Game restored(ff6::loadCommonData(pack), pack, saved, renderer, sound);
        for (int i = 0; i < 40; ++i) restored.tick(idle);
        CHECK(restored.map().id == 0x1C && restored.player().tileX() == 8 && restored.player().tileY() == 44);
        CHECK(restored.objects().object(0x34) != nullptr);  // the whole party is there
        const ff6::SaveState again = restored.saveState();
        CHECK(again.eventBits == saved.eventBits && again.battleFlags == saved.battleFlags);
        CHECK(again.activeParty == saved.activeParty && again.spritePalettes == saved.spritePalettes);
        for (size_t i = 0; i < saved.characters.size(); ++i) {
            CHECK(again.characters[i].enabled == saved.characters[i].enabled);
            CHECK(again.characters[i].party == saved.characters[i].party && again.characters[i].slot == saved.characters[i].slot);
            CHECK(again.characters[i].gfx == saved.characters[i].gfx && again.characters[i].palette == saved.characters[i].palette);
        }
    }

    {  // a chest gives its item once, tells so in a dialogue, stays open, and is open again in a saved game
        const uint32_t chestMap = 38;
        const int chestX = 65, chestY = 28;  // an item chest: item 0xC8 and treasure bit 0x0B
        test::RecordingSound chestSound;
        ff6::Game chests(ff6::loadCommonData(pack), pack, chestMap, renderer, chestSound);
        for (int i = 0; i < 600; ++i) chests.tick(idle);
        const auto cell = [&](const ff6::Game& game) {
            const ff6::MapLayout& layout = game.map().layers[FF6_BG1];
            return layout.cells[static_cast<size_t>(chestY) * layout.width + chestX];
        };
        CHECK(cell(chests) == FF6_TILE_CHEST_CLOSED);
        struct Side { int dx, dy, toward; };
        for (const Side side : {Side{0, 1, ff6::Up}, Side{0, -1, ff6::Down}, Side{1, 0, ff6::Left}, Side{-1, 0, ff6::Right}}) {
            if (chests.party().treasures[0x0B]) break;
            chests.placeParty(chestX + side.dx, chestY + side.dy);
            chests.tick({side.toward, false});
            for (int frame = 0; frame < FF6_STEP_FRAMES; ++frame) chests.tick(idle);
            if (chests.player().facing() != side.toward) continue;
            chests.tick(confirm);
        }
        CHECK(chests.party().treasures[0x0B] && chests.party().inventory[0].item == 0xC8 && chests.party().inventory[0].quantity == 1);
        CHECK(cell(chests) == FF6_TILE_CHEST_OPEN);
        CHECK(!chestSound.commands.empty() && chestSound.commands.back().arg1 == FF6_SFX_TREASURE_CHEST);
        for (int i = 0; i < 10; ++i) chests.tick(idle);
        CHECK(chests.dialogue().active());  // "Received the item!"
        size_t glyphs = 0;  // the dialogue names the item: more than the words around it
        for (int i = 0; i < 600 && chests.dialogue().active(); ++i) {
            chests.tick(i % 30 == 0 ? confirm : idle);
            glyphs = std::max(glyphs, chests.dialogue().glyphs().size());
        }
        CHECK(glyphs > 14);
        chests.tick(confirm);  // an open chest gives nothing more
        for (int i = 0; i < 60; ++i) chests.tick(idle);
        CHECK(chests.party().inventory[0].quantity == 1);
        const ff6::SaveState saved = ff6::saveFromJson(ff6::saveToJson(chests.saveState()));
        ff6::Game restored(ff6::loadCommonData(pack), pack, saved, renderer, chestSound);
        CHECK(restored.party().treasures[0x0B] && restored.party().inventory[0].item == 0xC8);
        CHECK(cell(restored) == FF6_TILE_CHEST_OPEN);
    }

    {  // a monster in a box: the chest opens, the event tells, and the battle starts
        uint32_t boxMap = 0;
        int boxX = 0, boxY = 0, boxBit = 0;
        for (uint32_t id = FF6_FIRST_SUB_MAP; id < FF6_BATTLE_MAP_COUNT && boxMap == 0; ++id) {
            ff6::MapData candidate;
            try {
                candidate = ff6::loadMap(pack, id);
            } catch (const std::runtime_error&) {
                continue;
            }
            for (size_t i = 0; i + FF6_TREASURE_PROP_SIZE <= candidate.treasures.size(); i += FF6_TREASURE_PROP_SIZE) {
                const uint8_t* chest = &candidate.treasures[i];
                const int kind = chest[2] | (chest[3] << 8);
                if (!(kind & FF6_TREASURE_KIND_MONSTER)) continue;
                boxMap = id;
                boxX = chest[0];
                boxY = chest[1];
                boxBit = kind & FF6_TREASURE_BIT_MASK;
                break;
            }
        }
        CHECK(boxMap != 0);
        test::RecordingSound boxSound;
        ff6::Game box(ff6::loadCommonData(pack), pack, boxMap, renderer, boxSound);
        for (int i = 0; i < 600; ++i) box.tick(idle);
        struct Side { int dx, dy, toward; };
        for (const Side side : {Side{0, 1, ff6::Up}, Side{0, -1, ff6::Down}, Side{1, 0, ff6::Left}, Side{-1, 0, ff6::Right}}) {
            if (box.party().treasures[static_cast<size_t>(boxBit)]) break;
            box.placeParty(boxX + side.dx, boxY + side.dy);
            box.tick({side.toward, false});
            for (int frame = 0; frame < FF6_STEP_FRAMES; ++frame) box.tick(idle);
            if (box.player().facing() != side.toward) continue;
            box.tick(confirm);
        }
        CHECK(box.party().treasures[static_cast<size_t>(boxBit)]);
        for (int i = 0; i < 3000 && !box.battleOpen(); ++i) box.tick(i % 20 == 0 ? confirm : idle);
        CHECK(box.battleOpen());
    }
    {  // a new game begins with the title screen; a press of the button ends it, and a new game starts the story
        test::RecordingSound startSound;
        ff6::Game start(ff6::loadCommonData(pack), pack, ff6::NewGame{}, renderer, startSound);
        CHECK(!start.cutsceneOpen() && start.brightness() == 0);
        start.tick(idle);
        CHECK(start.cutsceneOpen() && !start.eventBits().get(FF6_EVENT_BIT_SKIPPED));
        CHECK(startSound.commands.size() == 1 && startSound.commands[0].command == FF6_SOUND_PLAY_SONG && startSound.commands[0].arg1 == 2);
        for (int i = 0; i < 400; ++i) start.tick(idle);
        CHECK(start.cutsceneOpen());
        start.draw();
        CHECK(renderer.draws.size() == 1 && renderer.draws.front().destination.w == FF6_SCREEN_WIDTH);  // one full-screen image
        start.tick(confirm);
        for (int i = 0; i < 60 && !start.loadMenuOpen(); ++i) start.tick(idle);
        CHECK(!start.cutsceneOpen() && start.loadMenuOpen() && start.eventBits().get(FF6_EVENT_BIT_SKIPPED));
        for (int i = 0; i < 60; ++i) start.tick(idle);  // the menu fades in
        start.draw();
        CHECK(renderer.draws.size() == 1 && start.brightness() == FF6_BRIGHTNESS_FULL);
        start.tick(confirm);  // no saved game: it is the new game
        for (int i = 0; i < 100 && start.loadMenuOpen(); ++i) start.tick(idle);
        CHECK(!start.loadMenuOpen() && !start.eventBits().get(FF6_EVENT_BIT_SKIPPED));
        for (int i = 0; i < 400 && !start.dialogue().active(); ++i) start.tick(idle);
        CHECK(start.map().id == 23 && start.dialogue().active());  // "Long ago, the War of the Magi...", over the mountains
    }
    {  // with a saved game, the menu goes on with it: the map and the story come back
        ff6::Game played(ff6::loadCommonData(pack), pack, 0x1C, renderer, sound);
        played.party().gil = 1234;
        const ff6::SaveState saved = played.saveState();
        test::RecordingSound startSound;
        ff6::Game start(ff6::loadCommonData(pack), pack, ff6::NewGame{}, renderer, startSound);
        start.setLoadHandler([&saved] { return std::make_unique<ff6::SaveState>(saved); });
        start.tick(idle);
        for (int i = 0; i < 400; ++i) start.tick(idle);  // the license, then the beginning of the title
        start.tick(confirm);
        for (int i = 0; i < 60 && !start.loadMenuOpen(); ++i) start.tick(idle);
        CHECK(start.loadMenuOpen());
        for (int i = 0; i < 60; ++i) start.tick(idle);
        start.tick(confirm);  // the menu begins on "Continue"
        for (int i = 0; i < 100 && start.loadMenuOpen(); ++i) start.tick(idle);
        for (int i = 0; i < 100 && start.map().id != 0x1C; ++i) start.tick(idle);
        CHECK(start.map().id == 0x1C);
        CHECK(start.party().gil == 1234 && !start.cutsceneOpen() && !start.loadMenuOpen());
    }
}
