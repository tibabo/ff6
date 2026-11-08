#include "menu/field_menu.hpp"

#include <cstdio>

#include "field/map.hpp"

namespace ff6 {
namespace {

const char* const kOptions[FF6_MENU_ITEM_COUNT] = {"Item", "Skills", "Equip", "Relic", "Status", "Config", "Save"};

// The text of a number, right aligned in `width` characters; larger numbers keep their last digits.
std::string number(long value, int width, bool zeroes = false) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), zeroes ? "%0*ld" : "%*ld", width, value);
    std::string text = buffer;
    return text.substr(text.size() - static_cast<size_t>(width));
}

}  // namespace

void FieldMenu::open(const MenuStatus& status, GameConfig& config) {
    status_ = status;
    config_ = &config;
    closed_ = false;
    inConfig_ = false;
    inItems_ = false;
    choosing_ = false;
    inCharacter_ = false;
    confirmingSave_ = false;
    saveRequested_ = false;
    if (!config.cursorMemory()) cursor_ = 0;
    screen_.applyConfig(config);
    repeat_.reset();
}

void FieldMenu::moveCursor(int step) {
    cursor_ = (cursor_ + step + FF6_MENU_ITEM_COUNT) % FF6_MENU_ITEM_COUNT;
    sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
}

// The direction repeats while it is held, see DirectionRepeat.
void FieldMenu::update(const InputState& input) {
    if (inCharacter_) {
        characterMenu_.update(input);
        if (characterMenu_.closed()) {
            status_.party[static_cast<size_t>(selected_)] = characterMenu_.character();  // the equipment may have changed
            inCharacter_ = false;
            repeat_.reset();
        }
        return;
    }
    if (inItems_) {
        itemMenu_.update(input);
        if (itemMenu_.closed()) {
            if (!itemMenu_.party().empty()) status_.party = itemMenu_.party();  // the items may have healed them
            inItems_ = false;
            repeat_.reset();
        }
        return;
    }
    if (inConfig_) {
        configMenu_.update(input);
        if (configMenu_.closed()) {
            inConfig_ = false;
            repeat_.reset();
        }
        return;
    }
    const int direction = repeat_.update(input.direction);
    if (choosing_) {  // the hand is on the characters: the screen of the one that is chosen opens
        const int count = static_cast<int>(status_.party.size());
        if (direction == Up || direction == Down) {
            selected_ = (selected_ + (direction == Down ? 1 : count - 1)) % count;
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        }
        if (input.cancel) {
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
            choosing_ = false;
        } else if (input.confirm) {
            static constexpr CharacterScreen kScreens[] = {CharacterScreen::Skills, CharacterScreen::Equip,
                                                           CharacterScreen::Relic, CharacterScreen::Status};
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
            characterMenu_.open(kScreens[cursor_ - FF6_MENU_FIRST_CHARACTER_ITEM], status_.party[static_cast<size_t>(selected_)],
                                &status_.equip);
            inCharacter_ = true;
        }
        return;
    }
    if (confirmingSave_) {
        if (direction == Up || direction == Down) {
            saveYes_ = !saveYes_;
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        }
        if (input.cancel || (input.confirm && !saveYes_)) {
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
            confirmingSave_ = false;
        } else if (input.confirm) {
            sound_.command(FF6_SFX_MENU_SUCCESS, 0, 0);
            saveRequested_ = true;
            confirmingSave_ = false;
        }
        return;
    }
    if (direction == Up) moveCursor(-1);
    if (direction == Down) moveCursor(1);
    if (input.cancel || input.menu) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        closed_ = true;
    } else if (input.confirm && cursor_ == FF6_MENU_ITEM_ITEM) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        itemMenu_.open(status_.item.items ? status_.item.items() : status_.items, status_.party, &status_.item);
        inItems_ = true;
    } else if (input.confirm && cursor_ == FF6_MENU_CONFIG_ITEM) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        configMenu_.open(*config_);
        inConfig_ = true;
    } else if (input.confirm && cursor_ >= FF6_MENU_FIRST_CHARACTER_ITEM && cursor_ <= FF6_MENU_LAST_CHARACTER_ITEM &&
               !status_.party.empty()) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        choosing_ = true;
        selected_ = 0;
    } else if (input.confirm && cursor_ == FF6_MENU_SAVE_ITEM) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        confirmingSave_ = true;
        saveYes_ = true;
    } else if (input.confirm) {
        sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
    }
}

