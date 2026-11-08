#include "menu/character_menu.hpp"

#include <algorithm>
#include <cstdio>

#include "field/map.hpp"

namespace ff6 {
namespace {

using Layer = MenuScreen::Layer;

std::string number(long value, int width) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%*ld", width, value);
    return buffer;
}

const char* const kSlotNames[FF6_MENU_EQUIP_SLOTS] = {"R-hand", "L-hand", "Head", "Body", "Relic", "Relic"};
const char* const kSkillNames[FF6_MENU_SKILL_LISTS] = {"Espers", "Magic", "SwdTech", "Blitz", "Lore", "Rage", "Dance"};
const int kSkillRows[FF6_MENU_SKILL_LISTS] = {3, 5, 9, 11, 13, 15, 17};

}  // namespace

void CharacterMenu::open(CharacterScreen which, const MenuCharacter& character, const EquipHooks* hooks) {
    which_ = which;
    character_ = character;
    hooks_ = hooks;
    picking_ = false;
    listing_ = false;
    closed_ = false;
    cursor_ = 0;
    repeat_.reset();
}

int CharacterMenu::slot() const { return which_ == CharacterScreen::Relic ? FF6_EQUIP_FIRST_RELIC + cursor_ : cursor_; }

// The list of what fits the slot, with the choice of taking the item off first.
void CharacterMenu::startPick() {
    if (!hooks_ || !hooks_->choices) {
        sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
        return;
    }
    choices_ = {FF6_ITEM_EMPTY};
    for (const int item : hooks_->choices(character_.id, slot())) choices_.push_back(item);
    pickCursor_ = pickScroll_ = 0;
    picking_ = true;
    sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
    updatePreview();
}

void CharacterMenu::updatePreview() {
    preview_ = hooks_ && hooks_->preview ? hooks_->preview(character_.id, slot(), choices_[static_cast<size_t>(pickCursor_)])
                                         : character_;
}

void CharacterMenu::updatePick(int direction, const InputState& input) {
    const int count = static_cast<int>(choices_.size());
    const int before = pickCursor_;
    if (direction == Up && pickCursor_ > 0) --pickCursor_;
    if (direction == Down && pickCursor_ < count - 1) ++pickCursor_;
    if (pickCursor_ != before) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        if (pickCursor_ < pickScroll_) pickScroll_ = pickCursor_;
        if (pickCursor_ >= pickScroll_ + FF6_EQUIP_LIST_ROWS) pickScroll_ = pickCursor_ - FF6_EQUIP_LIST_ROWS + 1;
        updatePreview();
    }
    if (input.cancel) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        picking_ = false;
    } else if (input.confirm) {
        if (hooks_ && hooks_->equip) {
            character_ = hooks_->equip(character_.id, slot(), choices_[static_cast<size_t>(pickCursor_)]);
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        } else {
            sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
        }
        picking_ = false;
    }
}

int CharacterMenu::rows() const {
    switch (which_) {
        case CharacterScreen::Equip: return FF6_MENU_EQUIP_SLOTS;
        case CharacterScreen::Relic: return 2;
        case CharacterScreen::Skills: return FF6_MENU_SKILL_LISTS;
        default: return 1;
    }
}

void CharacterMenu::update(const InputState& input) {
    const int direction = repeat_.update(input.direction);
    if (picking_) {
        updatePick(direction, input);
        return;
    }
    if (listing_) {  // the names of one category: they can be browsed, nothing is cast from here
        const int size = static_cast<int>(character_.skills[static_cast<size_t>(cursor_)].size());
        const int before = listCursor_;
        if (direction == Up) listCursor_ = std::max(listCursor_ - 1, 0);
        if (direction == Down) listCursor_ = std::min(listCursor_ + 1, size - 1);
        if (listCursor_ != before) {
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
            if (listCursor_ < listScroll_) listScroll_ = listCursor_;
            if (listCursor_ >= listScroll_ + FF6_SKILL_LIST_ROWS) listScroll_ = listCursor_ - FF6_SKILL_LIST_ROWS + 1;
        }
        if (input.cancel) {
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
            listing_ = false;
        } else if (input.confirm) {
            const bool esper = cursor_ == 0 && hooks_ && hooks_->equipEsper && listCursor_ < static_cast<int>(character_.espers.size());
            if (esper) {
                character_ = hooks_->equipEsper(character_.id, character_.espers[static_cast<size_t>(listCursor_)]);
                sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
            } else {
                sound_.command(FF6_SFX_MENU_INVALID, 0, 0);
            }
        }
        return;
    }
    const int count = rows();
    if (count > 1) {  // the lists do not wrap
        if (direction == Up && cursor_ > 0) {
            --cursor_;
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        } else if (direction == Down && cursor_ < count - 1) {
            ++cursor_;
            sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        }
    }
    if (input.cancel) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        closed_ = true;
    } else if (input.confirm && (which_ == CharacterScreen::Equip || which_ == CharacterScreen::Relic)) {
        startPick();
    } else if (input.confirm && which_ == CharacterScreen::Skills && !character_.skills[static_cast<size_t>(cursor_)].empty()) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        listing_ = true;
        listCursor_ = listScroll_ = 0;
    } else if (input.confirm && which_ != CharacterScreen::Status) {
        sound_.command(FF6_SFX_MENU_INVALID, 0, 0);  // nothing to list or to equip here
    }
}

