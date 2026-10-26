#include "menu/item_menu.hpp"

#include <algorithm>
#include <cstdio>

#include "field/map.hpp"

namespace ff6 {
namespace {

const char* const kOptions[FF6_ITEM_OPTION_COUNT] = {"USE", "ARRANGE", "RARE"};

}  // namespace

void ItemMenu::open(const std::vector<MenuItem>& items) {
    items_ = items;
    closed_ = false;
    inList_ = false;
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

void ItemMenu::update(const InputState& input) {
    const int direction = repeat_.update(input.direction);
    if (inList_) {
        if (direction == Up) move(-1);
        if (direction == Down) move(1);
        if (input.cancel) {
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
            inList_ = false;
        } else if (input.confirm) {
            sound_.command(FF6_SFX_MENU_INVALID, 0, 0);  // the items cannot be used yet
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
    } else if (input.confirm) {
        sound_.command(FF6_SFX_MENU_INVALID, 0, 0);  // arrange and rare items are not ported yet
    }
}

void ItemMenu::draw() {
    using Layer = MenuScreen::Layer;
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
    for (int row = 0; row < FF6_ITEM_ROWS; ++row) {
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
    if (inList_) {
        screen_.hand(FF6_ITEM_CURSOR_X, FF6_ITEM_LIST_TOP + (selected_ - first_) * FF6_ITEM_ROW_HEIGHT + FF6_ITEM_CURSOR_DY);
    } else {
        static constexpr int kCursorX[FF6_ITEM_OPTION_COUNT] = FF6_ITEM_OPTION_CURSOR_X;
        screen_.hand(kCursorX[option_], FF6_ITEM_OPTION_CURSOR_Y);
    }
}

}  // namespace ff6
