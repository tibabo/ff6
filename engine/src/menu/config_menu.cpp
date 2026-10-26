#include "menu/config_menu.hpp"

#include <cstdio>

#include "field/map.hpp"

namespace ff6 {
namespace {

enum Row1 { BattleMode, BattleSpeed, MessageSpeed, CommandSet, Gauge, Sound, Cursor, Reequip, Controller };
enum Row2 { MagicOrder, WindowPattern, ColorSlot, Red, Green, Blue };

// An option with two choices: the first is selected when the bit is clear, the second when it is set.
struct Toggle {
    Row1 row;
    uint8_t GameConfig::*byte;
    uint8_t mask;
    const char* first;
    const char* second;
};

const Toggle kToggles[] = {
    {BattleMode, &GameConfig::battle, FF6_CONFIG_WAIT, "Active", "Wait"},
    {CommandSet, &GameConfig::battle, FF6_CONFIG_COMMAND_SHORT, "Window", "Short"},
    {Gauge, &GameConfig::display, FF6_CONFIG_GAUGE_OFF, "On", "Off"},
    {Sound, &GameConfig::display, FF6_CONFIG_MONO, "Stereo", "Mono"},
    {Cursor, &GameConfig::display, FF6_CONFIG_CURSOR_MEMORY, "Reset", "Memory"},
    {Reequip, &GameConfig::display, FF6_CONFIG_REEQUIP_EMPTY, "Optimum", "Empty"},
    {Controller, &GameConfig::controller, FF6_CONFIG_MULTIPLE_CONTROLLERS, "Single", "Multiple"},
};

int textRow(Row1 row) { return FF6_CONFIG_FIRST_ROW + 2 * row; }

// The kinds of magic, in the order that each of the six orders lists them: healing, attack, effect.
const int kMagicOrders[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 2, 0}, {1, 0, 2}, {2, 0, 1}, {2, 1, 0}};
const char* const kMagicNames[3] = {"Healing", "Attack", "Effect"};

std::string number2(int value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%2d", value);
    return buffer;
}

// The component of a BGR555 color: 0 red, 1 green, 2 blue.
int component(uint16_t color, int which) { return (color >> (which * FF6_COLOR_BITS)) & FF6_CONFIG_COMPONENT_MAX; }

uint16_t withComponent(uint16_t color, int which, int value) {
    const int shift = which * FF6_COLOR_BITS;
    return static_cast<uint16_t>((color & ~(FF6_CONFIG_COMPONENT_MAX << shift)) | (value << shift));
}

}  // namespace

void ConfigMenu::open(GameConfig& config) {
    config_ = &config;
    closed_ = false;
    page_ = 0;
    cursor_ = 0;
    scroll_ = FF6_CONFIG_SCROLL_FIRST;
    scrollFrames_ = 0;
    repeat_.reset();
    screen_.applyConfig(config);
}

void ConfigMenu::startScroll(int direction) {
    sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    scrollFrames_ = FF6_CONFIG_PAGE_SCROLL_FRAMES;
    scrollDirection_ = direction;
}

void ConfigMenu::changePage1(int direction) {
    const bool right = direction == Right;
    sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    if (cursor_ == BattleSpeed || cursor_ == MessageSpeed) {
        const bool message = cursor_ == MessageSpeed;
        const int shift = message ? FF6_CONFIG_MESSAGE_SPEED_SHIFT : 0;
        const int mask = message ? FF6_CONFIG_MESSAGE_SPEED_MASK : FF6_CONFIG_BATTLE_SPEED_MASK;
        int speed = (config_->battle & mask) >> shift;
        if (right ? speed == FF6_CONFIG_SPEED_MAX : speed == 0) return;
        speed += right ? 1 : -1;
        config_->battle = static_cast<uint8_t>((config_->battle & ~mask) | (speed << shift));
        return;
    }
    for (const Toggle& toggle : kToggles) {
        if (toggle.row != cursor_) continue;
        uint8_t& value = config_->*toggle.byte;
        value = static_cast<uint8_t>(right ? (value | toggle.mask) : (value & ~toggle.mask));
    }
}

