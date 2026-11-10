#include "menu/party_menu.hpp"

#include <algorithm>
#include <cstdio>

#include "field/map.hpp"

namespace ff6 {
namespace {

using Layer = MenuScreen::Layer;

constexpr int kBlockColumns = 8;
constexpr int kStep = 28;  // between the places of the block and between the two rows of a party
constexpr int kBlockIconX = 0x18, kBlockIconY = 0x5C;  // the sprite of the first place of the block
constexpr int kPartyIconX = 0x18, kPartyIconY = 0x9C;  // and of the first place of the first party
constexpr int kPartyColumnStep = 32, kPartyStep = 80;  // between the two columns of a party, and between parties
constexpr int kHandX = 8, kBlockHandY = 100, kPartyHandY = 164;  // the hand, from the position that the game gives its sprite
constexpr int kMessageFrames = 32;
constexpr int kTitleY = 22, kLevelY = 56, kHpY = 68, kMpY = 80;  // the lines of text, in pixels, after the scroll of the game's tables
constexpr int kPortraitX = 26, kPortraitY = 48;
constexpr int kIconX = 72, kIconY = 48, kIconStep = 10;  // the status icons start here, and each one is this far from the last
constexpr int kArrowDy = 20;  // the arrow of a character that has to stay, under its sprite

// The column of a party that the hand goes to from a column of the block, and back (_c37466 and _c37476).
int partyColumn(int parties, int column) {
    static constexpr int kThree[8] = {0, 1, 1, 2, 3, 4, 4, 5};
    if (parties >= 3) return kThree[column];
    return std::min(column, parties * 2 - 1);
}

int blockColumn(int column) {
    static constexpr int kBack[6] = {0, 1, 3, 4, 6, 7};
    return kBack[column];
}

std::string number(int value, int width) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%*d", width, value);
    return buffer;
}

}  // namespace

int PartyMenu::slotIndex(int area, int col, int row) {
    return area == 0 ? row * kBlockColumns + col : kBlockSlots + (col / 2) * kPartyPlaces + (col % 2) * 2 + row;
}

void PartyMenu::open(const PartyMenuSetup& setup) {
    characters_ = setup.characters;
    forced_ = setup.forced;
    parties_ = std::clamp(setup.parties, 1, kMaxParties);
    slots_.fill(-1);
    int block = 0;
    for (int i = 0; i < FF6_CHAR_COUNT; ++i) {
        kept_[static_cast<size_t>(i)] = setup.clear ? 0 : characters_[static_cast<size_t>(i)].party;
        if (characters_[static_cast<size_t>(i)].enabled && kept_[static_cast<size_t>(i)] == 0) slots_[static_cast<size_t>(block++)] = i;
    }
    for (int party = 1; party <= parties_; ++party) {
        int place = 0;
        for (int i = 0; i < FF6_CHAR_COUNT && place < kPartyPlaces; ++i) {
            if (characters_[static_cast<size_t>(i)].enabled && kept_[static_cast<size_t>(i)] == party) {
                slots_[static_cast<size_t>(kBlockSlots + (party - 1) * kPartyPlaces + place++)] = i;
            }
        }
    }
    area_ = col_ = row_ = 0;
    picking_ = false;
    error_ = errorFrames_ = 0;
    closed_ = false;
    repeat_.reset();
}

std::array<int, FF6_CHAR_COUNT> PartyMenu::assignment() const {
    std::array<int, FF6_CHAR_COUNT> result = kept_;
    for (size_t at = 0; at < slots_.size(); ++at) {
        if (slots_[at] < 0) continue;
        result[static_cast<size_t>(slots_[at])] = at < static_cast<size_t>(kBlockSlots) ? 0 : static_cast<int>(at - kBlockSlots) / kPartyPlaces + 1;
    }
    return result;
}

std::array<int, FF6_CHAR_COUNT> PartyMenu::places() const {
    std::array<int, FF6_CHAR_COUNT> result{};
    for (size_t at = kBlockSlots; at < slots_.size(); ++at) {
        if (slots_[at] >= 0) result[static_cast<size_t>(slots_[at])] = static_cast<int>(at - kBlockSlots) % kPartyPlaces;
    }
    return result;
}

