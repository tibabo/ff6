#include <cstdlib>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "menu/party_menu.hpp"

namespace {

ff6::InputState press(int direction) {
    ff6::InputState input;
    input.direction = direction;
    return input;
}

}  // namespace

// The party selection of the game: the characters of no party stand in a block, the parties below it, and two places swap.
void testPartyMenu() {
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::CommonData common = ff6::loadCommonData(pack);
    const ff6::MenuData menuData = ff6::loadMenuData(pack);
    ff6::MenuScreen screen(common, menuData);
    test::RecordingSound sound;
    ff6::PartyMenu menu(screen, sound);
    const ff6::InputState idle, confirm = [] { ff6::InputState s; s.confirm = true; return s; }(),
                                  start = [] { ff6::InputState s; s.start = true; return s; }(),
                                  cancel = [] { ff6::InputState s; s.cancel = true; return s; }();
    const auto step = [&](int direction) {
        menu.update(press(direction));
        menu.update(idle);
    };

    ff6::PartyMenuSetup setup;
    setup.parties = 2;
    setup.forced = 1u << 1;
    for (int i = 0; i < 6; ++i) setup.characters[static_cast<size_t>(i)].enabled = true;
    setup.characters[0].party = setup.characters[1].party = 1;
    setup.characters[2].party = 2;
    menu.open(setup);
    menu.draw();
    // The characters of no party fill the block in order, the others the places of their party.
    CHECK(menu.inBlock(0) == 3 && menu.inBlock(2) == 5 && menu.inBlock(3) == -1);
    CHECK(menu.inParty(0, 0) == 0 && menu.inParty(0, 1) == 1 && menu.inParty(0, 2) == -1 && menu.inParty(1, 0) == 2);
    CHECK(menu.cursor() == 0);
    step(ff6::Down);  // the second row of the block
    CHECK(menu.cursor() == ff6::PartyMenu::kBlockSlots - 8 + 0);
    step(ff6::Down);  // and out of it, to the first place of the first party
    CHECK(menu.cursor() == ff6::PartyMenu::kBlockSlots);
    menu.update(confirm);  // the leader is picked
    CHECK(menu.picked());
    step(ff6::Right);  // the second column of the party: its third place
    CHECK(menu.cursor() == ff6::PartyMenu::kBlockSlots + 2);
    menu.update(confirm);  // they swap
    CHECK(!menu.picked() && menu.inParty(0, 2) == 0 && menu.inParty(0, 0) == -1);
    // The hand goes back up to the block by the table of the game.
    step(ff6::Left);
    step(ff6::Up);
    CHECK(menu.cursor() == 8);  // the second row of the block, first column
    // A character that has to stay cannot be picked, but an empty place can.
    step(ff6::Down);
    step(ff6::Down);
    step(ff6::Down);
    CHECK(menu.cursor() == ff6::PartyMenu::kBlockSlots + 1 && menu.inParty(0, 1) == 1);  // the second place of the first party
    step(ff6::Up);
    menu.update(confirm);  // the first place holds nobody now
    CHECK(menu.picked());
    menu.update(cancel);
    CHECK(!menu.picked());
    // Every party needs someone: with the second one emptied the menu stays open and says so, for a while.
    step(ff6::Right);
    step(ff6::Right);  // the first place of the second party, with its leader
    menu.update(confirm);
    step(ff6::Up);
    step(ff6::Up);
    menu.update(confirm);  // an empty place of the block: the leader of the second party goes there
    menu.update(start);
    CHECK(!menu.closed() && menu.message() == 1);
    for (int i = 0; i < 40; ++i) menu.update(idle);
    CHECK(menu.message() == 0 && !menu.closed());

    // Whoever is in a place of a party is in that party, and the places are kept.
    menu.open(setup);
    menu.update(start);
    CHECK(menu.closed());
    const std::array<int, FF6_CHAR_COUNT> party = menu.assignment(), place = menu.places();
    CHECK(party[0] == 1 && party[1] == 1 && party[2] == 2 && party[3] == 0 && place[1] == 1);
    // The one that is forced cannot be picked up.
    menu.open(setup);
    step(ff6::Down);
    step(ff6::Down);
    step(ff6::Down);  // the second place of the first party: character 1
    CHECK(menu.cursor() == ff6::PartyMenu::kBlockSlots + 1 && menu.inParty(0, 1) == 1);
    menu.update(confirm);
    CHECK(!menu.picked() && sound.commands.back().command == FF6_SFX_MENU_INVALID);
    // With one party, an empty one is "No one there!".
    ff6::PartyMenuSetup alone;
    alone.parties = 1;
    alone.characters[0].enabled = true;
    menu.open(alone);
    menu.update(start);
    CHECK(!menu.closed() && menu.message() == 2);

    // The same place twice opens the status screen of the character, B comes back; an empty place says no.
    alone.characters[0].status = FF6_ST_POISON | FF6_ST_BLIND | FF6_ST_FLOAT;
    alone.characters[0].party = 1;
    menu.open(alone);
    menu.draw();  // the icons of the statuses
    step(ff6::Down);
    step(ff6::Down);
    menu.update(confirm);
    CHECK(menu.picked() && !menu.viewing());
    menu.update(confirm);
    CHECK(menu.viewing() && !menu.picked());
    menu.draw();
    menu.update(idle);
    menu.update(cancel);
    for (int i = 0; i < 4; ++i) menu.update(idle);
    CHECK(!menu.viewing() && !menu.closed());
    alone.characters[0].status = FF6_ST_DEAD;  // "Wounded"
    menu.open(alone);
    menu.draw();
    step(ff6::Down);
    step(ff6::Down);
    step(ff6::Right);
    menu.update(confirm);
    menu.update(confirm);
    CHECK(!menu.viewing() && sound.commands.back().command == FF6_SFX_MENU_INVALID);
}