// Each row steps a value by one, and stops at its limits; the sound is played even then.
void ConfigMenu::changePage2(int direction) {
    const int step = direction == Right ? 1 : -1;
    sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    switch (cursor_) {
        case MagicOrder: {
            const int order = config_->magicOrder() + step;
            if (order < 0 || order > FF6_CONFIG_ORDER_MAX) return;
            config_->controller = static_cast<uint8_t>((config_->controller & ~FF6_CONFIG_ORDER_MASK) | order);
            break;
        }
        case WindowPattern: {
            const int pattern = config_->windowPattern() + step;
            if (pattern < 0 || pattern > FF6_CONFIG_WINDOW_MAX) return;
            config_->display = static_cast<uint8_t>((config_->display & ~FF6_CONFIG_WINDOW_MASK) | pattern);
            break;
        }
        case ColorSlot: {
            const int slot = config_->colorSlot() + step;
            if (slot < 0 || slot > FF6_CONFIG_COLOR_MAX) return;
            config_->controller = static_cast<uint8_t>((config_->controller & ~FF6_CONFIG_COLOR_MASK) |
                                                       (slot << FF6_CONFIG_COLOR_SHIFT));
            break;
        }
        default: {
            const int which = cursor_ - Red;  // red, green and blue are the bits 0 to 4, 5 to 9 and 10 to 14
            uint16_t& color = config_->editedColor();
            const int value = component(color, which) + step;
            if (value < 0 || value > FF6_CONFIG_COMPONENT_MAX) return;
            color = withComponent(color, which, value);
            break;
        }
    }
    screen_.applyConfig(*config_);
}

// The A button on the colors: the color of the font, or the colors of this pattern of window, start again.
void ConfigMenu::revertColor() {
    sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
    if (config_->colorSlot() == 0) {
        config_->fontColor = FF6_CONFIG_FONT_COLOR_DEFAULT;
    } else {
        config_->windowColors[static_cast<size_t>(config_->windowPattern())] =
            screen_.data().windowColors[static_cast<size_t>(config_->windowPattern())];
    }
    screen_.applyConfig(*config_);
}

void ConfigMenu::update(const InputState& input) {
    if (scrollFrames_ != 0) {
        scroll_ += scrollDirection_ * FF6_CONFIG_PAGE_SCROLL_STEP;
        if (--scrollFrames_ == 0) {
            page_ = scrollDirection_ > 0 ? 1 : 0;
            cursor_ = page_ == 1 ? 0 : FF6_CONFIG_PAGE1_ROWS - 1;
            repeat_.reset();
        }
        return;
    }
    const int rows = page_ == 0 ? FF6_CONFIG_PAGE1_ROWS : FF6_CONFIG_PAGE2_ROWS;
    const int direction = repeat_.update(input.direction);
    if (direction == Down) {  // the cursor does not wrap: past the last option of a page, the next page scrolls in
        if (cursor_ < rows - 1) {
            ++cursor_;
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        } else if (page_ == 0) {
            startScroll(1);
        }
    } else if (direction == Up) {
        if (cursor_ > 0) {
            --cursor_;
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        } else if (page_ == 1) {
            startScroll(-1);
        }
    } else if (direction == Left || direction == Right) {
        if (page_ == 0) {
            changePage1(direction);
        } else {
            changePage2(direction);
        }
    }
    if (input.cancel) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        closed_ = true;
    } else if (input.confirm && page_ == 1 && cursor_ >= ColorSlot) {
        revertColor();
    }
}

