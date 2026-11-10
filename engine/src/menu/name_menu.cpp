#include "menu/name_menu.hpp"

#include "field/map.hpp"

namespace ff6 {
namespace {

constexpr const char* kRows[] = {"ABCDEabcde", "FGHIJfghij", "KLMNOklmno", "PQRSTpqrst", "UVWXYuvwxy", "Z!?/:z\"'-.", "0123456789"};
constexpr int kRowCount = 7, kColumns = 10;
constexpr int kGridX = 9, kGridY = 10;  // in tiles; a letter every two tiles across and down

}  // namespace

void NameMenu::open(int character, const std::string& current) {
    character_ = character;
    name_ = current.substr(0, kMaxLength);
    row_ = column_ = 0;
    closed_ = false;
    repeat_.reset();
}

void NameMenu::update(const InputState& input) {
    const int direction = repeat_.update(input.direction);
    if (direction == Up || direction == Down) row_ = (row_ + (direction == Down ? 1 : kRowCount - 1)) % kRowCount;
    if (direction == Left || direction == Right) column_ = (column_ + (direction == Right ? 1 : kColumns - 1)) % kColumns;
    if (direction != FF6_NO_DIRECTION) sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    if (input.start) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        closed_ = true;
    } else if (input.confirm) {
        if (static_cast<int>(name_.size()) < kMaxLength) {
            name_.push_back(kRows[row_][column_]);
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        } else {
            sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
        }
    } else if (input.cancel) {
        if (!name_.empty()) {
            name_.pop_back();
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        } else {
            sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
        }
    }
}

void NameMenu::draw() {
    using Layer = MenuScreen::Layer;
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(1, 1, 5, 5);
    screen_.window(8, 1, 20, 2);
    screen_.window(9, 5, 18, 2);
    screen_.window(7, 8, 22, 17);
    screen_.portrait(8, 8, menuPortrait(character_));
    screen_.text(9, 2, "Please enter a name.", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    std::string shown = name_;
    while (static_cast<int>(shown.size()) < kMaxLength) shown.push_back('_');
    screen_.text(10, 6, shown, Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    for (int row = 0; row < kRowCount; ++row) {
        for (int column = 0; column < kColumns; ++column) {
            screen_.text(kGridX + column * 2, kGridY + row * 2, std::string(1, kRows[row][column]), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        }
    }
    screen_.hand((kGridX + column_ * 2) * FF6_MENU_TILE_SIZE - FF6_MENU_CURSOR_SIZE, (kGridY + row_ * 2) * FF6_MENU_TILE_SIZE + FF6_ITEM_CURSOR_DY);
}

}  // namespace ff6