void CharacterMenu::drawName(int x, int row) {
    screen_.text(x, row, characterName(character_.id), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
}

// The positions of the labels of the game where it gives them, the rest laid out to fit one screen.
void CharacterMenu::drawStatus() {
    const MenuCharacter& c = character_;
    screen_.text(2, 2, "Status", Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.portrait(FF6_MENU_PORTRAIT_X, 40, menuPortrait(c.id));
    drawName(10, 5);
    screen_.text(10, 7, "LV", Layer::Bg1, FF6_MENU_BG1_TEAL);
    screen_.text(10, 8, "HP", Layer::Bg1, FF6_MENU_BG1_TEAL);
    screen_.text(10, 9, "MP", Layer::Bg1, FF6_MENU_BG1_TEAL);
    screen_.text(15, 7, number(c.level, 2), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(13, 8, number(c.hp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(17, 8, "/", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(18, 8, number(c.maxHp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(13, 9, number(c.mp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(17, 9, "/", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(18, 9, number(c.maxMp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    struct Stat { int x, row; const char* label; int value; const char* unit; };
    const Stat stats[] = {
        {3, 12, "Vigor", c.vigor, ""}, {3, 14, "Speed", c.speed, ""}, {3, 16, "Stamina", c.stamina, ""},
        {3, 18, "Mag.Pwr", c.magicPower, ""}, {16, 12, "Bat.Pwr", c.attackPower, ""},
        {16, 14, "Defense", c.defense, ""}, {16, 16, "Evade %", c.evade, ""}, {16, 18, "Mag.Def", c.magicDefense, ""},
        {16, 20, "MBlock%", c.magicEvade, ""}};
    for (const Stat& stat : stats) {
        screen_.text(stat.x, stat.row, stat.label, Layer::Bg3, FF6_MENU_BG3_TEAL);
        screen_.codes(stat.x + 7, stat.row, {FF6_MENU_CODE_2DOT}, Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        screen_.text(stat.x + 8, stat.row, number(stat.value, 3), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    }
    screen_.text(2, 22, "Your Exp:", Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.text(14, 22, number(c.experience, 8), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    screen_.text(2, 24, "For level up:", Layer::Bg3, FF6_MENU_BG3_TEAL);
    screen_.text(14, 24, number(c.nextLevel, 8), Layer::Bg3, FF6_MENU_BG3_DEFAULT);
}

void CharacterMenu::drawEquip() {
    const bool relic = which_ == CharacterScreen::Relic;
    const MenuCharacter& shown = picking_ ? preview_ : character_;  // with the item that the hand is on
    const MenuData& data = screen_.data();
    const auto itemName = [&](int item) {
        const uint8_t* name = data.itemNames.data() + static_cast<size_t>(item) * FF6_ITEM_NAME_BYTES;
        return std::vector<uint8_t>(name, name + FF6_ITEM_NAME_BYTES);
    };
    if (picking_) screen_.window(1, FF6_EQUIP_STAT_ROW - 1, FF6_EQUIP_LIST_WIDTH, FF6_EQUIP_LIST_ROWS);
    screen_.text(2, 2, relic ? "RELIC" : "EQUIP", Layer::Bg3, FF6_MENU_BG3_TEAL);
    drawName(10, 4);
    for (int slot = 0; slot < FF6_MENU_EQUIP_SLOTS; ++slot) {
        const int row = FF6_EQUIP_SLOT_ROW + slot;
        screen_.text(3, row, kSlotNames[slot], Layer::Bg3, relic && slot < FF6_EQUIP_FIRST_RELIC ? FF6_MENU_BG3_GRAY : FF6_MENU_BG3_TEAL);
        const int item = shown.equipment[slot];
        if (item == FF6_ITEM_EMPTY) {
            screen_.text(10, row, "Empty", Layer::Bg1, FF6_MENU_BG1_GRAY);
        } else {
            screen_.codes(10, row, itemName(item), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        }
    }
    // The statistics; while an item is picked, the ones that it changes are yellow when higher and gray when lower.
    struct Stat { const char* label; int MenuCharacter::*value; };
    static const Stat kStats[] = {{"Vigor", &MenuCharacter::vigor}, {"Speed", &MenuCharacter::speed},
                                  {"Stamina", &MenuCharacter::stamina}, {"Mag.Pwr", &MenuCharacter::magicPower},
                                  {"Bat.Pwr", &MenuCharacter::attackPower}, {"Defense", &MenuCharacter::defense},
                                  {"Evade %", &MenuCharacter::evade}, {"Mag.Def", &MenuCharacter::magicDefense},
                                  {"MBlock%", &MenuCharacter::magicEvade}};
    int row = FF6_EQUIP_STAT_ROW;
    for (const Stat& stat : kStats) {
        const int now = character_.*stat.value, then = shown.*stat.value;
        screen_.text(FF6_EQUIP_STAT_LABEL_X, row, stat.label, Layer::Bg3, FF6_MENU_BG3_TEAL);
        screen_.text(FF6_EQUIP_STAT_VALUE_X, row++, number(then, 3), Layer::Bg3,
                     then > now ? FF6_MENU_BG3_YELLOW : then < now ? FF6_MENU_BG3_GRAY : FF6_MENU_BG3_DEFAULT);
    }
    if (!picking_) {
        screen_.hand(FF6_MENU_EQUIP_CURSOR_X, (FF6_EQUIP_SLOT_ROW + slot()) * FF6_MENU_TILE_SIZE + FF6_ITEM_CURSOR_DY);
        return;
    }
    for (int i = 0; i < FF6_EQUIP_LIST_ROWS && pickScroll_ + i < static_cast<int>(choices_.size()); ++i) {
        const int item = choices_[static_cast<size_t>(pickScroll_ + i)];
        if (item == FF6_ITEM_EMPTY) {
            screen_.text(FF6_EQUIP_LIST_X, FF6_EQUIP_STAT_ROW + i, "Remove", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        } else {
            screen_.codes(FF6_EQUIP_LIST_X, FF6_EQUIP_STAT_ROW + i, itemName(item), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
        }
    }
    screen_.hand(FF6_MENU_EQUIP_CURSOR_X, (FF6_EQUIP_STAT_ROW + pickCursor_ - pickScroll_) * FF6_MENU_TILE_SIZE + FF6_ITEM_CURSOR_DY);
}

void CharacterMenu::drawSkills() {
    const MenuCharacter& c = character_;
    for (int list = 0; list < FF6_MENU_SKILL_LISTS; ++list) {
        screen_.text(3, kSkillRows[list], kSkillNames[list], Layer::Bg3,
                     !c.skills[static_cast<size_t>(list)].empty() ? FF6_MENU_BG3_DEFAULT : FF6_MENU_BG3_GRAY);
    }
    drawName(18, 5);
    screen_.text(18, 7, "LV", Layer::Bg1, FF6_MENU_BG1_TEAL);
    screen_.text(18, 9, "HP", Layer::Bg1, FF6_MENU_BG1_TEAL);
    screen_.text(18, 11, "MP", Layer::Bg1, FF6_MENU_BG1_TEAL);
    screen_.text(23, 7, number(c.level, 2), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(21, 9, number(c.hp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(25, 9, "/", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(26, 9, number(c.maxHp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(21, 11, number(c.mp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(25, 11, "/", Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    screen_.text(26, 11, number(c.maxMp, 4), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    if (listing_) {  // the names of the category over the right of the screen
        const SkillList& names = c.skills[static_cast<size_t>(cursor_)];
        screen_.window(FF6_SKILL_LIST_WINDOW_X, 3, FF6_SKILL_LIST_WINDOW_WIDTH, FF6_SKILL_LIST_ROWS + 2);
        for (int i = 0; i < FF6_SKILL_LIST_ROWS && listScroll_ + i < static_cast<int>(names.size()); ++i) {
            const bool carried = cursor_ == 0 && listScroll_ + i < static_cast<int>(c.espers.size()) &&
                                 c.espers[static_cast<size_t>(listScroll_ + i)] == c.equippedEsper;
            screen_.codes(FF6_SKILL_LIST_X, 4 + i, names[static_cast<size_t>(listScroll_ + i)], Layer::Bg1,
                          carried ? FF6_MENU_BG1_TEAL : FF6_MENU_BG1_DEFAULT);
        }
        screen_.hand(FF6_SKILL_LIST_X * FF6_MENU_TILE_SIZE - FF6_MENU_CURSOR_SIZE,
                     (4 + listCursor_ - listScroll_) * FF6_MENU_TILE_SIZE + FF6_ITEM_CURSOR_DY);
        return;
    }
    screen_.hand(FF6_MENU_SKILL_CURSOR_X, kSkillRows[cursor_] * FF6_MENU_TILE_SIZE + FF6_ITEM_CURSOR_DY);
}

void CharacterMenu::draw() {
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    screen_.window(1, 1, 28, 24);
    switch (which_) {
        case CharacterScreen::Status: drawStatus(); break;
        case CharacterScreen::Skills: drawSkills(); break;
        default: drawEquip(); break;
    }
}

}  // namespace ff6