void ConfigMenu::drawPage1(int offset) {
    using Layer = MenuScreen::Layer;
    screen_.setTextOffset(offset);
    static const struct { int row; const char* label; } kLabels[] = {
        {5, "Bat.Mode"}, {7, "Bat.Speed"}, {9, "Msg.Speed"}, {11, "Cmd.Set"}, {13, "Gauge"},
        {15, "Sound"}, {17, "Cursor"}, {19, "Reequip"}, {21, "Controller"}};
    for (const auto& label : kLabels) screen_.text(3, label.row, label.label, Layer::Bg1, FF6_MENU_BG1_TEAL);
    screen_.text(14, 8, "Fast", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(22, 8, "Slow", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    for (const Toggle& toggle : kToggles) {
        const bool second = (config_->*toggle.byte & toggle.mask) != 0;
        screen_.text(FF6_CONFIG_FIRST_COLUMN, textRow(toggle.row), toggle.first, Layer::Bg1,
                     second ? FF6_MENU_BG1_GRAY : FF6_MENU_BG1_DEFAULT);
        screen_.text(FF6_CONFIG_SECOND_COLUMN, textRow(toggle.row), toggle.second, Layer::Bg1,
                     second ? FF6_MENU_BG1_DEFAULT : FF6_MENU_BG1_GRAY);
    }
    for (const Row1 row : {BattleSpeed, MessageSpeed}) {  // the numbers are gray and the one in use is white
        const int selected = row == BattleSpeed ? config_->battleSpeed() : config_->messageSpeed();
        for (int speed = 0; speed <= FF6_CONFIG_SPEED_MAX; ++speed) {
            screen_.text(FF6_CONFIG_FIRST_COLUMN + 2 * speed, textRow(row), std::string(1, static_cast<char>('1' + speed)),
                         Layer::Bg1, speed == selected ? FF6_MENU_BG1_DEFAULT : FF6_MENU_BG1_GRAY);
        }
    }
}

// The second page is drawn on the tile rows of the screen below the first.
void ConfigMenu::drawPage2(int offset) {
    using Layer = MenuScreen::Layer;
    screen_.setTextOffset(offset);
    const auto text = [&](int x, int row, const std::string& value, int color) {
        screen_.text(x, row, value, Layer::Bg1, color);
    };
    const auto numerals = [&](int row, int count, int selected) {
        for (int i = 0; i < count; ++i) {
            text(FF6_CONFIG_FIRST_COLUMN + 2 * i, row, std::string(1, static_cast<char>('1' + i)),
                 i == selected ? FF6_MENU_BG1_DEFAULT : FF6_MENU_BG1_GRAY);
        }
    };
    text(3, 5, "Mag.Order", FF6_MENU_BG1_TEAL);
    text(3, 13, "Window", FF6_MENU_BG1_TEAL);
    text(3, 15, "Color", FF6_MENU_BG1_TEAL);
    numerals(5, FF6_CONFIG_ORDER_MAX + 1, config_->magicOrder());
    numerals(13, FF6_CONFIG_WINDOW_MAX + 1, config_->windowPattern());
    for (int line = 0; line < 3; ++line) {  // A, B and C hold a kind of magic each
        const int row = 7 + 2 * line;
        const int kind = kMagicOrders[config_->magicOrder()][line];
        screen_.codes(FF6_CONFIG_FIRST_COLUMN, row, {menuCode(static_cast<char>('A' + line)), FF6_MENU_CODE_2DOT}, Layer::Bg1,
                      FF6_MENU_BG1_DEFAULT);
        screen_.codes(16, row, {static_cast<uint8_t>(FF6_MENU_CODE_WHITE + kind)}, Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        text(17, row, kMagicNames[kind], FF6_MENU_BG1_DEFAULT);
    }
    const bool font = config_->colorSlot() == 0;
    text(14, 15, "Font", font ? FF6_MENU_BG1_DEFAULT : FF6_MENU_BG1_GRAY);
    text(22, 15, "Window", font ? FF6_MENU_BG1_GRAY : FF6_MENU_BG1_DEFAULT);
    const auto& colors = config_->windowColors[static_cast<size_t>(config_->windowPattern())];
    for (int i = 0; i < FF6_MENU_WINDOW_COLORS; ++i) screen_.swatch(22 + i, 17, colors[static_cast<size_t>(i)]);
    if (!font) {
        screen_.codes(21 + config_->colorSlot(), 18, {FF6_MENU_CODE_UP_ARROW}, Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    }
    const uint16_t edited = config_->editedColor();
    static const char* const kLetters[3] = {"R", "G", "B"};
    for (int which = 0; which < 3; ++which) {
        const int row = 19 + 2 * which;
        const int value = component(edited, which);
        text(14, row, kLetters[which], FF6_MENU_BG1_DEFAULT);
        text(16, row, number2(value), FF6_MENU_BG1_DEFAULT);
        std::vector<uint8_t> bar = {FF6_MENU_CODE_GAUGE_LEFT};  // the frame of the bar, then what is filled of it
        bar.insert(bar.end(), FF6_CONFIG_BAR_CELLS, FF6_MENU_CODE_GAUGE_EMPTY);
        bar.push_back(FF6_MENU_CODE_GAUGE_RIGHT);
        screen_.codes(18, row, bar, Layer::Bg1, FF6_MENU_BG1_COLOR_BAR);
        std::vector<uint8_t> filled(static_cast<size_t>(value / 4), FF6_MENU_CODE_GAUGE_FULL);
        filled.push_back(static_cast<uint8_t>(FF6_MENU_CODE_GAUGE_EMPTY + 2 * (value & 3)));
        screen_.codes(19, row, filled, Layer::Bg1, FF6_MENU_BG1_COLOR_BAR);
    }
}

void ConfigMenu::draw() {
    using Layer = MenuScreen::Layer;
    screen_.clear();
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(1, 3, 28, 22);
    screen_.window(23, 1, 6, 2);
    screen_.setTextOffset(FF6_CONFIG_TEXT_OFFSET_Y);  // the third background is scrolled by 5 pixels
    screen_.text(24, 2, "Config", Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.setClip(FF6_CONFIG_CLIP_TOP, FF6_CONFIG_CLIP_BOTTOM);  // the pages scroll behind the frame of the window
    drawPage1(-scroll_);
    drawPage2(FF6_CONFIG_PAGE_PIXELS - scroll_);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.setTextOffset(0);
    static constexpr int kPage2Y[FF6_CONFIG_PAGE2_ROWS] = FF6_CONFIG_PAGE2_CURSOR_Y;
    const int y = page_ == 0 ? FF6_CONFIG_CURSOR_Y + FF6_CONFIG_CURSOR_STEP * cursor_ : kPage2Y[cursor_];
    screen_.hand(FF6_CONFIG_CURSOR_X, y);
}

}  // namespace ff6
