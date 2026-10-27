#include "menu/shop_menu.hpp"

#include <algorithm>
#include <cstdio>

#include "field/map.hpp"
namespace ff6 {
namespace {

using Layer = MenuScreen::Layer;

const char* const kKinds[] = {"", "Weapon", "Armor", "Item", "Relics", "Vendor"};
const char* const kGreeting = "Hi! Can I help you?";
constexpr int kOptionColumns[3] = FF6_SHOP_OPTION_COLUMNS;

std::string number(long value, int width) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%*ld", width, value);
    return buffer;
}

std::vector<uint8_t> itemName(const MenuData& data, int item) {
    const uint8_t* name = data.itemNames.data() + static_cast<size_t>(item) * FF6_ITEM_NAME_BYTES;
    return std::vector<uint8_t>(name, name + FF6_ITEM_NAME_BYTES);
}

}  // namespace

void ShopMenu::open(int kind, const std::vector<int>& stock, const ShopHooks& hooks) {
    kind_ = kind;
    stock_ = stock;
    hooks_ = hooks;
    mode_ = ShopMode::Options;
    message_ = kGreeting;
    option_ = selected_ = first_ = 0;
    quantity_ = maxQuantity_ = 1;
    closed_ = false;
    repeat_.reset();
}

int ShopMenu::owned(int item) const {
    if (!hooks_.items) return 0;
    for (const MenuItem& held : hooks_.items()) {
        if (held.id == item) return held.quantity;
    }
    return 0;
}

int ShopMenu::currentItem() const {
    if (mode_ == ShopMode::BuyList || mode_ == ShopMode::BuyQuantity) {
        return selected_ < static_cast<int>(stock_.size()) ? stock_[static_cast<size_t>(selected_)] : FF6_ITEM_EMPTY;
    }
    return selected_ < static_cast<int>(inventory_.size()) ? inventory_[static_cast<size_t>(selected_)].id : FF6_ITEM_EMPTY;
}

// How many can be bought: no more than the inventory holds and the gil pays.
void ShopMenu::enterBuyQuantity() {
    const int item = currentItem();
    const int price = hooks_.buyPrice ? hooks_.buyPrice(item) : 0;
    const int have = owned(item);
    const long gil = hooks_.gil ? hooks_.gil() : 0;
    if (have >= FF6_MAX_ITEM_QUANTITY) {
        message_ = "Too many!";
    } else if (price > gil) {
        message_ = "You need more GP!";
    } else {
        maxQuantity_ = std::min<long>(FF6_MAX_ITEM_QUANTITY - have, price > 0 ? gil / price : FF6_MAX_ITEM_QUANTITY);
        quantity_ = 1;
        mode_ = ShopMode::BuyQuantity;
        message_ = "How many?";
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        return;
    }
    sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
}

void ShopMenu::enterSellQuantity() {
    const MenuItem& item = inventory_[static_cast<size_t>(selected_)];
    if (!hooks_.sellPrice || hooks_.sellPrice(item.id) <= 0) {
        sound_.command(FF6_SFX_MENU_INVALID, 0, 0);  // the shop does not buy it
        return;
    }
    maxQuantity_ = item.quantity;
    quantity_ = 1;
    mode_ = ShopMode::SellQuantity;
    message_ = "How many?";
    sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
}

// Left and right change the quantity by one, up and down by ten; it stays between one and the most that fits.
void ShopMenu::updateQuantity(int direction) {
    const int before = quantity_;
    if (direction == Right) quantity_ = std::min(quantity_ + 1, maxQuantity_);
    if (direction == Left) quantity_ = std::max(quantity_ - 1, 1);
    if (direction == Up) quantity_ = std::min(quantity_ + 10, maxQuantity_);
    if (direction == Down) quantity_ = std::max(quantity_ - 10, 1);
    if (quantity_ != before) sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
}