int PartyMenu::count(int party) const {
    int members = 0;
    for (int place = 0; place < kPartyPlaces; ++place) members += inParty(party - 1, place) >= 0 ? 1 : 0;
    return members;
}

// The hand goes along the block and the parties; down from the second row of the block it gets to the parties, and up from
// their first row back.
void PartyMenu::move(int direction) {
    const int before = cursor();
    const int columns = area_ == 0 ? kBlockColumns : parties_ * 2;
    if (direction == Left && col_ > 0) --col_;
    if (direction == Right && col_ < columns - 1) ++col_;
    if (direction == Down) {
        if (row_ == 0) {
            row_ = 1;
        } else if (area_ == 0) {
            area_ = 1;
            col_ = partyColumn(parties_, col_);
            row_ = 0;
        }
    }
    if (direction == Up) {
        if (row_ == 1) {
            row_ = 0;
        } else if (area_ == 1) {
            area_ = 0;
            col_ = blockColumn(col_);
            row_ = 1;
        }
    }
    if (cursor() != before) sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
}

void PartyMenu::finish() {
    for (int party = 1; party <= parties_; ++party) {
        if (count(party) != 0) continue;
        sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
        error_ = parties_ == 1 ? 2 : 1;
        errorFrames_ = kMessageFrames;
        return;
    }
    sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    closed_ = true;
}

void PartyMenu::update(const InputState& input) {
    if (viewing_) {
        status_.update(input);
        if (status_.closed()) viewing_ = false;
        return;
    }
    if (errorFrames_ > 0) {  // the message stays, and the menu waits
        if (--errorFrames_ == 0) error_ = 0;
        return;
    }
    const int direction = repeat_.update(input.direction);
    if (direction != FF6_NO_DIRECTION) move(direction);
    const int at = cursor();
    const int who = slots_[static_cast<size_t>(at)];
    const bool fixed = who >= 0 && ((forced_ >> who) & 1) != 0;
    if (input.confirm) {
        if (fixed) {
            sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
        } else if (!picking_) {
            picking_ = true;
            picked_ = at;
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        } else if (at == picked_) {  // the same place twice: the status of the character, if there is one
            if (who < 0) {
                sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
            } else {
                picking_ = false;
                viewing_ = true;
                status_.open(CharacterScreen::Status, characters_[static_cast<size_t>(who)].info);
                sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
            }
        } else {
            std::swap(slots_[static_cast<size_t>(at)], slots_[static_cast<size_t>(picked_)]);
            picking_ = false;
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        }
    } else if (input.cancel || input.start) {
        if (picking_) {
            picking_ = false;
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        } else {
            finish();
        }
    }
}

void PartyMenu::drawText(int x, int y, const std::string& text, int color) {
    screen_.setTextOffset(y % FF6_MENU_TILE_SIZE);
    screen_.text(x, y / FF6_MENU_TILE_SIZE, text, Layer::Bg1, color);
    screen_.setTextOffset(0);
}

void PartyMenu::drawHand(int index) {
    if (index < kBlockSlots) {
        screen_.hand(kHandX + (index % kBlockColumns) * kStep, kBlockHandY + (index / kBlockColumns) * kStep);
        return;
    }
    const int place = index - kBlockSlots;
    const int party = place / kPartyPlaces, slot = place % kPartyPlaces;
    screen_.hand(kHandX + (slot / 2) * kPartyColumnStep + party * kPartyStep, kPartyHandY + (slot % 2) * kStep);
}

void PartyMenu::drawIcon(int index) {
    const int who = slots_[static_cast<size_t>(index)];
    if (who < 0) return;
    const PartyMenuCharacter& c = characters_[static_cast<size_t>(who)];
    int x, y;
    if (index < kBlockSlots) {
        x = kBlockIconX + (index % kBlockColumns) * kStep;
        y = kBlockIconY + (index / kBlockColumns) * kStep;
    } else {
        const int place = index - kBlockSlots;
        const int party = place / kPartyPlaces, slot = place % kPartyPlaces;
        x = kPartyIconX + (slot / 2) * kPartyColumnStep + party * kPartyStep;
        y = kPartyIconY + (slot % 2) * kStep;
    }
    screen_.mapSprite(x, y + 1, c.gfx, c.palette);
    if (((forced_ >> who) & 1) != 0) screen_.menuSprite(x, y + kArrowDy, FF6_MENU_ARROW_TILE, false);
}

