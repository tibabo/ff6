#include "menu/item_menu.hpp"

#include <algorithm>
#include <cstdio>

#include "field/map.hpp"

namespace ff6 {
namespace {

const char* const kOptions[FF6_ITEM_OPTION_COUNT] = {"USE", "ARRANGE", "RARE"};

std::string number(int value, int width) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%*d", width, value);
    return buffer;
}

}  // namespace

void ItemMenu::open(const std::vector<MenuItem>& items, const std::vector<MenuCharacter>& party, const ItemHooks* hooks) {
    items_ = items;
    party_ = party;
    hooks_ = hooks;
    closed_ = false;
    inList_ = false;
    inRare_ = false;
    picked_ = -1;
    rareCursor_ = 0;
    targeting_ = false;
    leaveItem_ = -1;
    target_ = 0;
    option_ = 0;
    selected_ = 0;
    first_ = 0;
    repeat_.reset();
}

// The list has no wrap: the page scrolls when the hand goes past its first or last row.
void ItemMenu::move(int step) {
    const int last = static_cast<int>(items_.size()) - 1;
    const int next = std::max(0, std::min(selected_ + step, last));
    if (next == selected_) return;
    selected_ = next;
    if (selected_ < first_) first_ = selected_;
    if (selected_ >= first_ + FF6_ITEM_ROWS) first_ = selected_ - FF6_ITEM_ROWS + 1;
    sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
}

// The inventory as the game has it now, with the hand kept in the page.
void ItemMenu::refresh() {
    if (hooks_ && hooks_->items) items_ = hooks_->items();
    if (hooks_ && hooks_->party) party_ = hooks_->party();
    selected_ = std::max(0, std::min(selected_, static_cast<int>(items_.size()) - 1));
    first_ = std::max(0, std::min(first_, selected_));
    if (selected_ >= first_ + FF6_ITEM_ROWS) first_ = selected_ - FF6_ITEM_ROWS + 1;
}

// The rare items: two columns of ten places, the hand on any of them; the cursor wraps on both axes.
void ItemMenu::updateRare(int direction, const InputState& input) {
    int row = rareCursor_ / 2, column = rareCursor_ % 2;
    if (direction == Up) row = (row + FF6_RARE_ROWS - 1) % FF6_RARE_ROWS;
    if (direction == Down) row = (row + 1) % FF6_RARE_ROWS;
    if (direction == Left || direction == Right) column = 1 - column;
    if (row * 2 + column != rareCursor_) {
        rareCursor_ = row * 2 + column;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (input.cancel) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        inRare_ = false;
    }
}

void ItemMenu::update(const InputState& input) {
    const int direction = repeat_.update(input.direction);
    if (targeting_) {
        updateTarget(direction, input);
        return;
    }
    if (inRare_) {
        updateRare(direction, input);
        return;
    }
    if (inList_) {
        if (direction == Up) move(-1);
        if (direction == Down) move(1);
        if (input.cancel) {  // ITEM_MOVE takes the pick back first, then the list is left
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
            if (picked_ >= 0) {
                selected_ = picked_;
                picked_ = -1;
                first_ = std::max(0, std::min(first_, selected_));
                if (selected_ >= first_ + FF6_ITEM_ROWS) first_ = selected_ - FF6_ITEM_ROWS + 1;
            } else {
                inList_ = false;
            }
        } else if (input.confirm && picked_ < 0) {  // the first A picks the item: on another place it swaps, on the same it uses
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
            picked_ = selected_;
        } else if (input.confirm && picked_ != selected_) {
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
            if (hooks_ && hooks_->swap) hooks_->swap(picked_, selected_);
            picked_ = -1;
            refresh();
        } else if (input.confirm) {
            picked_ = -1;
            const int item = items_[static_cast<size_t>(selected_)].id;
            if (item == FF6_ITEM_TENT || item == FF6_ITEM_WARP_STONE) {
                if (hooks_ && hooks_->leave && hooks_->leave(item)) {
                    sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
                    leaveItem_ = item;
                    closed_ = true;
                } else {
                    sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
                }
            } else if (hooks_ && hooks_->usable && hooks_->usable(item) && !party_.empty()) {
                sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
                targeting_ = true;
                target_ = 0;
            } else {
                sound_.command(FF6_SFX_MENU_INVALID, 0, 0);  // the other items have nothing to do here
            }
        }
        return;
    }
    if ((direction == Left && option_ > 0) || (direction == Right && option_ < FF6_ITEM_OPTION_COUNT - 1)) {
        option_ += direction == Right ? 1 : -1;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (input.cancel) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        closed_ = true;
    } else if (input.confirm && option_ == 0 && !items_.empty()) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        inList_ = true;
        picked_ = -1;
    } else if (input.confirm && option_ == 1 && hooks_ && hooks_->arrange) {  // SortItemsByIcon
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        hooks_->arrange();
        selected_ = first_ = 0;
        refresh();
    } else if (input.confirm && option_ == 2 && hooks_ && hooks_->rareItems) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        inRare_ = true;
        rareCursor_ = 0;
    } else if (input.confirm) {
        sound_.command(FF6_SFX_MENU_INVALID, 0, 0);  // use with nothing to use, or without the game behind it
    }
}