void FieldMenu::drawCharacter(const MenuCharacter& character, int slot) {
    const int row = 3 + slot * FF6_MENU_BLOCK_STEP;
    using Layer = MenuScreen::Layer;
    static const int kPortraits[FF6_CHAR_COUNT] = FF6_MENU_PORTRAITS_OF_CHARACTER;
    if (character.id >= 0 && character.id < FF6_CHAR_COUNT) {
        screen_.portrait(FF6_MENU_PORTRAIT_X, FF6_MENU_PORTRAIT_Y + slot * FF6_MENU_PORTRAIT_STEP, kPortraits[character.id]);
    }
    screen_.text(10, row + 2, "LV", Layer::Bg1, FF6_MENU_BG1_TEAL);
    screen_.text(10, row + 3, "HP", Layer::Bg1, FF6_MENU_BG1_TEAL);
    screen_.text(10, row + 4, "MP", Layer::Bg1, FF6_MENU_BG1_TEAL);
    screen_.text(17, row + 3, "/", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(17, row + 4, "/", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(8, row, characterName(character.id), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(15, row + 2, number(character.level, 2), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(13, row + 3, number(character.hp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(18, row + 3, number(character.maxHp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(13, row + 4, number(character.mp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(18, row + 4, number(character.maxMp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
}

// DrawMainMenu: the windows, then the text of each of them.
void FieldMenu::draw() {
    if (inCharacter_) {
        characterMenu_.draw();
        return;
    }
    if (inItems_) {
        itemMenu_.draw();
        return;
    }
    if (inConfig_) {
        configMenu_.draw();
        return;
    }
    drawMain(!choosing_ && !confirmingSave_, !confirmingSave_);
    using Layer = MenuScreen::Layer;
    if (choosing_) {
        screen_.hand(FF6_MENU_CHARACTER_CURSOR_X, FF6_MENU_CHARACTER_CURSOR_Y + FF6_MENU_PORTRAIT_STEP * selected_);
    } else if (confirmingSave_) {  // the question covers the list of options
        screen_.window(22, 1, 7, 10);
        screen_.text(23, 3, "Erasing", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.text(23, 5, "data.", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.text(23, 7, "Okay?", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.text(26, 9, "Yes", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.text(26, 11, "No", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.hand(FF6_MENU_SAVE_CURSOR_X, FF6_MENU_SAVE_CURSOR_Y + (saveYes_ ? 0 : 2 * FF6_MENU_TILE_SIZE));
    }
}

void FieldMenu::drawMain(bool handOnOptions, bool showOptions) {
    using Layer = MenuScreen::Layer;
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.clear();
    screen_.window(1, 1, 28, 24);
    if (showOptions) screen_.window(23, 1, 6, 13);
    screen_.window(23, 16, 6, 2);
    screen_.window(22, 20, 7, 5);
    for (int i = 0; showOptions && i < FF6_MENU_ITEM_COUNT; ++i) {
        const bool bg1 = i == 2 || i == 5;  // Equip and Config are on the other background in the game
        screen_.text(FF6_MENU_OPTIONS_TEXT_X, FF6_MENU_OPTIONS_TEXT_Y + 2 * i, kOptions[i],
                     bg1 ? Layer::Bg1 : Layer::Bg3, bg1 ? FF6_MENU_BG1_DEFAULT : FF6_MENU_BG3_DEFAULT);
    }
    screen_.text(25, 17, "Time", Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.text(23, 21, "Steps", Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.text(23, 24, "Gp", Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.text(27, 18, ":", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    screen_.text(25, 18, number(status_.hours, 2), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    screen_.text(28, 18, number(status_.minutes, 2, true), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    screen_.text(23, 22, number(status_.steps, 7), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    screen_.text(23, 25, number(status_.gil, 7), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    for (size_t slot = 0; slot < status_.party.size() && slot < FF6_PARTY_SIZE; ++slot) {
        drawCharacter(status_.party[slot], static_cast<int>(slot));
    }
    if (handOnOptions) screen_.hand(FF6_MENU_CURSOR_X, FF6_MENU_CURSOR_Y + FF6_MENU_CURSOR_STEP * cursor_);
}

}  // namespace ff6
