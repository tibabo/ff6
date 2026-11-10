#include "menu/colosseum_menu.hpp"

#include <algorithm>
#include <cstdio>

#include "field/map.hpp"

namespace ff6 {
namespace {

using Layer = MenuScreen::Layer;

std::vector<uint8_t> itemName(const MenuData& data, int item) {
    const uint8_t* name = data.itemNames.data() + static_cast<size_t>(item) * FF6_ITEM_NAME_BYTES;
    return std::vector<uint8_t>(name, name + FF6_ITEM_NAME_BYTES);
}

std::string number(int value, int width) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%*d", width, value);
    return buffer;
}

}  // namespace

void ColosseumMenu::open(const ColosseumHooks& hooks) {
    hooks_ = hooks;
    items_ = hooks_.items ? hooks_.items() : std::vector<MenuItem>();
    party_ = hooks_.party ? hooks_.party() : std::vector<MenuCharacter>();
    mode_ = ColosseumMode::Items;
    selected_ = first_ = target_ = 0;
    item_ = FF6_ITEM_EMPTY;
    character_ = -1;
    closed_ = chosen_ = false;
    match_ = ColosseumMatch();
    repeat_.reset();
}

void ColosseumMenu::update(const InputState& input) {
    const int direction = repeat_.update(input.direction);
    if (mode_ == ColosseumMode::Items) {
        const int count = static_cast<int>(items_.size());
        const int before = selected_;
        if (direction == Up) selected_ = std::max(selected_ - 1, 0);
        if (direction == Down) selected_ = std::min(selected_ + 1, count - 1);
        if (selected_ != before) {
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
            if (selected_ < first_) first_ = selected_;
            if (selected_ >= first_ + FF6_ITEM_ROWS) first_ = selected_ - FF6_ITEM_ROWS + 1;
        }
        if (input.cancel) {
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
            closed_ = true;
        } else if (input.confirm) {
            if (count == 0) {
                sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
                return;
            }
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
            item_ = items_[static_cast<size_t>(selected_)].id;
            match_ = hooks_.match ? hooks_.match(item_) : ColosseumMatch();
            mode_ = ColosseumMode::Challenger;
            target_ = 0;
        }
        return;
    }
    const int count = static_cast<int>(party_.size());
    if ((direction == Up || direction == Down) && count > 0) {
        target_ = (target_ + (direction == Down ? 1 : count - 1)) % count;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (input.cancel) {  // back to the choice of the item
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        mode_ = ColosseumMode::Items;
        item_ = FF6_ITEM_EMPTY;
    } else if (input.confirm && count > 0) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        character_ = party_[static_cast<size_t>(target_)].id;
        chosen_ = closed_ = true;
    }
}

void ColosseumMenu::drawItems() {
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(1, 1, 5, 2);
    screen_.window(8, 1, 21, 2);
    screen_.window(1, 5, 28, 3);
    screen_.window(1, 10, 28, 15);
    screen_.text(2, 2, "Item", Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.text(9, 2, "Choose an item to wager", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    const MenuData& data = screen_.data();
    for (int row = 0; row < FF6_ITEM_ROWS; ++row) {
        const size_t place = static_cast<size_t>(first_ + row);
        if (place >= items_.size()) break;
        const int y = FF6_ITEM_LIST_TOP + row * FF6_ITEM_ROW_HEIGHT;
        screen_.setTextOffset(y % FF6_MENU_TILE_SIZE);
        std::vector<uint8_t> codes = itemName(data, items_[place].id);
        codes.push_back(menuCode(':'));
        screen_.codes(FF6_ITEM_NAME_X, y / FF6_MENU_TILE_SIZE, codes, Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(FF6_ITEM_QUANTITY_X, y / FF6_MENU_TILE_SIZE, number(items_[place].quantity, 2), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    }
    screen_.setTextOffset(0);
    if (!items_.empty()) {
        const std::vector<uint8_t> text = data.itemDescription(items_[static_cast<size_t>(selected_)].id);
        std::vector<uint8_t> line;
        int row = 0;
        for (size_t i = 0; i <= text.size(); ++i) {
            if (i < text.size() && text[i] != FF6_ITEM_DESC_NEWLINE) {
                line.push_back(text[i]);
                continue;
            }
            const int y = FF6_ITEM_DESC_TOP + row++ * FF6_ITEM_ROW_HEIGHT;
            screen_.setTextOffset(y % FF6_MENU_TILE_SIZE);
            screen_.codes(FF6_ITEM_DESC_X, y / FF6_MENU_TILE_SIZE, line, Layer::Bg3, FF6_MENU_BG3_DEFAULT);
            line.clear();
        }
        screen_.setTextOffset(0);
        screen_.hand(FF6_ITEM_CURSOR_X, FF6_ITEM_LIST_TOP + (selected_ - first_) * FF6_ITEM_ROW_HEIGHT + FF6_ITEM_CURSOR_DY);
    }
}

// The prize and the wager, the opponent, and the characters to choose from.
void ColosseumMenu::drawChallenger() {
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(1, 1, 13, 5);
    screen_.window(16, 1, 13, 5);
    screen_.window(1, 7, 28, 4);
    screen_.window(1, 12, 28, 13);
    screen_.text(2, 2, "Prize", Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.text(17, 2, "Wager", Layer::Bg3, FF6_MENU_BG3_TEAL);
    if (match_.prizeHidden || match_.prize == FF6_ITEM_EMPTY) {
        screen_.text(2, 4, "?????", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    } else {
        screen_.codes(2, 4, itemName(screen_.data(), match_.prize), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    }
    screen_.codes(17, 4, itemName(screen_.data(), item_), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    screen_.text(2, 8, "VS", Layer::Bg3, FF6_MENU_BG3_YELLOW);
    if (match_.shadow) {
        screen_.text(6, 8, "Shadow", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    } else if (hooks_.monsterName) {
        screen_.codes(6, 8, hooks_.monsterName(match_.monster), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    }
    for (size_t slot = 0; slot < party_.size() && slot < FF6_PARTY_SIZE; ++slot) {
        const MenuCharacter& c = party_[slot];
        const int row = 13 + static_cast<int>(slot) * 3;
        screen_.text(4, row, characterName(c.id), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(14, row, "LV", Layer::Bg1, FF6_MENU_BG1_TEAL);
        screen_.text(17, row, number(c.level, 2), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(21, row, "HP", Layer::Bg1, FF6_MENU_BG1_TEAL);
        screen_.text(24, row, number(c.hp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    }
    if (!party_.empty()) screen_.hand(FF6_ITEM_CURSOR_X, (13 + target_ * 3) * FF6_MENU_TILE_SIZE + FF6_ITEM_CURSOR_DY);
}

void ColosseumMenu::draw() {
    if (mode_ == ColosseumMode::Items) {
        drawItems();
    } else {
        drawChallenger();
    }
}

}  // namespace ff6
