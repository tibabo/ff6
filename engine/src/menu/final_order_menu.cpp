#include "menu/final_order_menu.hpp"

#include "field/map.hpp"

namespace ff6 {
namespace {

constexpr int kEnd = FinalOrderMenu::kSlots + 1;

}  // namespace

void FinalOrderMenu::open(const std::vector<int>& characters) {
    available_ = characters;
    available_.resize(kSlots, -1);
    used_.assign(kSlots, false);
    order_.clear();
    cursor_ = 1;
    closed_ = false;
    repeat_.reset();
}

// The characters that were not chosen follow in the order that they have on the left.
void FinalOrderMenu::finish() {
    for (int i = 0; i < kSlots; ++i) {
        if (!used_[static_cast<size_t>(i)] && available_[static_cast<size_t>(i)] >= 0) order_.push_back(available_[static_cast<size_t>(i)]);
    }
    closed_ = true;
}

void FinalOrderMenu::update(const InputState& input) {
    const int direction = repeat_.update(input.direction);
    if (direction == Up || direction == Down) {
        cursor_ = (cursor_ + (direction == Down ? 1 : kEnd)) % (kEnd + 1);
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (input.start) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        finish();
    } else if (input.confirm) {
        if (cursor_ == 0) {
            order_.clear();
            used_.assign(kSlots, false);
            cursor_ = 1;
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        } else if (cursor_ == kEnd) {
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
            finish();
        } else if (available_[static_cast<size_t>(cursor_ - 1)] >= 0 && !used_[static_cast<size_t>(cursor_ - 1)]) {
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
            used_[static_cast<size_t>(cursor_ - 1)] = true;
            order_.push_back(available_[static_cast<size_t>(cursor_ - 1)]);
            if (static_cast<int>(order_.size()) == kSlots) cursor_ = kEnd;
        } else {
            sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
        }
    } else if (input.cancel) {  // the last one chosen is taken back
        if (order_.empty()) {
            sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
            return;
        }
        for (int i = 0; i < kSlots; ++i) {
            if (available_[static_cast<size_t>(i)] == order_.back()) used_[static_cast<size_t>(i)] = false;
        }
        order_.pop_back();
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
}

void FinalOrderMenu::draw() {
    using Layer = MenuScreen::Layer;
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(1, 1, 28, 24);
    screen_.text(3, 3, "Reset", Layer::Bg1, cursor_ == 0 ? FF6_MENU_BG1_TEAL : FF6_MENU_BG1_DEFAULT);
    screen_.text(22, 3, "End", Layer::Bg1, cursor_ == kEnd ? FF6_MENU_BG1_TEAL : FF6_MENU_BG1_DEFAULT);
    for (int i = 0; i < kSlots; ++i) {
        const int who = available_[static_cast<size_t>(i)];
        if (who >= 0) {
            screen_.text(6, 6 + i * 1, characterName(who), Layer::Bg1, used_[static_cast<size_t>(i)] ? FF6_MENU_BG1_GRAY : FF6_MENU_BG1_DEFAULT);
        }
        if (i < static_cast<int>(order_.size())) screen_.text(20, 6 + i, characterName(order_[static_cast<size_t>(i)]), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(18, 6 + i, std::to_string(i + 1), Layer::Bg1, FF6_MENU_BG1_TEAL);
    }
    const int row = cursor_ == 0 || cursor_ == kEnd ? 3 : 5 + cursor_;
    const int column = cursor_ == kEnd ? 21 : cursor_ == 0 ? 2 : 5;
    screen_.hand(column * FF6_MENU_TILE_SIZE - FF6_MENU_CURSOR_SIZE / 2, row * FF6_MENU_TILE_SIZE + FF6_ITEM_CURSOR_DY);
}

}  // namespace ff6
