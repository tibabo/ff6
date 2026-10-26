#include "menu/character_menu.hpp"

#include <cstdio>

#include "field/map.hpp"

namespace ff6 {
namespace {

using Layer = MenuScreen::Layer;

std::string characterName(int id) {
    static const char* const kNames[] = FF6_CHARACTER_NAMES;
    std::string name = id >= 0 && id < FF6_CHAR_COUNT ? kNames[id] : "";
    if (!name.empty()) name[0] = static_cast<char>(name[0] - 'a' + 'A');
    return name;
}

std::string number(long value, int width) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%*ld", width, value);
    return buffer;
}

const char* const kSlotNames[FF6_MENU_EQUIP_SLOTS] = {"R-hand", "L-hand", "Head", "Body", "Relic", "Relic"};
const char* const kSkillNames[FF6_MENU_SKILL_LISTS] = {"Espers", "Magic", "SwdTech", "Blitz", "Lore", "Rage", "Dance"};
const int kSkillRows[FF6_MENU_SKILL_LISTS] = {3, 5, 9, 11, 13, 15, 17};

// Who has which list of skills: the espers are not in the placeholder inventory, so nobody has them.
bool hasSkills(int list, int character) {
    switch (list) {
        case 1: return character == 0 || character == 6 || character == 7 || character == 8;  // magic
        case 2: return character == 2;  // sword techniques
        case 3: return character == 5;  // blitzes
        case 4: return character == 7;  // lores
        case 5: return character == 11;  // rages
        case 6: return character == 10;  // dances
        default: return false;
    }
}

}  // namespace

void CharacterMenu::open(CharacterScreen which, const MenuCharacter& character) {
    which_ = which;
    character_ = character;
    closed_ = false;
    cursor_ = 0;
    repeat_.reset();
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
    } else if (input.confirm && which_ != CharacterScreen::Status) {
        sound_.command(FF6_SFX_MENU_INVALID, 0, 0);  // the equipment and the skills cannot be changed or listed yet
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
    screen_.text(2, 2, relic ? "RELIC" : "EQUIP", Layer::Bg3, FF6_MENU_BG3_TEAL);
    drawName(10, 4);
    const MenuData& data = screen_.data();
    for (int slot = 0; slot < FF6_MENU_EQUIP_SLOTS; ++slot) {
        const int row = 7 + 2 * slot;
        screen_.text(3, row, kSlotNames[slot], Layer::Bg3, relic && slot < 4 ? FF6_MENU_BG3_GRAY : FF6_MENU_BG3_TEAL);
        const int item = character_.equipment[slot];
        if (item == FF6_ITEM_EMPTY) {
            screen_.text(10, row, "Empty", Layer::Bg1, FF6_MENU_BG1_GRAY);
            continue;
        }
        const uint8_t* name = &data.itemNames[static_cast<size_t>(item) * FF6_ITEM_NAME_BYTES];
        screen_.codes(10, row, std::vector<uint8_t>(name, name + FF6_ITEM_NAME_BYTES), Layer::Bg1, FF6_MENU_BG1_DEFAULT);
    }
    const int slot = relic ? FF6_MENU_EQUIP_SLOTS - 2 + cursor_ : cursor_;
    screen_.hand(FF6_MENU_EQUIP_CURSOR_X, 7 * FF6_MENU_TILE_SIZE + 2 * slot * FF6_MENU_TILE_SIZE + FF6_ITEM_CURSOR_DY);
}

void CharacterMenu::drawSkills() {
    const MenuCharacter& c = character_;
    for (int list = 0; list < FF6_MENU_SKILL_LISTS; ++list) {
        screen_.text(3, kSkillRows[list], kSkillNames[list], Layer::Bg3,
                     hasSkills(list, c.id) ? FF6_MENU_BG3_DEFAULT : FF6_MENU_BG3_GRAY);
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