void ShopMenu::update(const InputState& input) {
    const int direction = repeat_.update(input.direction);
    switch (mode_) {
        case ShopMode::Options:
            if ((direction == Left && option_ > 0) || (direction == Right && option_ < 2)) {
                option_ += direction == Right ? 1 : -1;
                sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
            }
            if (input.cancel || (input.confirm && option_ == 2)) {
                sound_.command(input.cancel ? FF6_SFX_MENU_MOVE : FF6_SFX_MENU_SELECT, 0, 0);
                closed_ = true;
            } else if (input.confirm && option_ == 0 && !stock_.empty()) {
                sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
                mode_ = ShopMode::BuyList;
                selected_ = first_ = 0;
                message_ = "Help yourself!";
            } else if (input.confirm && option_ == 1) {
                inventory_ = hooks_.items ? hooks_.items() : std::vector<MenuItem>();
                if (inventory_.empty()) {
                    sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
                } else {
                    sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
                    mode_ = ShopMode::SellList;
                    selected_ = first_ = 0;
                    message_ = "Whatcha got?";
                }
            } else if (input.confirm) {
                sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
            }
            break;
        case ShopMode::BuyList:
        case ShopMode::SellList: {
            const bool buying = mode_ == ShopMode::BuyList;
            const int count = static_cast<int>(buying ? stock_.size() : inventory_.size());
            const int rows = buying ? FF6_SHOP_ITEMS : FF6_SHOP_SELL_ROWS;
            const int before = selected_;
            if (direction == Up) selected_ = std::max(selected_ - 1, 0);
            if (direction == Down) selected_ = std::min(selected_ + 1, count - 1);
            if (selected_ != before) {
                sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
                if (selected_ < first_) first_ = selected_;
                if (selected_ >= first_ + rows) first_ = selected_ - rows + 1;
            }
            if (input.cancel) {
                sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
                mode_ = ShopMode::Options;
                message_ = kGreeting;
            } else if (input.confirm) {
                if (buying) {
                    enterBuyQuantity();
                } else {
                    enterSellQuantity();
                }
            }
            break;
        }
        case ShopMode::BuyQuantity:
        case ShopMode::SellQuantity: {
            const bool buying = mode_ == ShopMode::BuyQuantity;
            updateQuantity(direction);
            if (input.cancel) {
                sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
                mode_ = buying ? ShopMode::BuyList : ShopMode::SellList;
                message_ = buying ? "Help yourself!" : "Whatcha got?";
            } else if (input.confirm) {
                const int item = currentItem();
                const bool done = buying ? hooks_.buy && hooks_.buy(item, quantity_) : hooks_.sell && hooks_.sell(item, quantity_);
                if (!done) {
                    sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
                    break;
                }
                sound_.command(FF6_SFX_MENU_SUCCESS, 0, 0);
                message_ = "Bye!";
                mode_ = buying ? ShopMode::BuyList : ShopMode::SellList;
                if (!buying) {
                    inventory_ = hooks_.items ? hooks_.items() : std::vector<MenuItem>();
                    if (inventory_.empty()) {
                        mode_ = ShopMode::Options;
                    } else {
                        selected_ = std::min(selected_, static_cast<int>(inventory_.size()) - 1);
                        first_ = std::max(0, std::min(first_, selected_));
                    }
                }
            }
            break;
        }
    }
}