void ItemMenu::draw() {
    using Layer = MenuScreen::Layer;
    if (targeting_) {
        drawTarget();
        return;
    }
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(1, 1, 4, 2);
    screen_.window(7, 1, 22, 2);
    screen_.window(1, 5, 28, 3);
    screen_.window(1, 10, 28, 15);
    screen_.text(2, FF6_ITEM_OPTION_ROW, "Item", Layer::Bg3, FF6_MENU_BG3_TEAL);
    static constexpr int kOptionX[FF6_ITEM_OPTION_COUNT] = FF6_ITEM_OPTION_X;
    for (int i = 0; i < FF6_ITEM_OPTION_COUNT; ++i) {
        screen_.text(kOptionX[i], FF6_ITEM_OPTION_ROW, kOptions[i], Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    }
    // The rows are 12 pixels apart: the lines of text are 16 apart in the tiles, and the game squeezes them.
    const MenuData& data = screen_.data();
    if (inRare_) drawRare();
    for (int row = 0; row < FF6_ITEM_ROWS && !inRare_; ++row) {
        const size_t place = static_cast<size_t>(first_ + row);
        if (place >= items_.size()) break;
        const MenuItem& item = items_[place];
        const int y = FF6_ITEM_LIST_TOP + row * FF6_ITEM_ROW_HEIGHT;
        screen_.setTextOffset(y % FF6_MENU_TILE_SIZE);
        const int tileRow = y / FF6_MENU_TILE_SIZE;
        const uint8_t* name = &data.itemNames[static_cast<size_t>(item.id) * FF6_ITEM_NAME_BYTES];
        std::vector<uint8_t> codes(name, name + FF6_ITEM_NAME_BYTES);
        codes.push_back(menuCode(':'));
        screen_.codes(FF6_ITEM_NAME_X, tileRow, codes, Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        char quantity[16];
        std::snprintf(quantity, sizeof(quantity), "%2d", item.quantity);
        screen_.text(FF6_ITEM_QUANTITY_X, tileRow, quantity, Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    }
    screen_.setTextOffset(0);
    if (!inRare_) {
        drawDescription();
        drawCount(static_cast<int>(items_.size()));
    }
    if (inRare_) {
        static constexpr int kRareX[2] = FF6_RARE_CURSOR_X;
        screen_.hand(kRareX[rareCursor_ % 2], FF6_ITEM_LIST_TOP + (rareCursor_ / 2) * FF6_ITEM_ROW_HEIGHT + FF6_ITEM_CURSOR_DY);
    } else if (inList_) {
        if (picked_ >= 0 && picked_ >= first_ && picked_ < first_ + FF6_ITEM_ROWS) {  // the item to move keeps a hand
            screen_.hand(FF6_ITEM_CURSOR_X, FF6_ITEM_LIST_TOP + (picked_ - first_) * FF6_ITEM_ROW_HEIGHT + FF6_ITEM_CURSOR_DY);
        }
        screen_.hand(FF6_ITEM_CURSOR_X, FF6_ITEM_LIST_TOP + (selected_ - first_) * FF6_ITEM_ROW_HEIGHT + FF6_ITEM_CURSOR_DY);
    } else {
        static constexpr int kCursorX[FF6_ITEM_OPTION_COUNT] = FF6_ITEM_OPTION_CURSOR_X;
        screen_.hand(kCursorX[option_], FF6_ITEM_OPTION_CURSOR_Y);
    }
}

// The description of the item that the hand is on, in the window under the options: two lines, 12 pixels apart.
void ItemMenu::drawDescription() {
    if (items_.empty()) return;
    drawText(screen_.data().itemDescription(items_[static_cast<size_t>(selected_)].id));
}

void ItemMenu::drawText(const std::vector<uint8_t>& text) {
    std::vector<uint8_t> line;
    int row = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i < text.size() && text[i] != FF6_ITEM_DESC_NEWLINE) {
            line.push_back(text[i]);
            continue;
        }
        const int y = FF6_ITEM_DESC_TOP + row++ * FF6_ITEM_ROW_HEIGHT;
        screen_.setTextOffset(y % FF6_MENU_TILE_SIZE);
        screen_.codes(FF6_ITEM_DESC_X, y / FF6_MENU_TILE_SIZE, line, MenuScreen::Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        line.clear();
    }
    screen_.setTextOffset(0);
}

// The number of places that are used, or of rare items, at the right of the description.
void ItemMenu::drawCount(int count) {
    screen_.text(FF6_ITEM_COUNT_X, FF6_ITEM_COUNT_ROW, number(count, 3), MenuScreen::Layer::Bg3, FF6_MENU_BG3_DEFAULT);
}

// The rare items that the party has, in the order of their bits, two to a row; the description of the one under the hand.
void ItemMenu::drawRare() {
    const std::vector<int> owned = hooks_ && hooks_->rareItems ? hooks_->rareItems() : std::vector<int>();
    const MenuData& data = screen_.data();
    for (size_t place = 0; place < owned.size() && place < FF6_RARE_ITEM_COUNT; ++place) {
        const int y = FF6_ITEM_LIST_TOP + static_cast<int>(place / 2) * FF6_ITEM_ROW_HEIGHT;
        screen_.setTextOffset(y % FF6_MENU_TILE_SIZE);
        const uint8_t* name = &data.rareNames[static_cast<size_t>(owned[place]) * FF6_ITEM_NAME_BYTES];
        screen_.codes(place % 2 ? FF6_RARE_SECOND_COLUMN_X : FF6_ITEM_NAME_X, y / FF6_MENU_TILE_SIZE,
                      std::vector<uint8_t>(name, name + FF6_ITEM_NAME_BYTES), MenuScreen::Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    }
    screen_.setTextOffset(0);
    if (static_cast<size_t>(rareCursor_) < owned.size()) drawText(data.rareDescription(owned[static_cast<size_t>(rareCursor_)]));
    drawCount(static_cast<int>(owned.size()));
}

// The hand is on the characters: the one that gets the item. Using it again keeps the hand here while some are left.
void ItemMenu::updateTarget(int direction, const InputState& input) {
    const int count = static_cast<int>(party_.size());
    if ((direction == Up || direction == Down) && count > 0) {
        target_ = (target_ + (direction == Down ? 1 : count - 1)) % count;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (input.cancel) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        targeting_ = false;
    } else if (input.confirm) {
        const int item = items_[static_cast<size_t>(selected_)].id;
        const int character = party_[static_cast<size_t>(target_)].id;
        if (!hooks_ || !hooks_->canUse || !hooks_->use || !hooks_->canUse(item, character) || !hooks_->use(item, character)) {
            sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
            return;
        }
        sound_.command(FF6_SFX_MENU_SUCCESS, 0, 0);
        if (hooks_->items) items_ = hooks_->items();
        if (hooks_->party) party_ = hooks_->party();
        if (items_.empty()) {
            inList_ = targeting_ = false;
            selected_ = first_ = 0;
            return;
        }
        selected_ = std::min(selected_, static_cast<int>(items_.size()) - 1);
        first_ = std::max(0, std::min(first_, selected_));
        if (selected_ >= first_ + FF6_ITEM_ROWS) first_ = selected_ - FF6_ITEM_ROWS + 1;
        if (items_[static_cast<size_t>(selected_)].id != item) targeting_ = false;  // the last one was used
    }
}

// DrawItemTargetMenu: the item and its quantity on the left, the characters on the right.
void ItemMenu::drawTarget() {
    using Layer = MenuScreen::Layer;
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(10, 1, 19, 24);
    screen_.window(1, 1, 13, 2);
    screen_.window(1, 5, 7, 3);
    const MenuItem& chosen = items_[static_cast<size_t>(selected_)];
    const uint8_t* name = screen_.data().itemNames.data() + static_cast<size_t>(chosen.id) * FF6_ITEM_NAME_BYTES;
    screen_.codes(2, 2, std::vector<uint8_t>(name, name + FF6_ITEM_NAME_BYTES), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    screen_.text(2, 6, ":" + number(chosen.quantity, 2), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    static const int kPortraits[FF6_CHAR_COUNT] = FF6_MENU_PORTRAITS_OF_CHARACTER;
    for (size_t slot = 0; slot < party_.size() && slot < FF6_PARTY_SIZE; ++slot) {
        const MenuCharacter& c = party_[slot];
        const int row = 3 + static_cast<int>(slot) * FF6_MENU_BLOCK_STEP;
        if (c.id >= 0 && c.id < FF6_CHAR_COUNT) {
            screen_.portrait(FF6_ITEM_TARGET_PORTRAIT_X, FF6_MENU_PORTRAIT_Y + static_cast<int>(slot) * FF6_MENU_PORTRAIT_STEP, kPortraits[c.id]);
        }
        screen_.text(16, row, characterName(c.id), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(18, row + 2, "LV", Layer::Bg1, FF6_MENU_BG1_TEAL);
        screen_.text(18, row + 3, "HP", Layer::Bg1, FF6_MENU_BG1_TEAL);
        screen_.text(18, row + 4, "MP", Layer::Bg1, FF6_MENU_BG1_TEAL);
        screen_.text(25, row + 3, "/", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(25, row + 4, "/", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(23, row + 2, number(c.level, 2), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(21, row + 3, number(c.hp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(26, row + 3, number(c.maxHp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(21, row + 4, number(c.mp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(26, row + 4, number(c.maxMp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    }
    screen_.hand(FF6_ITEM_TARGET_CURSOR_X, FF6_MENU_CHARACTER_CURSOR_Y + FF6_MENU_PORTRAIT_STEP * target_);
}

}  // namespace ff6
