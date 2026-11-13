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
    leaveItem_ = -1;
    inConfig_ = false;
    inItems_ = false;
    choosing_ = false;
    inCharacter_ = false;
    confirmingSave_ = false;
    saveRequested_ = false;
    ordering_ = false;
    orderFrom_ = -1;
    orderAnim_ = OrderAnim::None;
    animFrames_ = 0;
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
            if (itemMenu_.leaveItem() >= 0) {  // the tent or the warp stone: the whole menu is over
                leaveItem_ = itemMenu_.leaveItem();
                closed_ = true;
            }
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
    if (ordering_) {
        updateOrder(direction, input);
        return;
    }
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
    if (direction == Left && !status_.party.empty()) {  // MainMenuLeftBtn: the options slide away and the party can be ordered
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        ordering_ = true;
        orderFrom_ = -1;
        selected_ = 0;
        orderAnim_ = OrderAnim::SlideIn;
        animFrames_ = FF6_MENU_ORDER_SLIDE_FRAMES;
        return;
    }
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
    } else if (input.confirm && cursor_ == FF6_MENU_SAVE_ITEM && status_.canSave) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        confirmingSave_ = true;
        saveYes_ = true;
    } else if (input.confirm) {
        sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
    }
}

// ORDER_SELECT and ORDER_MOVE: A on a character picks it, then A on another one swaps the two and A on the same one changes
// its row; B takes the pick back, then leaves the screen, like Right does.
void FieldMenu::updateOrder(int direction, const InputState& input) {
    if (animFrames_ > 0) {  // the screen slides, a portrait moves, two characters change places: nothing is asked meanwhile
        if (--animFrames_ == 0) {
            if (orderAnim_ == OrderAnim::SlideOut) ordering_ = false;
            orderAnim_ = OrderAnim::None;
        }
        return;
    }
    const int count = static_cast<int>(status_.party.size());
    if ((direction == Up || direction == Down) && count > 0) {
        selected_ = (selected_ + (direction == Down ? 1 : count - 1)) % count;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (input.cancel || (direction == Right && orderFrom_ < 0)) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        if (orderFrom_ >= 0) {
            selected_ = orderFrom_;
            orderFrom_ = -1;
        } else {
            orderAnim_ = OrderAnim::SlideOut;
            animFrames_ = FF6_MENU_ORDER_SLIDE_FRAMES;
        }
    } else if (input.confirm) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        if (orderFrom_ < 0) {
            orderFrom_ = selected_;
        } else {
            const OrderHooks& hooks = status_.order;
            if (orderFrom_ == selected_) {  // the portrait moves a pixel at each frame to the other row
                if (hooks.row) status_.party = hooks.row(selected_);
                orderAnim_ = OrderAnim::Row;
                animFrames_ = FF6_MENU_ORDER_ROW_FRAMES;
                animSlots_[0] = selected_;
            } else if (hooks.swap) {  // the two blocks go, then the new ones come
                status_.party = hooks.swap(orderFrom_, selected_);
                orderAnim_ = OrderAnim::Swap;
                animFrames_ = 2 * FF6_MENU_ORDER_ROW_FRAMES;
                animSlots_[0] = orderFrom_;
                animSlots_[1] = selected_;
            }
            orderFrom_ = -1;
        }
    }
}

void FieldMenu::drawCharacter(const MenuCharacter& character, int slot) {
    const int row = 3 + slot * FF6_MENU_BLOCK_STEP;
    using Layer = MenuScreen::Layer;
    const bool swapping = orderAnim_ == OrderAnim::Swap && animFrames_ > FF6_MENU_ORDER_ROW_FRAMES &&
                          (slot == animSlots_[0] || slot == animSlots_[1]);
    if (swapping) return;  // the block of the old place has gone and the new one has not come yet
    int portraitX = FF6_MENU_PORTRAIT_X + (character.backRow ? FF6_MENU_PORTRAIT_BACK_ROW_DX : 0);
    if (orderAnim_ == OrderAnim::Row && slot == animSlots_[0]) portraitX += character.backRow ? -animFrames_ : animFrames_;
    if (character.id >= 0 && character.id < FF6_CHAR_COUNT) {
        screen_.portrait(portraitX, FF6_MENU_PORTRAIT_Y + slot * FF6_MENU_PORTRAIT_STEP, screen_.characterPortrait(character.id));
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
    const bool sliding = orderAnim_ == OrderAnim::SlideIn || orderAnim_ == OrderAnim::SlideOut;
    drawMain(!choosing_ && !confirmingSave_ && !ordering_, !confirmingSave_ && (!ordering_ || sliding));
    using Layer = MenuScreen::Layer;
    if (ordering_) {  // the whole screen slides to the right, and the label of the screen comes in from the left
        const int shift = orderAnim_ == OrderAnim::SlideIn    ? FF6_MENU_ORDER_SLIDE_STEP * (FF6_MENU_ORDER_SLIDE_FRAMES - animFrames_)
                          : orderAnim_ == OrderAnim::SlideOut ? FF6_MENU_ORDER_SLIDE_STEP * animFrames_
                                                              : FF6_MENU_ORDER_SCROLL;
        if (shift > 0) screen_.shiftRight(shift);
        screen_.setOriginX(shift - FF6_MENU_ORDER_SCROLL);
        screen_.window(1, 1, 7, 2);
        screen_.text(3, 2, "Order", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.setOriginX(0);
        if (!sliding) {
            if (orderFrom_ >= 0) screen_.hand(FF6_MENU_CHARACTER_CURSOR_X + shift, FF6_MENU_CHARACTER_CURSOR_Y + FF6_MENU_PORTRAIT_STEP * orderFrom_);
            screen_.hand(FF6_MENU_CHARACTER_CURSOR_X + shift, FF6_MENU_CHARACTER_CURSOR_Y + FF6_MENU_PORTRAIT_STEP * selected_);
        }
    } else if (choosing_) {
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
        const bool gray = i == FF6_MENU_SAVE_ITEM && !status_.canSave;
        screen_.text(FF6_MENU_OPTIONS_TEXT_X, FF6_MENU_OPTIONS_TEXT_Y + 2 * i, kOptions[i],
                     bg1 ? Layer::Bg1 : Layer::Bg3, bg1 ? FF6_MENU_BG1_DEFAULT : gray ? FF6_MENU_BG3_GRAY : FF6_MENU_BG3_DEFAULT);
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
