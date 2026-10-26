#pragma once

#include "input.hpp"
#include "menu/menu_config.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// The config menu (src/menu/config.asm). The first page holds the options that the left and right buttons change, the
// second one the order of the magic, the pattern of the windows and their colors, and the color of the font. The
// pages are one above the other and scroll.
class ConfigMenu {
public:
    ConfigMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    // The settings must stay valid while the menu is open.
    void open(GameConfig& config);
    void update(const InputState& input);
    void draw();
    // True once the player has asked to go back.
    bool closed() const { return closed_; }
    int cursor() const { return cursor_; }
    int page() const { return page_; }
    bool scrolling() const { return scrollFrames_ != 0; }

private:
    void changePage1(int direction);
    void changePage2(int direction);
    void revertColor();
    void startScroll(int direction);
    void drawPage1(int offset);
    void drawPage2(int offset);

    MenuScreen& screen_;
    Sound& sound_;
    GameConfig* config_ = nullptr;
    int page_ = 0;  // 0 or 1
    int cursor_ = 0;
    int scroll_ = FF6_CONFIG_SCROLL_FIRST;  // the vertical scroll of the text of the pages
    int scrollFrames_ = 0, scrollDirection_ = 0;
    bool closed_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
