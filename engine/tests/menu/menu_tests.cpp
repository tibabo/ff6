#include <algorithm>
#include <cstdlib>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/map.hpp"
#include "menu/field_menu.hpp"
#include "menu/menu_screen.hpp"
#include "menu/colosseum_menu.hpp"
#include "menu/name_menu.hpp"
#include "menu/final_order_menu.hpp"
#include "menu/shop_menu.hpp"
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
    menu.rareNames.assign(static_cast<size_t>(FF6_RARE_ITEM_COUNT) * FF6_ITEM_NAME_BYTES, 0xFF);
    menu.rareDescPtrs.assign(FF6_RARE_ITEM_COUNT, 0);
    menu.rareDescriptions.assign(FF6_RARE_DESC_BYTES, 0);
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

    {  // a portrait is an image of its own, transparent where the color is 0
        ff6::MenuData withPortrait = menu;
        withPortrait.portraitTiles[static_cast<size_t>(FF6_MENU_PORTRAIT_TILES) * FF6_MENU_CURSOR_TILE_BYTES] = 0x80;  // the first pixel of portrait 1, color 1
        withPortrait.portraitPalettes[FF6_MENU_PORTRAIT_COLORS + 1] = 0x001F;
        const std::vector<uint8_t> image = ff6::buildPortraitImage(withPortrait, 1);
        CHECK(image.size() == static_cast<size_t>(FF6_MENU_PORTRAIT_SIZE) * FF6_MENU_PORTRAIT_SIZE * FF6_BYTES_PER_PIXEL);
        CHECK(image[0] == 255 && image[1] == 0 && image[2] == 0 && image[FF6_ALPHA_OFFSET] == 255);
        CHECK(image[FF6_BYTES_PER_PIXEL + FF6_ALPHA_OFFSET] == 0);
        CHECK(ff6::buildPortraitImage(withPortrait, -1).size() == image.size());
    }

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

    {  // the mosaic gives each block the color of its first pixel
        ff6::MenuScreen screen(common, menu);
        screen.clear();
        screen.plot(0, 0, 10, 20, 30);
        screen.plot(1, 1, 200, 200, 200);
        screen.plot(4, 0, 50, 60, 70);
        screen.mosaic(0, 0, 8, 8, 4);
        CHECK(pixelAt(screen, 3, 3)[0] == 10 && pixelAt(screen, 1, 1)[1] == 20);
        CHECK(pixelAt(screen, 7, 3)[2] == 70);
        screen.mosaic(0, 0, 8, 8, 1);
        CHECK(pixelAt(screen, 3, 3)[0] == 10);
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
        field.update({FF6_NO_DIRECTION, true});  // the second option, Skills, asks for a character
        CHECK(field.choosingCharacter() && sound.commands.back().command == FF6_SFX_MENU_SELECT);
        field.draw();
        ff6::InputState cancel;
        cancel.cancel = true;
        field.update(cancel);  // back to the options
        CHECK(!field.choosingCharacter() && !field.closed());
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

    {  // the screens of a character, and the save question
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::GameConfig config;
        ff6::MenuStatus status;
        for (const int id : {0, 5, 11}) {
            ff6::MenuCharacter character;
            character.id = id;
            character.level = 3;
            character.equipment[0] = 5;
            status.party.push_back(character);
        }
        const auto press = [&](int direction) {
            field.update({direction, false});
            field.update({});
        };
        const auto select = [&] { field.update({FF6_NO_DIRECTION, true}); };
        ff6::InputState cancel;
        cancel.cancel = true;

        field.open(status, config);
        for (int option = 0; option < 4; ++option) press(ff6::Down);  // Status is the fifth option
        select();
        CHECK(field.choosingCharacter() && field.selectedCharacter() == 0);
        press(ff6::Up);  // the characters wrap
        CHECK(field.selectedCharacter() == 2);
        press(ff6::Down);
        press(ff6::Down);
        CHECK(field.selectedCharacter() == 1);
        field.draw();
        select();
        CHECK(field.inCharacter() && field.characterMenu().which() == ff6::CharacterScreen::Status);
        field.draw();
        select();  // nothing to do on the status
        field.update(cancel);
        CHECK(!field.inCharacter() && field.choosingCharacter());
        field.update(cancel);
        CHECK(!field.choosingCharacter() && !field.closed());

        press(ff6::Up);  // Relic: two rows, and the list does not wrap
        select();
        select();
        CHECK(field.characterMenu().which() == ff6::CharacterScreen::Relic && field.characterMenu().cursor() == 0);
        CHECK(field.characterMenu().inOptions());
        field.draw();
        press(ff6::Right);  // Remove
        press(ff6::Right);
        CHECK(field.characterMenu().option() == 1);
        press(ff6::Left);
        select();  // Equip: the slots
        CHECK(!field.characterMenu().inOptions() && !field.characterMenu().removing());
        press(ff6::Down);
        press(ff6::Down);
        CHECK(field.characterMenu().cursor() == 1);
        field.draw();
        select();
        CHECK(sound.commands.back().command == FF6_SFX_MENU_INVALID);  // without the game behind it, nothing can be changed
        field.update(cancel);
        CHECK(field.characterMenu().inOptions());
        field.update(cancel);
        field.update(cancel);
        press(ff6::Up);  // Equip, then Skills
        press(ff6::Up);
        select();
        select();
        CHECK(field.characterMenu().which() == ff6::CharacterScreen::Skills);
        press(ff6::Down);
        CHECK(field.characterMenu().cursor() == 1);
        field.draw();
        field.update(cancel);
        field.update(cancel);

        // Save asks first, and a no or the cancel button does not save.
        for (int option = 0; option < 5; ++option) press(ff6::Down);
        CHECK(field.cursor() == FF6_MENU_SAVE_ITEM);
        select();
        CHECK(field.confirmingSave());
        field.draw();
        press(ff6::Down);  // no
        select();
        CHECK(!field.confirmingSave() && !field.takeSaveRequest());
        select();
        field.update(cancel);
        CHECK(!field.confirmingSave() && !field.takeSaveRequest());
        select();
        select();  // yes
        CHECK(!field.confirmingSave() && sound.commands.back().command == FF6_SFX_MENU_SUCCESS);
        CHECK(field.takeSaveRequest() && !field.takeSaveRequest());  // the request is taken once
    }

    {  // the equipment: a list of what fits the slot, a preview of each item, then the change
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::GameConfig config;
        ff6::MenuStatus status;
        ff6::MenuCharacter terra;
        terra.id = 0;
        terra.vigor = 30;
        terra.equipment[0] = 1;
        status.party.push_back(terra);
        int equipped = -1;
        status.equip.choices = [](int, int slot) { return slot == 0 ? std::vector<int>{4, 7} : std::vector<int>{}; };
        status.equip.preview = [terra](int, int slot, int item) {
            ff6::MenuCharacter shown = terra;
            shown.equipment[slot] = item;
            shown.vigor = item == 7 ? 35 : 30;
            return shown;
        };
        status.equip.equip = [terra, &equipped](int, int slot, int item) {
            equipped = item;
            ff6::MenuCharacter changed = terra;
            changed.equipment[slot] = item;
            return changed;
        };
        const auto press = [&](int direction) {
            field.update({direction, false});
            field.update({});
        };
        const auto select = [&] { field.update({FF6_NO_DIRECTION, true}); };
        ff6::InputState cancel;
        cancel.cancel = true;
        field.open(status, config);
        press(ff6::Down);
        press(ff6::Down);  // Equip
        select();
        select();
        CHECK(field.inCharacter() && field.characterMenu().which() == ff6::CharacterScreen::Equip);
        field.draw();
        CHECK(field.characterMenu().inOptions());
        select();  // Equip: the slots
        select();  // the right hand: the choice of taking the item off comes first
        const ff6::CharacterMenu& character = field.characterMenu();
        CHECK(character.picking() && character.choices() == std::vector<int>({FF6_ITEM_EMPTY, 4, 7}) && character.pick() == 0);
        field.draw();
        press(ff6::Down);
        press(ff6::Down);
        press(ff6::Down);  // the list does not wrap
        CHECK(character.pick() == 2);
        field.draw();
        field.update(cancel);  // back to the slots, nothing changed
        CHECK(!character.picking() && equipped == -1 && field.inCharacter());
        select();
        press(ff6::Down);
        select();  // the first item of the inventory that fits
        CHECK(!character.picking() && equipped == 4 && character.character().equipment[0] == 4);
        press(ff6::Down);  // the left hand has nothing that fits but the empty choice
        select();
        CHECK(character.picking() && character.choices().size() == 1);
        field.update(cancel);
        field.update(cancel);
        CHECK(character.inOptions() && field.inCharacter());
        // Optimum and Empty ask the game; Rmove takes the item of the slot off.
        press(ff6::Right);
        select();
        CHECK(sound.commands.back().command == FF6_SFX_MENU_INVALID);  // no hook
        int optimized = 0, emptied = 0;
        status.equip.optimum = [&](int) {
            ++optimized;
            ff6::MenuCharacter best = terra;
            best.equipment[0] = 7;
            return best;
        };
        status.equip.empty = [&](int) {
            ++emptied;
            ff6::MenuCharacter bare = terra;
            bare.equipment[0] = FF6_ITEM_EMPTY;
            return bare;
        };
        field.update(cancel);
        CHECK(!field.inCharacter());
        field.open(status, config);  // again, with the hooks
        press(ff6::Down);
        press(ff6::Down);
        select();
        select();
        press(ff6::Right);
        select();  // Optimum
        CHECK(optimized == 1 && character.character().equipment[0] == 7 && character.inOptions());
        press(ff6::Right);
        press(ff6::Right);
        select();  // Empty
        CHECK(emptied == 1 && character.character().equipment[0] == FF6_ITEM_EMPTY && character.inOptions());
        press(ff6::Left);
        select();  // Rmove: the slots, and the item of the slot goes
        CHECK(!character.inOptions() && character.removing());
        field.draw();
        select();
        CHECK(equipped == FF6_ITEM_EMPTY && character.character().equipment[0] == FF6_ITEM_EMPTY);
        field.update(cancel);
        CHECK(character.inOptions());
        field.update(cancel);
        CHECK(!field.inCharacter());
    }

    {  // using an item: the list, then the character, which stays chosen while the item lasts
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::GameConfig config;
        ff6::MenuStatus status;
        ff6::MenuCharacter hurt;
        hurt.id = 0;
        hurt.hp = 10;
        hurt.maxHp = 60;
        status.party.push_back(hurt);
        status.items.push_back({233, 2});
        status.items.push_back({7, 1});
        int quantity = 2, hp = 10;
        status.item.usable = [](int item) { return item == 233; };
        status.item.canUse = [&](int, int) { return hp < 60 && quantity > 0; };
        status.item.use = [&](int, int) {
            hp = std::min(60, hp + 25);
            --quantity;
            return true;
        };
        status.item.items = [&] {
            std::vector<ff6::MenuItem> items;
            if (quantity > 0) items.push_back({233, quantity});
            items.push_back({7, 1});
            return items;
        };
        status.item.party = [&] {
            ff6::MenuCharacter now = hurt;
            now.hp = hp;
            return std::vector<ff6::MenuCharacter>{now};
        };
        const auto select = [&] { field.update({FF6_NO_DIRECTION, true}); };
        ff6::InputState cancel;
        cancel.cancel = true;
        field.open(status, config);
        select();  // Item
        select();  // Use
        const ff6::ItemMenu& items = field.itemMenu();
        select();  // the first item is picked
        CHECK(!items.targeting() && items.picked() == 0);
        select();  // and picked again: it is used
        CHECK(items.targeting() && items.target() == 0 && items.picked() == -1);
        field.draw();
        select();
        CHECK(hp == 35 && quantity == 1 && items.targeting() && items.party()[0].hp == 35);
        select();
        CHECK(hp == 60 && quantity == 0 && !items.targeting());  // the last potion is gone: back to the list
        CHECK(items.items().size() == 1 && items.items()[0].id == 7);
        select();
        select();  // an item that the menu cannot use
        CHECK(!items.targeting() && sound.commands.back().command == FF6_SFX_MENU_INVALID);
        field.update(cancel);
        field.update(cancel);
        CHECK(!field.inItems());
    }

    {  // Order: left of the options, A on two characters swaps them, A twice on one changes its row, B leaves
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::GameConfig config;
        ff6::MenuStatus status;
        for (const int id : {0, 5, 11}) {
            ff6::MenuCharacter character;
            character.id = id;
            status.party.push_back(character);
        }
        std::vector<ff6::MenuCharacter> party = status.party;
        status.order.swap = [&](int from, int to) {
            std::swap(party[static_cast<size_t>(from)], party[static_cast<size_t>(to)]);
            return party;
        };
        status.order.row = [&](int place) {
            party[static_cast<size_t>(place)].backRow = !party[static_cast<size_t>(place)].backRow;
            return party;
        };
        const auto press = [&](int direction) {
            field.update({direction, false});
            field.update({});
        };
        const auto select = [&] { field.update({FF6_NO_DIRECTION, true}); };
        ff6::InputState cancel;
        cancel.cancel = true;
        field.open(status, config);
        const auto wait = [&](int frames) {
            for (int frame = 0; frame < frames; ++frame) {
                field.draw();
                field.update({});
            }
        };
        press(ff6::Left);
        CHECK(field.ordering() && field.orderAnimating());
        select();  // the screen is still sliding: nothing is picked
        wait(FF6_MENU_ORDER_SLIDE_FRAMES);
        CHECK(field.ordering() && !field.orderAnimating() && field.orderFrom() == -1);
        field.draw();
        select();  // the first character
        CHECK(field.orderFrom() == 0);
        press(ff6::Down);
        field.draw();
        select();  // and the second: they change places
        CHECK(field.orderFrom() == -1 && field.party()[0].id == 5 && field.party()[1].id == 0 && field.orderAnimating());
        wait(2 * FF6_MENU_ORDER_ROW_FRAMES);
        CHECK(!field.orderAnimating());
        select();
        select();  // the same one twice (the hand stayed on the second place): its row
        CHECK(field.party()[1].backRow && !field.party()[0].backRow && field.orderFrom() == -1 && field.orderAnimating());
        wait(FF6_MENU_ORDER_ROW_FRAMES);
        field.draw();
        select();
        field.update(cancel);  // B takes the pick back, and the hand goes back to it
        CHECK(field.ordering() && field.orderFrom() == -1 && !field.orderAnimating());
        field.update(cancel);  // and the screen slides back
        CHECK(field.ordering() && field.orderAnimating());
        wait(FF6_MENU_ORDER_SLIDE_FRAMES);
        CHECK(!field.ordering() && !field.closed());
        press(ff6::Left);
        wait(FF6_MENU_ORDER_SLIDE_FRAMES);
        press(ff6::Right);  // Right leaves too
        wait(FF6_MENU_ORDER_SLIDE_FRAMES);
        CHECK(!field.ordering());
    }

    {  // the inventory: a pick and another place swap the items, Arrange sorts them, Rare lists the key items
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::GameConfig config;
        ff6::MenuStatus status;
        status.party.push_back(ff6::MenuCharacter());
        std::vector<ff6::MenuItem> inventory = {{233, 2}, {7, 1}, {238, 4}};
        std::vector<int> rare = {0, 2, 3};
        status.items = inventory;
        status.item.items = [&] { return inventory; };
        status.item.swap = [&](int first, int second) { std::swap(inventory[static_cast<size_t>(first)], inventory[static_cast<size_t>(second)]); };
        int arranged = 0;
        status.item.arrange = [&] {
            ++arranged;
            std::sort(inventory.begin(), inventory.end(), [](const ff6::MenuItem& a, const ff6::MenuItem& b) { return a.id < b.id; });
        };
        status.item.rareItems = [&] { return rare; };
        const auto press = [&](int direction) {
            field.update({direction, false});
            field.update({});
        };
        const auto select = [&] { field.update({FF6_NO_DIRECTION, true}); };
        ff6::InputState cancel;
        cancel.cancel = true;
        field.open(status, config);
        select();  // Item
        const ff6::ItemMenu& items = field.itemMenu();
        select();  // Use: the list
        select();  // the first is picked
        press(ff6::Down);
        press(ff6::Down);
        select();  // the third takes its place
        CHECK(inventory[0].id == 238 && inventory[2].id == 233 && items.items()[0].id == 238 && items.picked() == -1);
        select();
        field.update(cancel);  // B takes the pick back
        CHECK(items.picked() == -1 && items.inList());
        field.update(cancel);
        CHECK(!items.inList());
        press(ff6::Right);  // Arrange
        select();
        CHECK(arranged == 1 && items.items()[0].id == 7 && items.selected() == 0);
        press(ff6::Right);  // Rare
        select();
        CHECK(items.inRare() && items.rareCursor() == 0);
        field.draw();
        press(ff6::Right);
        press(ff6::Down);
        CHECK(items.rareCursor() == 3);
        press(ff6::Left);
        press(ff6::Up);
        press(ff6::Up);  // the cursor wraps over the ten rows
        CHECK(items.rareCursor() == 18);
        field.draw();
        field.update(cancel);
        CHECK(!items.inRare() && !field.inItems() == false);
    }

    {  // Save is for the save points: gray, and it does nothing, anywhere else
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::GameConfig config;
        ff6::MenuStatus status;
        status.party.push_back(ff6::MenuCharacter());
        status.canSave = false;
        field.open(status, config);
        for (int option = 0; option < FF6_MENU_SAVE_ITEM; ++option) {
            field.update({ff6::Down, false});
            field.update({});
        }
        field.draw();
        field.update({FF6_NO_DIRECTION, true});
        CHECK(!field.confirmingSave() && sound.commands.back().command == FF6_SFX_MENU_INVALID);
    }

    {  // the tent ends the whole menu, if the game lets it be used here
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::GameConfig config;
        ff6::MenuStatus status;
        status.party.push_back(ff6::MenuCharacter());
        status.items.push_back({FF6_ITEM_TENT, 1});
        bool allowed = false;
        status.item.leave = [&](int) { return allowed; };
        const auto select = [&] { field.update({FF6_NO_DIRECTION, true}); };
        field.open(status, config);
        select();  // Item
        select();  // Use
        select();  // the tent is picked
        select();  // and used, here that does nothing
        CHECK(sound.commands.back().command == FF6_SFX_MENU_INVALID && !field.closed() && field.takeLeaveItem() == -1);
        allowed = true;
        select();
        select();
        CHECK(field.closed() && field.takeLeaveItem() == FF6_ITEM_TENT && field.takeLeaveItem() == -1);
    }

    {  // the name of a character: letters from the grid, one taken away, and Start
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::NameMenu names(screen, sound);
        names.open(0, "Terra");
        names.draw();
        names.update({ff6::Right, false});
        names.update({});
        names.update({FF6_NO_DIRECTION, true});  // "B" goes after "Terra" (a sixth letter)
        CHECK(names.name() == "TerraB");
        names.update({FF6_NO_DIRECTION, true});  // no seventh
        CHECK(names.name() == "TerraB");
        names.update({FF6_NO_DIRECTION, false, true});
        CHECK(names.name() == "Terra");
        ff6::InputState start;
        start.start = true;
        names.update(start);
        CHECK(names.closed());
        ff6::setCharacterName(0, "Teri");
        CHECK(ff6::characterName(0) == "Teri");
        ff6::setCharacterName(0, "");
        CHECK(ff6::characterName(0) == "Terra");
    }

    {  // the order for the end: two chosen, one taken back, then Start puts the others after
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FinalOrderMenu order(screen, sound);
        order.open({0, 1, 2, 3});
        order.draw();
        order.update({ff6::Down, false});
        order.update({});
        order.update({FF6_NO_DIRECTION, true});  // the second one first
        order.update({ff6::Up, false});
        order.update({});
        order.update({FF6_NO_DIRECTION, true});  // then the first one
        CHECK(order.order() == std::vector<int>({1, 0}));
        order.update({FF6_NO_DIRECTION, false, true});
        CHECK(order.order() == std::vector<int>({1}));
        ff6::InputState start;
        start.start = true;
        order.update(start);
        CHECK(order.closed() && order.order() == std::vector<int>({1, 0, 2, 3}));
    }

    {  // the colosseum: the item to wager, then the character
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::ColosseumMenu colosseum(screen, sound);
        ff6::ColosseumHooks hooks;
        hooks.items = [] { return std::vector<ff6::MenuItem>{{233, 3}, {234, 1}}; };
        hooks.party = [] {
            std::vector<ff6::MenuCharacter> party(2);
            party[0].id = 0;
            party[1].id = 5;
            return party;
        };
        hooks.match = [](int item) {
            ff6::ColosseumMatch match;
            match.monster = item;
            match.prize = 7;
            return match;
        };
        colosseum.open(hooks);
        colosseum.draw();
        colosseum.update({ff6::Down, false});
        colosseum.update({});
        colosseum.update({FF6_NO_DIRECTION, true});
        CHECK(colosseum.mode() == ff6::ColosseumMode::Challenger && colosseum.item() == 234 && colosseum.match().monster == 234);
        colosseum.draw();
        colosseum.update({ff6::Down, false});
        colosseum.update({});
        colosseum.update({FF6_NO_DIRECTION, true});
        CHECK(colosseum.closed() && colosseum.chosen() && colosseum.character() == 5);
        colosseum.open(hooks);
        colosseum.update({FF6_NO_DIRECTION, false, true});
        CHECK(colosseum.closed() && !colosseum.chosen());
    }

    {  // the shop: buy and sell, with the quantity limited by the gil and by 99
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::ShopMenu shop(screen, sound);
        long gil = 120;
        int potions = 0;
        ff6::ShopHooks hooks;
        hooks.buyPrice = [](int item) { return item == 233 ? 50 : 1000; };
        hooks.sellPrice = [](int item) { return item == 233 ? 25 : 0; };
        hooks.buy = [&](int item, int quantity) {
            if (item != 233 || quantity * 50 > gil) return false;
            gil -= quantity * 50;
            potions += quantity;
            return true;
        };
        hooks.sell = [&](int item, int quantity) {
            if (item != 233 || quantity > potions) return false;
            gil += quantity * 25;
            potions -= quantity;
            return true;
        };
        hooks.items = [&] {
            std::vector<ff6::MenuItem> items;
            if (potions > 0) items.push_back({233, potions});
            return items;
        };
        hooks.gil = [&] { return gil; };
        shop.open(3, {233, 234}, hooks);
        const auto press = [&](int direction) {
            shop.update({direction, false});
            shop.update({});
        };
        const auto select = [&] { shop.update({FF6_NO_DIRECTION, true}); };
        ff6::InputState cancel;
        cancel.cancel = true;
        CHECK(shop.mode() == ff6::ShopMode::Options && shop.message() == "Hi! Can I help you?");
        shop.draw();
        press(ff6::Left);  // the options do not wrap
        CHECK(shop.option() == 0);
        select();
        CHECK(shop.mode() == ff6::ShopMode::BuyList && shop.message() == "Help yourself!");
        shop.draw();
        select();  // the potion: 120 gil buy two
        CHECK(shop.mode() == ff6::ShopMode::BuyQuantity && shop.quantity() == 1);
        press(ff6::Up);  // ten more is more than the gil allows
        CHECK(shop.quantity() == 2);
        press(ff6::Down);
        CHECK(shop.quantity() == 1);
        press(ff6::Right);
        press(ff6::Right);
        CHECK(shop.quantity() == 2);
        shop.draw();
        select();
        CHECK(potions == 2 && gil == 20 && shop.mode() == ff6::ShopMode::BuyList && shop.message() == "Bye!");
        select();  // 20 gil do not pay for another one
        CHECK(shop.mode() == ff6::ShopMode::BuyList && shop.message() == "You need more GP!");
        CHECK(sound.commands.back().command == FF6_SFX_MENU_INVALID);
        press(ff6::Down);
        select();  // and the other item is out of reach too
        CHECK(shop.mode() == ff6::ShopMode::BuyList);
        shop.update(cancel);
        CHECK(shop.mode() == ff6::ShopMode::Options);
        press(ff6::Right);
        select();  // sell
        CHECK(shop.mode() == ff6::ShopMode::SellList && shop.message() == "Whatcha got?");
        shop.draw();
        select();
        CHECK(shop.mode() == ff6::ShopMode::SellQuantity);
        press(ff6::Up);
        CHECK(shop.quantity() == 2);  // no more than the party has
        shop.draw();
        select();
        CHECK(potions == 0 && gil == 70 && shop.mode() == ff6::ShopMode::Options);  // nothing left to sell
        press(ff6::Right);
        select();  // exit
        CHECK(shop.closed());
    }

    {  // the lists of the Skills screen: the categories that have names open, the others do not
        ff6::MenuScreen screen(common, menu);
        test::RecordingSound sound;
        ff6::FieldMenu field(screen, sound);
        ff6::GameConfig config;
        ff6::MenuStatus status;
        ff6::MenuCharacter terra;
        terra.id = 0;
        terra.skills[1] = {{0xE9, 0x85, 0xA2, 0xB1, 0xA4, 0xFF, 0xFF}, {0xE9, 0x82, 0xB4, 0xAB, 0xB3, 0xFF, 0xFF}};  // two spells
        status.party.push_back(terra);
        const auto press = [&](int direction) {
            field.update({direction, false});
            field.update({});
        };
        const auto select = [&] { field.update({FF6_NO_DIRECTION, true}); };
        ff6::InputState cancel;
        cancel.cancel = true;
        field.open(status, config);
        press(ff6::Down);  // Skills
        select();
        select();
        const ff6::CharacterMenu& character = field.characterMenu();
        CHECK(character.which() == ff6::CharacterScreen::Skills && !character.listing());
        select();  // the espers: there are none
        CHECK(!character.listing() && sound.commands.back().command == FF6_SFX_MENU_INVALID);
        press(ff6::Down);  // the magic
        select();
        CHECK(character.listing() && character.listCursor() == 0);
        field.draw();
        press(ff6::Down);
        press(ff6::Down);  // the list does not wrap
        CHECK(character.listCursor() == 1);
        field.draw();
        field.update(cancel);  // back to the categories, then out
        CHECK(!character.listing() && field.inCharacter());
        field.update(cancel);
        CHECK(!field.inCharacter());
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

        // The descriptions: the potion has one line, the dirk none, the first sword-like items two lines.
        const std::vector<uint8_t> potion = real.itemDescription(233);
        CHECK(potion.size() == 15 && potion[0] == FF6_MENU_CODE_UPPER_A + 'R' - 'A');
        CHECK(real.itemDescription(0).empty() && real.itemDescription(-1).empty() && real.itemDescription(256).empty());
        const std::vector<uint8_t> evades = real.itemDescription(2);
        CHECK(std::count(evades.begin(), evades.end(), FF6_ITEM_DESC_NEWLINE) == 1);
    }
}
