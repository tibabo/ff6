#include <cstdlib>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/map.hpp"
#include "menu/field_menu.hpp"
#include "menu/menu_screen.hpp"
#include "menu/window_style.hpp"

namespace {

// A font of one letter and a palette of one color for each text color, so that the colors can be told apart.
ff6::MenuData syntheticMenu() {
    ff6::MenuData menu;
    menu.fontTiles.assign(static_cast<size_t>(FF6_MENU_FONT_TILES) * FF6_MENU_FONT_TILE_BYTES, 0);
    menu.fontTiles[static_cast<size_t>(FF6_MENU_CODE_UPPER_A) * FF6_MENU_FONT_TILE_BYTES] = 0x80;  // upper left pixel, color 1
    menu.fontPalette.fill(0);
    menu.fontPalette[16 + 1] = 0x001F;  // the second palette of the bg1 text: red
    menu.fontPalette[4 + 1] = 0x03E0;  // the second palette of the bg3 text: green
    menu.cursorTiles.assign(4 * FF6_MENU_CURSOR_TILE_BYTES, 0);
    menu.cursorTiles[0] = 0x80;  // the pixel is of color 12 in the four planes
    menu.cursorTiles[1] = 0x80;
    menu.cursorTiles[16] = 0x80;
    menu.cursorTiles[17] = 0x80;
    menu.cursorPalette = {0, 0, 0, 0x7C00};  // color 15: blue
    menu.portraitTiles.assign(static_cast<size_t>(FF6_MENU_PORTRAIT_COUNT) * FF6_MENU_PORTRAIT_TILES * FF6_MENU_CURSOR_TILE_BYTES, 0);
    menu.portraitPalettes.assign(static_cast<size_t>(FF6_MENU_PORTRAIT_COUNT) * FF6_MENU_PORTRAIT_COLORS, 0);
    menu.itemNames.assign(static_cast<size_t>(FF6_ITEM_COUNT) * FF6_ITEM_NAME_BYTES, 0xFF);
    menu.itemNames[5 * FF6_ITEM_NAME_BYTES + 1] = FF6_MENU_CODE_UPPER_A;  // the item 5 is called A
    return menu;
}

ff6::CommonData syntheticCommon() {
    ff6::CommonData common;
    common.windowTiles.assign(static_cast<size_t>(FF6_WINDOW_TILE_COUNT) * FF6_WINDOW_TILE_BYTES, 255);  // opaque white
    return common;
}

const uint8_t* pixelAt(const ff6::MenuScreen& screen, int x, int y) {
    return &screen.pixels()[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL];
}

}  // namespace

