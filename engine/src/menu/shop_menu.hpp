#pragma once

#include <functional>
#include <string>
#include <vector>

#include "input.hpp"
#include "menu/item_menu.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// What the shop asks of the game.
struct ShopHooks {
    std::function<int(int item)> buyPrice;
    std::function<int(int item)> sellPrice;  // 0 for an item that the shop does not buy
    std::function<bool(int item, int quantity)> buy;  // pays and adds the items
    std::function<bool(int item, int quantity)> sell;  // takes the items and pays
    std::function<std::vector<MenuItem>()> items;  // the inventory
    std::function<long()> gil;
};

enum class ShopMode { Options, BuyList, BuyQuantity, SellList, SellQuantity };

// The shop menu (src/menu/shop.asm): BUY, SELL and EXIT, the list of what the shop sells with its prices, and the
// quantity of each purchase or sale. The positions of the texts are close to the game's, not exact.
class ShopMenu {
public:
    ShopMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    // `kind` is the kind of shop (weapons, armor, items, relics, vendor); `stock` the items that it sells.
    void open(int kind, const std::vector<int>& stock, const ShopHooks& hooks);
    void update(const InputState& input);
    void draw();
    bool closed() const { return closed_; }
    ShopMode mode() const { return mode_; }
    int option() const { return option_; }
    int selected() const { return selected_; }
    int quantity() const { return quantity_; }
    const std::string& message() const { return message_; }

private:
    void updateQuantity(int direction);
    void enterBuyQuantity();
    void enterSellQuantity();
    int owned(int item) const;
    int currentItem() const;
    void drawTop(bool showGil);
    void drawBuyList();
    void drawSellList();
    void drawQuantity();

    MenuScreen& screen_;
    Sound& sound_;
    ShopHooks hooks_;
    int kind_ = 0;
    std::vector<int> stock_;
    std::vector<MenuItem> inventory_;  // what the sell list shows
    ShopMode mode_ = ShopMode::Options;
    std::string message_;
    int option_ = 0, selected_ = 0, first_ = 0, quantity_ = 1, maxQuantity_ = 1;
    bool closed_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
