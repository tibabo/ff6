#include "menu/load_menu.hpp"

#include "field/map.hpp"

namespace ff6 {
namespace {

using Layer = MenuScreen::Layer;

constexpr int kWindowX = 10, kWindowY = 10, kWindowWidth = 12, kWindowHeight = 4;
constexpr int kOptionX = 12, kOptionRows[2] = {11, 13};

}  // namespace

void LoadMenu::open(bool canContinue) {
    canContinue_ = canContinue;
    closed_ = false;
    option_ = canContinue ? 1 : 0;  // a saved game is the likelier choice
    repeat_.reset();
}

void LoadMenu::update(const InputState& input) {
    const int direction = repeat_.update(input.direction);
    if ((direction == Up && option_ > 0) || (direction == Down && option_ < 1)) {
        option_ += direction == Down ? 1 : -1;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (!input.confirm) return;
    if (option_ == 1 && !canContinue_) {
        sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
        return;
    }
    sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
    closed_ = true;
}

void LoadMenu::draw() {
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(kWindowX, kWindowY, kWindowWidth, kWindowHeight);
    screen_.text(kOptionX, kOptionRows[0], "New Game", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    screen_.text(kOptionX, kOptionRows[1], "Continue", Layer::Bg3, canContinue_ ? FF6_MENU_BG3_DEFAULT : FF6_MENU_BG3_GRAY);
    screen_.hand(kOptionX * FF6_MENU_TILE_SIZE - FF6_MENU_CURSOR_SIZE, kOptionRows[option_] * FF6_MENU_TILE_SIZE + FF6_ITEM_CURSOR_DY);
}

}  // namespace ff6
