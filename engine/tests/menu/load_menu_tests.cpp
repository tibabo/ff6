#include <cstdlib>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "menu/load_menu.hpp"

// The menu after the title screen, with the real font of the data pack when FF6_PACK points to one.
void testLoadMenu() {
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::CommonData common = ff6::loadCommonData(pack);
    const ff6::MenuData menuData = ff6::loadMenuData(pack);
    ff6::MenuScreen screen(common, menuData);
    test::RecordingSound sound;
    ff6::LoadMenu menu(screen, sound);
    const ff6::InputState confirm{FF6_NO_DIRECTION, true}, up{ff6::Up, false}, down{ff6::Down, false}, idle{};

    menu.open(true);  // with a saved game, the cursor is on Continue
    menu.draw();
    CHECK(!menu.closed() && menu.continueChosen());
    bool inked = false;
    for (size_t i = 0; i < screen.pixels().size(); i += FF6_BYTES_PER_PIXEL) inked = inked || screen.pixels()[i + FF6_ALPHA_OFFSET] != 0;
    CHECK(inked);
    menu.update(up);
    menu.update(idle);
    CHECK(!menu.continueChosen() && sound.commands.back().command == FF6_SFX_MENU_MOVE);
    menu.update(confirm);
    CHECK(menu.closed() && !menu.continueChosen() && sound.commands.back().command == FF6_SFX_MENU_SELECT);

    menu.open(false);  // without one, there is only the new game
    CHECK(!menu.closed() && !menu.continueChosen());
    menu.update(down);
    menu.update(idle);
    menu.update(confirm);
    CHECK(!menu.closed() && sound.commands.back().command == FF6_SFX_MENU_INVALID);
    menu.update(up);
    menu.update(idle);
    menu.update(confirm);
    CHECK(menu.closed() && !menu.continueChosen());
}