void testMenu() {
    const ff6::MenuData menu = syntheticMenu();
    const ff6::CommonData common = syntheticCommon();

    // The text of the menus is made of the codes of the font: letters, digits and symbols.
    CHECK(ff6::menuCode('A') == 0x80 && ff6::menuCode('Z') == 0x99 && ff6::menuCode('a') == 0x9A && ff6::menuCode('z') == 0xB3);
    CHECK(ff6::menuCode('0') == 0xB4 && ff6::menuCode('9') == 0xBD && ff6::menuCode('!') == 0xBE && ff6::menuCode('/') == 0xC0);
    CHECK(ff6::menuCode(' ') == FF6_MENU_CODE_SPACE && ff6::menuCode('\n') == FF6_MENU_CODE_SPACE);

    {  // text is drawn tile by tile, with the colors of its palette, which are not the same on the two backgrounds
        ff6::MenuScreen screen(common, menu);
        screen.clear();
        screen.text(2, 3, "A", ff6::MenuScreen::Layer::Bg1, FF6_MENU_BG1_TEAL);
        const uint8_t* red = pixelAt(screen, 2 * FF6_MENU_TILE_SIZE, 3 * FF6_MENU_TILE_SIZE);
        CHECK(red[0] == 255 && red[1] == 0 && red[2] == 0 && red[FF6_ALPHA_OFFSET] == 255);
        const uint8_t* next = pixelAt(screen, 2 * FF6_MENU_TILE_SIZE + 1, 3 * FF6_MENU_TILE_SIZE);
        CHECK(next[0] == 0 && next[1] == 0 && next[2] == 0);  // the black of the screen: the other pixels of the tile are clear
        screen.text(4, 3, "A", ff6::MenuScreen::Layer::Bg3, FF6_MENU_BG3_GRAY);
        const uint8_t* green = pixelAt(screen, 4 * FF6_MENU_TILE_SIZE, 3 * FF6_MENU_TILE_SIZE);
        CHECK(green[0] == 0 && green[1] == 255 && green[2] == 0);
    }

    {  // a window covers its frame and inside; the hand and the text are over it
        ff6::MenuScreen screen(common, menu);
        screen.clear();
        screen.window(1, 1, 2, 2);
        CHECK(pixelAt(screen, FF6_MENU_TILE_SIZE, FF6_MENU_TILE_SIZE)[0] == 255);  // upper left corner of the frame
        CHECK(pixelAt(screen, 5 * FF6_MENU_TILE_SIZE - 1, 5 * FF6_MENU_TILE_SIZE - 1)[0] == 255);  // lower right
        CHECK(pixelAt(screen, 5 * FF6_MENU_TILE_SIZE, 5 * FF6_MENU_TILE_SIZE)[0] == 0);  // outside
        screen.hand(100, 50);
        const uint8_t* blue = pixelAt(screen, 100, 50);
        CHECK(blue[0] == 0 && blue[1] == 0 && blue[2] == 255);
    }

    {  // the field menu: the hand moves with the direction, wraps, and repeats while the direction is held
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::MenuStatus status;
        ff6::MenuCharacter terra;
        terra.id = 0;
        terra.level = 5;
        terra.hp = terra.maxHp = 123;
        terra.mp = terra.maxMp = 45;
        status.party.push_back(terra);
        ff6::GameConfig config;
        field.open(status, config);
        CHECK(field.cursor() == 0 && !field.closed());
        field.update({ff6::Up, false});
        CHECK(field.cursor() == FF6_MENU_ITEM_COUNT - 1);  // the list wraps
        CHECK(sound.commands.size() == 1 && sound.commands[0].command == FF6_SFX_MENU_MOVE);
        field.update({ff6::Down, false});
        CHECK(field.cursor() == 0);
        field.update({ff6::Down, false});  // held: nothing for the delay
        CHECK(field.cursor() == 0);
        for (int frame = 2; frame < FF6_MENU_REPEAT_DELAY + FF6_MENU_REPEAT_RATE - 1; ++frame) field.update({ff6::Down, false});
        CHECK(field.cursor() == 0);
        field.update({ff6::Down, false});  // and then it repeats
        CHECK(field.cursor() == 1);
        field.update({});
        field.draw();  // the party, the options and the hand are drawn without trouble
        field.update({FF6_NO_DIRECTION, true});
        CHECK(sound.commands.back().command == FF6_SFX_MENU_INVALID);  // the other options are not ported yet
        CHECK(!field.closed());
        ff6::InputState cancel;
        cancel.cancel = true;
        field.update(cancel);
        CHECK(field.closed());
        field.open(status, config);  // the cursor goes back to the first option, unless the config asks to remember it
        CHECK(!field.closed() && field.cursor() == 0);
        field.update({ff6::Down, false});
        field.update(cancel);
        config.display |= FF6_CONFIG_CURSOR_MEMORY;
        field.open(status, config);
        CHECK(field.cursor() == 1);
    }

    {  // the config menu is opened by the Config option, and changes the settings with the left and right buttons
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::GameConfig config;
        CHECK(config.battle == 0x2A && config.battleSpeed() == 2 && config.messageSpeed() == 2);  // the defaults of a new game
        field.open({}, config);
        for (int option = 0; option < FF6_MENU_CONFIG_ITEM; ++option) {
            field.update({ff6::Down, false});
            field.update({});
        }
        field.update({FF6_NO_DIRECTION, true});
        CHECK(field.inConfig() && sound.commands.back().command == FF6_SFX_MENU_SELECT);
        field.draw();

        const auto press = [&](int direction) {
            field.update({direction, false});
            field.update({});
        };
        press(ff6::Up);  // the cursor does not wrap in the config menu
        press(ff6::Right);  // the battle mode: the right button selects the second choice, wait
        CHECK((config.battle & FF6_CONFIG_WAIT) != 0);
        press(ff6::Left);  // and the left button the first, active
        CHECK((config.battle & FF6_CONFIG_WAIT) == 0);
        press(ff6::Down);
        press(ff6::Right);  // the battle speed goes from 3 to 4, and stops at 6
        CHECK(config.battleSpeed() == 3);
        for (int i = 0; i < 5; ++i) press(ff6::Right);
        CHECK(config.battleSpeed() == FF6_CONFIG_SPEED_MAX && config.messageSpeed() == 2);
        press(ff6::Down);
        for (int i = 0; i < 3; ++i) press(ff6::Left);
        CHECK(config.messageSpeed() == 0 && config.battleSpeed() == FF6_CONFIG_SPEED_MAX);
        for (int i = 0; i < 6; ++i) press(ff6::Down);  // down to the last option, which does not wrap
        press(ff6::Right);
        CHECK((config.controller & FF6_CONFIG_MULTIPLE_CONTROLLERS) != 0 && config.display == 0);
        field.draw();

        // Down from the last option scrolls the second page in, and the pages are not touched during the scroll.
        press(ff6::Down);
        CHECK(field.configMenu().scrolling());
        field.update({ff6::Down, false});
        for (int i = 0; i < FF6_CONFIG_PAGE_SCROLL_FRAMES; ++i) {
            field.draw();
            field.update({});
        }
        CHECK(field.configMenu().page() == 1 && field.configMenu().cursor() == 0 && !field.configMenu().scrolling());
        config.windowColors[7][0] = 0x0005;
        press(ff6::Right);  // the order of the magic: 1 to 6
        CHECK(config.magicOrder() == 1);
        for (int i = 0; i < 9; ++i) press(ff6::Right);
        CHECK(config.magicOrder() == FF6_CONFIG_ORDER_MAX && (config.controller & FF6_CONFIG_MULTIPLE_CONTROLLERS) != 0);
        press(ff6::Down);  // the pattern of the windows: 1 to 8
        for (int i = 0; i < 9; ++i) press(ff6::Right);
        CHECK(config.windowPattern() == FF6_CONFIG_WINDOW_MAX);
        press(ff6::Down);  // the color that is edited: the font, then the colors of the window
        CHECK(config.colorSlot() == 0);
        press(ff6::Right);
        CHECK(config.colorSlot() == 1);
        press(ff6::Down);  // red, green and blue
        press(ff6::Left);
        CHECK(config.windowColors[7][0] == 0x0004);
        press(ff6::Left);
        for (int i = 0; i < 6; ++i) press(ff6::Left);
        CHECK(config.windowColors[7][0] == 0);  // it stops at 0
        press(ff6::Down);
        press(ff6::Right);
        CHECK(config.windowColors[7][0] == (1 << FF6_COLOR_BITS));  // green is the next five bits
        field.draw();
        field.update({FF6_NO_DIRECTION, true});  // the A button puts the colors back
        CHECK(config.windowColors[7][0] == menu.windowColors[7][0] && sound.commands.back().command == FF6_SFX_MENU_SELECT);
        press(ff6::Up);
        press(ff6::Up);
        press(ff6::Up);
        press(ff6::Up);
        press(ff6::Up);  // up from the first option goes back to the first page
        CHECK(config.display == FF6_CONFIG_WINDOW_MAX);
        press(ff6::Up);
        for (int i = 0; i < FF6_CONFIG_PAGE_SCROLL_FRAMES; ++i) field.update({});
        CHECK(field.configMenu().page() == 0 && field.configMenu().cursor() == FF6_CONFIG_PAGE1_ROWS - 1);

        ff6::InputState cancel;
        cancel.cancel = true;
        field.update(cancel);
        CHECK(!field.inConfig() && !field.closed());  // back to the menu
    }

    {  // the item menu: the options at the top, then the list, which scrolls and does not wrap
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::GameConfig config;
        ff6::MenuStatus status;
        for (int i = 0; i < 14; ++i) status.items.push_back({3 + i, i + 1});
        field.open(status, config);
        field.update({FF6_NO_DIRECTION, true});  // the first option of the menu is Item
        CHECK(field.inItems() && !field.itemMenu().inList() && sound.commands.back().command == FF6_SFX_MENU_SELECT);
        field.draw();

        const auto press = [&](int direction) {
            field.update({direction, false});
            field.update({});
        };
        press(ff6::Left);  // the options do not wrap
        CHECK(field.itemMenu().option() == 0);
        press(ff6::Right);
        press(ff6::Right);
        press(ff6::Right);
        CHECK(field.itemMenu().option() == FF6_ITEM_OPTION_COUNT - 1);
        field.update({FF6_NO_DIRECTION, true});  // rare items are not ported yet
        CHECK(!field.itemMenu().inList() && sound.commands.back().command == FF6_SFX_MENU_INVALID);
        press(ff6::Left);
        press(ff6::Left);
        field.update({FF6_NO_DIRECTION, true});
        CHECK(field.itemMenu().inList() && field.itemMenu().selected() == 0);
        press(ff6::Up);
        CHECK(field.itemMenu().selected() == 0);
        for (int i = 0; i < FF6_ITEM_ROWS - 1; ++i) press(ff6::Down);
        CHECK(field.itemMenu().selected() == 9 && field.itemMenu().firstRow() == 0);
        press(ff6::Down);  // the page scrolls with the hand
        CHECK(field.itemMenu().selected() == 10 && field.itemMenu().firstRow() == 1);
        for (int i = 0; i < 10; ++i) press(ff6::Down);
        CHECK(field.itemMenu().selected() == 13 && field.itemMenu().firstRow() == 4);
        field.draw();
        for (int i = 0; i < 12; ++i) press(ff6::Up);
        CHECK(field.itemMenu().selected() == 1 && field.itemMenu().firstRow() == 1);
        ff6::InputState cancel;
        cancel.cancel = true;
        field.update(cancel);  // back to the options, then to the menu
        CHECK(field.inItems() && !field.itemMenu().inList());
        field.update(cancel);
        CHECK(!field.inItems() && !field.closed());
    }

    // With the real data, the first pattern of window with its own colors is the window of the common data.
    if (const char* pack = std::getenv("FF6_PACK")) {
        const ff6::MenuData real = ff6::loadMenuData(pack);
        ff6::GameConfig config;
        config.windowColors = real.windowColors;
        CHECK(ff6::buildWindowTiles(real, config) == ff6::loadCommonData(pack).windowTiles);
        config.display = 3;  // another pattern gives other tiles
        CHECK(ff6::buildWindowTiles(real, config) != ff6::loadCommonData(pack).windowTiles);
        config.windowColors[3][0] = 0x7FFF;  // and so does another color
        CHECK(ff6::buildWindowTiles(real, config).size() == ff6::loadCommonData(pack).windowTiles.size());
    }
}