// The icons of the statuses, or "Wounded" for the dead; the text that follows is gray for them.
void PartyMenu::drawStatus(const PartyMenuCharacter& c) {
    if (c.status & FF6_ST_DEAD) {
        drawText(9, kLevelY, "Wounded ", FF6_MENU_BG1_GRAY);
        return;
    }
    static constexpr uint32_t kStatuses[FF6_MENU_STATUS_ICON_COUNT] = {FF6_ST_FLOAT, FF6_ST_PETRIFY, FF6_ST_IMP, FF6_ST_VANISH,
                                                                       FF6_ST_POISON, FF6_ST_ZOMBIE, FF6_ST_BLIND};
    int x = kIconX;
    for (int icon = 0; icon < FF6_MENU_STATUS_ICON_COUNT; ++icon) {
        if ((c.status & kStatuses[icon]) == 0) continue;
        screen_.menuSprite(x, kIconY, FF6_MENU_STATUS_ICON_FIRST_TILE + icon * 2, true);
        x += kIconStep;
    }
}

void PartyMenu::draw() {
    if (viewing_) {
        status_.draw();
        return;
    }
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(1, 5, 28, 5);
    screen_.window(1, 18, 28, 7);
    screen_.window(1, 11, 28, 6);
    screen_.window(9, 1, 20, 2);
    screen_.window(1, 1, 6, 2);
    for (int party = 0; party < parties_; ++party) screen_.window(1 + party * 10, 19, 8, 6);
    drawText(2, kTitleY, "Lineup", FF6_MENU_BG1_TEAL);
    if (error_ == 2) {
        drawText(10, kTitleY, "No one there!       ", FF6_MENU_BG1_DEFAULT);
    } else if (error_ == 1) {
        drawText(10, kTitleY, "You need   group(s)!", FF6_MENU_BG1_DEFAULT);
        drawText(19, kTitleY, std::to_string(parties_), FF6_MENU_BG1_DEFAULT);
    } else {
        drawText(10, kTitleY, "Form   group(s).      ", FF6_MENU_BG1_DEFAULT);
        drawText(15, kTitleY, std::to_string(parties_), FF6_MENU_BG1_DEFAULT);
    }
    for (size_t at = 0; at < slots_.size(); ++at) {
        if (at < static_cast<size_t>(kBlockSlots) || static_cast<int>(at - kBlockSlots) / kPartyPlaces < parties_) drawIcon(static_cast<int>(at));
    }
    // What is known of the character under the hand.
    const int who = slots_[static_cast<size_t>(cursor())];
    if (who >= 0) {
        const PartyMenuCharacter& c = characters_[static_cast<size_t>(who)];
        const int color = (c.status & FF6_ST_DEAD) ? FF6_MENU_BG1_GRAY : FF6_MENU_BG1_DEFAULT;
        screen_.portrait(kPortraitX, kPortraitY, menuPortrait(who));
        drawStatus(c);
        drawText(18, kLevelY, "LV", FF6_MENU_BG1_TEAL);
        drawText(18, kHpY, "HP", FF6_MENU_BG1_TEAL);
        drawText(18, kMpY, "MP", FF6_MENU_BG1_TEAL);
        drawText(23, kLevelY, number(c.info.level, 2), color);
        drawText(21, kHpY, number(c.info.hp, 4), color);
        drawText(25, kHpY, "/", color);
        drawText(26, kHpY, number(c.info.maxHp, 4), color);
        drawText(21, kMpY, number(c.info.mp, 4), color);
        drawText(25, kMpY, "/", color);
        drawText(26, kMpY, number(c.info.maxMp, 4), color);
        drawText(9, kHpY, characterName(who), color);
        if (!c.esper.empty()) {
            screen_.setTextOffset(kMpY % FF6_MENU_TILE_SIZE);
            screen_.codes(9, kMpY / FF6_MENU_TILE_SIZE, c.esper, Layer::Bg1, color);
            screen_.setTextOffset(0);
        }
    }
    if (picking_) drawHand(picked_);
    drawHand(cursor());
}

}  // namespace ff6