// The title and the message on top; the gil is there too when the options are.
void ShopMenu::drawTop(bool showGil) {
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(1, 1, 7, 2);
    screen_.window(10, 1, 19, 2);
    screen_.text(2, 2, kKinds[kind_ >= 1 && kind_ <= 5 ? kind_ : 0], Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.text(11, 2, message_, Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    if (showGil) {
        screen_.window(1, 5, 16, 2);
        screen_.window(19, 5, 10, 2);
        screen_.text(kOptionColumns[0], FF6_SHOP_OPTION_ROW, "BUY", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.text(kOptionColumns[1], FF6_SHOP_OPTION_ROW, "SELL", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.text(kOptionColumns[2], FF6_SHOP_OPTION_ROW, "EXIT", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.text(20, FF6_SHOP_OPTION_ROW, number(hooks_.gil ? hooks_.gil() : 0, 7), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.text(28, FF6_SHOP_OPTION_ROW, "GP", Layer::Bg3, FF6_MENU_BG3_TEAL);
    }
}

void ShopMenu::drawBuyList() {
    drawTop(false);
    screen_.window(1, 5, 28, 12);
    screen_.window(1, 19, 28, 5);
    const MenuData& data = screen_.data();
    for (size_t i = 0; i < stock_.size(); ++i) {
        const int y = FF6_SHOP_BUY_TOP + static_cast<int>(i) * FF6_ITEM_ROW_HEIGHT;
        screen_.setTextOffset(y % FF6_MENU_TILE_SIZE);
        screen_.codes(FF6_SHOP_NAME_X, y / FF6_MENU_TILE_SIZE, itemName(data, stock_[i]), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        const int price = hooks_.buyPrice ? hooks_.buyPrice(stock_[i]) : 0;
        screen_.text(FF6_SHOP_PRICE_X, y / FF6_MENU_TILE_SIZE, number(price, 6), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(FF6_SHOP_PRICE_X + 7, y / FF6_MENU_TILE_SIZE, "GP", Layer::Bg1, FF6_MENU_BG1_TEAL);
    }
    screen_.setTextOffset(0);
    // What the item is, and how many of it the party has.
    const int item = currentItem();
    const std::vector<uint8_t> text = data.itemDescription(item);
    std::vector<uint8_t> line;
    int row = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i < text.size() && text[i] != FF6_ITEM_DESC_NEWLINE) {
            line.push_back(text[i]);
            continue;
        }
        const int y = FF6_SHOP_DESC_TOP + row++ * FF6_ITEM_ROW_HEIGHT;
        screen_.setTextOffset(y % FF6_MENU_TILE_SIZE);
        screen_.codes(FF6_ITEM_DESC_X, y / FF6_MENU_TILE_SIZE, line, Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        line.clear();
    }
    screen_.setTextOffset(0);
    screen_.text(3, 23, "Owned:" + number(owned(item), 3), Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.text(16, 23, number(hooks_.gil ? hooks_.gil() : 0, 7) + " GP", Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    if (mode_ == ShopMode::BuyList) {
        screen_.hand(FF6_SHOP_CURSOR_X, FF6_SHOP_BUY_TOP + selected_ * FF6_ITEM_ROW_HEIGHT + FF6_ITEM_CURSOR_DY);
    }
}

void ShopMenu::drawSellList() {
    drawTop(true);
    screen_.window(1, 9, 28, 16);
    const MenuData& data = screen_.data();
    for (int row = 0; row < FF6_SHOP_SELL_ROWS; ++row) {
        const size_t place = static_cast<size_t>(first_ + row);
        if (place >= inventory_.size()) break;
        const MenuItem& item = inventory_[place];
        const int y = FF6_SHOP_SELL_TOP + row * FF6_ITEM_ROW_HEIGHT;
        screen_.setTextOffset(y % FF6_MENU_TILE_SIZE);
        std::vector<uint8_t> codes = itemName(data, item.id);
        codes.push_back(menuCode(':'));
        screen_.codes(FF6_SHOP_NAME_X, y / FF6_MENU_TILE_SIZE, codes, Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        screen_.text(FF6_ITEM_QUANTITY_X, y / FF6_MENU_TILE_SIZE, number(item.quantity, 2), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        const int price = hooks_.sellPrice ? hooks_.sellPrice(item.id) : 0;
        screen_.text(FF6_SHOP_PRICE_X + 2, y / FF6_MENU_TILE_SIZE, number(price, 6), Layer::Bg1,
                     price > 0 ? FF6_MENU_BG1_DEFAULT : FF6_MENU_BG1_GRAY);
    }
    screen_.setTextOffset(0);
    if (mode_ == ShopMode::SellList) {
        screen_.hand(FF6_SHOP_CURSOR_X, FF6_SHOP_SELL_TOP + (selected_ - first_) * FF6_ITEM_ROW_HEIGHT + FF6_ITEM_CURSOR_DY);
    }
}

// The item, how many, and what it comes to.
void ShopMenu::drawQuantity() {
    drawTop(false);
    screen_.window(1, 5, 17, 7);
    screen_.window(20, 5, 9, 7);
    const bool buying = mode_ == ShopMode::BuyQuantity;
    const int item = currentItem();
    const long unit = buying ? (hooks_.buyPrice ? hooks_.buyPrice(item) : 0) : (hooks_.sellPrice ? hooks_.sellPrice(item) : 0);
    screen_.codes(3, 7, itemName(screen_.data(), item), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(3, 9, ":" + number(quantity_, 2), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(3, 11, buying ? "Owned:" + number(owned(item), 3) : "Own:" + number(maxQuantity_, 3), Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.text(21, 7, number(unit * quantity_, 7), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(22, 9, "GP", Layer::Bg1, FF6_MENU_BG1_TEAL);
}

void ShopMenu::draw() {
    switch (mode_) {
        case ShopMode::Options:
            drawTop(true);
            screen_.hand(kOptionColumns[option_] * FF6_MENU_TILE_SIZE - FF6_MENU_CURSOR_SIZE,
                         FF6_SHOP_OPTION_ROW * FF6_MENU_TILE_SIZE + FF6_ITEM_CURSOR_DY);
            break;
        case ShopMode::BuyList: drawBuyList(); break;
        case ShopMode::SellList: drawSellList(); break;
        case ShopMode::BuyQuantity:
        case ShopMode::SellQuantity: drawQuantity(); break;
    }
}

}  // namespace ff6
