#include "battle/battle_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "battle/battle_events.hpp"
#include "field/map.hpp"

namespace ff6 {
namespace {

using Layer = MenuScreen::Layer;

// The animation of a usable item (potions, antidotes, Fenix Down...), -1 for one that has none.
int itemAnimation(int item) {
    if (item == FF6_ITEM_ANIM_RENAME_CARD) return FF6_ITEM_ANIM_RENAME_CARD_ANIM;
    if (item >= FF6_ITEM_ANIM_FIRST && item <= FF6_ITEM_ANIM_LAST) return FF6_ITEM_ANIM_FIRST_ANIM + item - FF6_ITEM_ANIM_FIRST;
    return -1;
}
std::string number(long value, int width) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%*ld", width, value);
    return buffer;
}

std::vector<uint8_t> text(const std::string& string) {
    std::vector<uint8_t> codes;
    for (const char character : string) codes.push_back(menuCode(character));
    return codes;
}

// What a monster says, from the text of the game: letters from 0x80, 0xFF for a space, a new line (1) or a wait (5) ends what the
// window shows, and 2 and its character put the name of a character.
std::vector<std::string> dialogLines(const std::vector<uint8_t>& raw) {
    static const char kPunctuation[] = "!?/:\"'-.,.;#+()%~";  // 0xBE to 0xCE (0xC7 is an ellipsis)
    std::vector<std::string> lines;
    std::string line;
    const auto flush = [&] {
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (!line.empty()) lines.push_back(line);
        line.clear();
    };
    for (size_t i = 0; i < raw.size(); ++i) {
        const int code = raw[i];
        if (code >= 0x80 && code <= 0x99) line += static_cast<char>('A' + code - 0x80);
        else if (code >= 0x9A && code <= 0xB3) line += static_cast<char>('a' + code - 0x9A);
        else if (code >= 0xB4 && code <= 0xBD) line += static_cast<char>('0' + code - 0xB4);
        else if (code >= 0xBE && code <= 0xCE) line += kPunctuation[code - 0xBE];
        else if (code == 0xD2) line += '=';
        else if (code == 0xD3) line += '"';
        else if (code == 0xFF) line += ' ';
        else if (code == 0x01 || code == 0x05 || code == 0x07) flush();
        else if (code == 0x02 && i + 1 < raw.size()) line += characterName(raw[++i]);
    }
    flush();
    return lines;
}

// The colors of a picture with what a palette command adds to them or takes from them (a step of the game is 8 here).
std::vector<uint8_t> colored(const std::vector<uint8_t>& rgba, const AnimTint& tint) {
    std::vector<uint8_t> result = rgba;
    const int amounts[3] = {tint.r * 8, tint.g * 8, tint.b * 8};
    for (size_t i = 0; i + FF6_BYTES_PER_PIXEL <= result.size(); i += FF6_BYTES_PER_PIXEL) {
        if (result[i + FF6_ALPHA_OFFSET] == 0) continue;
        for (int channel = 0; channel < 3; ++channel) {
            const int value = result[i + static_cast<size_t>(channel)] + (tint.subtract ? -amounts[channel] : amounts[channel]);
            result[i + static_cast<size_t>(channel)] = static_cast<uint8_t>(std::clamp(value, 0, 255));
        }
    }
    return result;
}

constexpr const char* kStatusNames[FF6_ST_COUNT] = {
    "Blind", "Zombie", "Poison", "MagiTek", "Invisible", "Imp", "Petrify", "Wound", "Doom", "Near fatal", "Image",
    "Mute", "Berserk", "Muddled", "Seizure", "Sleep", "Dance", "Regen", "Slow", "Haste", "Stop", "Shell", "Safe",
    "Reflect", "Rage", "Frozen", "Reraise", "Morph", "Control", "Hide", "Intercept", "Float"};
constexpr const char* kCommandNames[FF6_CMD_COUNT] = {
    "Fight", "Item", "Magic", "Morph", "Revert", "Steal", "Capture", "SwdTech", "Throw", "Tools", "Blitz", "Runic", "Lore",
    "Sketch", "Control", "Slot", "Rage", "Leap", "Mimic", "Dance", "Row", "Defend", "Jump", "X Magic", "GP Rain",
    "Summon", "Health", "Shock", "Possess", "MagiTek", "", ""};

}  // namespace

BattleScreen::Codes BattleScreen::statusName(uint32_t status) const {
    Codes names;
    for (int bit = 0; bit < FF6_ST_COUNT; ++bit) {
        if (!(status & (1u << bit))) continue;
        if (!names.empty()) names = join({names, text(", ")});
        names = join({names, text(kStatusNames[bit])});
    }
    return names;
}

void BattleScreen::start(const BattleData& data, const GameRules& rules, PartyState& party,
    const std::vector<int>& characters, int battle, uint32_t seed, int battleSpeed, bool wait, int background, bool veldt,
    const Battle::Colosseum& colosseum) {
    data_ = &data;
    rules_ = &rules;
    party_ = &party;
    formation_ = data.formation(battle);
    battle_.emplace(data, rules, party, characters, battle, seed, battleSpeed, colosseum, gameRandom_);
    battle_->setQueuedTurns(true);
    introFrame_ = 0;
    waiting_.clear();
    actions_.clear();
    acting_ = -1;
    actionEnd_ = 0;
    commanded_.fill(false);
    wasReady_.fill(false);
    readyTimer_.fill(0);
    menuQueue_.clear();
    menuDelay_ = 0;
    xMagicFirst_ = nullptr;
    scrollX_ = scrollY_ = animation_ = 0;
    for (AnimShift& kept : keptShift_) kept = AnimShift();
    persistSlot_ = -1;
    sceneActive_ = false;
    bgShake_ = 0;
    eventPalette_ = -1;
    for (int slot = 0; slot < FF6_BATTLE_PARTY_SLOTS; ++slot) {
        keptPose_[slot] = gfxOverride_[slot] = -1;
        keptFlip_[slot] = keptHidden_[slot] = sceneControlled_[slot] = false;
    }
    if (battle_->sceneBackground() >= 0) background = battle_->sceneBackground();  // a scene has its own field
    background_ = background;
    battle_->setVeldt(veldt);
    battle_->setBackground(background);
    rageList_ = danceList_ = false;
    mode_ = BattleMode::Fighting;
    commands_.clear();
    list_.clear();
    group_ = false;
    castLead_ = 0;
    nameTimer_ = 0;
    stepBack_ = afterAnim_ = 0;
    stepFrom_.fill({0, 0});
    poseTimer_.fill(0);
    stepTimer_.fill(0);
    blinkTimer_.fill(0);
    monsterFlash_.fill(0);
    blowLead_ = 0;
    fadeTimer_.fill(0);
    escapeTimer_.fill(0);
    dying_.fill(false);
    fx_.clear();
    numerals_.clear();
    lastElement_ = 0;
    flashTimer_ = 0;
    messages_.clear();
    current_.clear();
    victory_.clear();
    currentKeyWait_ = currentFixed_ = false;
    messageTimer_ = 0;
    popupTarget_ = popupDamage_ = -1;
    pendingPopup_ = PendingPopup();
    heldHp_.fill(-1);
    queuedHp_.fill(-1);
    for (size_t i = 0; i < prevHp_.size(); ++i) prevHp_[i] = battle_->combatants()[i].hp;
    frames_ = 0;
    active_ = -1;
    command_ = 0;
    running_ = false;
    finishing_ = false;
    wait_ = wait;
    repeat_.reset();
    if (battle_->preemptive()) {
        queue(text("Preemptive attack!"));
    } else if (battle_->battleType() == FF6_BATTLE_TYPE_BACK) {
        queue(text("Back attack!"));
    } else if (battle_->battleType() == FF6_BATTLE_TYPE_PINCER) {
        queue(text("Pincer attack!"));
    } else if (battle_->battleType() == FF6_BATTLE_TYPE_SIDE) {
        queue(text("Side attack!"));
    }
}

void BattleScreen::close() {
    battle_.reset();
    mode_ = BattleMode::Done;
    messages_.clear();
    current_.clear();
}

BattleScreen::Codes BattleScreen::nameOf(int combatant) const {
    const Combatant& c = battle_->combatants()[static_cast<size_t>(combatant)];
    if (!c.monster) return text(characterName(c.id));
    if (c.charIndex >= 0) return text(characterName(c.charIndex));  // a character of a scene that fights for the monsters
    Codes name = data_->name(c.id);
    const auto end = std::find(name.begin(), name.end(), FF6_BATTLE_NAME_PAD_CODE);
    name.erase(end, name.end());
    return name;
}

BattleScreen::Codes BattleScreen::join(std::initializer_list<Codes> parts) const {
    Codes joined;
    for (const Codes& part : parts) joined.insert(joined.end(), part.begin(), part.end());
    return joined;
}

void BattleScreen::queue(const Codes& message, int target, int damage) { messages_.push_back({message, target, damage}); }

void BattleScreen::queueWith(const Codes& message, int target, int damage, int effect, int actor, int subject, int element, bool flash) {
    Message entry{message, target, damage};
    entry.effect = static_cast<Message::Effect>(effect);
    entry.actor = actor;
    entry.subject = subject;
    entry.element = element;
    entry.flash = flash;
    messages_.push_back(entry);
}

// The picture follows the message: a character steps and swings, casts, or flinches, a monster blinks or fades, and
// the field flashes with the color of the element.
// The middle of a combatant on the screen.
bool BattleScreen::center(int combatant, int& x, int& y) const {
    if (combatant < 0 || combatant >= FF6_BATTLE_COMBATANTS) return false;
    if (combatant < FF6_BATTLE_PARTY_SLOTS) {
        x = characterX(combatant) + FF6_CHAR_WIDTH / 2;
        y = characterTop(combatant) + FF6_CHAR_HEIGHT / 2;
        return true;
    }
    const size_t slot = static_cast<size_t>(combatant - FF6_BATTLE_PARTY_SLOTS);
    if (!gfx_) return false;
    const BattleImage& image = monsterImage(slot);
    x = monsterX(slot, image.width) + image.width / 2;
    y = monsterTop(slot) + image.height / 2;
    return true;
}

// The animation of an attack, as its scripts play it over the combatants it reaches.
void BattleScreen::startAnimation(const Message& message) {
    animated_ = false;
    if (message.attack >= FF6_FIRST_GENJU_ATTACK && message.attack < FF6_FIRST_NAMED_ATTACK && gfx_) {
        esperGfx_ = FF6_ESPER_GFX_FIRST + message.attack - FF6_FIRST_GENJU_ATTACK;
        esperAge_ = esperShown_ = 0;
        esperLeaving_ = false;
        esperStands_ = true;
        animated_ = true;
    }
    if (!player_ || message.attack < 0 || message.animTargets.empty()) return;
    AnimBody attacker;
    if (!body(message.actor, attacker)) return;
    std::vector<AnimBody> targets;
    for (const int target : message.animTargets) {
        AnimBody reached;
        if (body(target, reached)) targets.push_back(reached);
    }
    if (targets.empty()) return;
    const int type = battle_->battleType();
    const bool flipped = attackFlips(attacker, targets.front());
    player_->setMagitek(magitekMode());
    player_->start(message.attack, attacker, targets, flipped, type);
    player_->presetHidden(keptHidden_);
    plane_ = BattleImage();
    planeCycle_ = -1;
    esperAfterBolts_ = esperGfx_ >= 0 && player_->layerActive(2);
    esperPlaced_ = false;
    if (esperGfx_ >= 0 && player_->plane().kind == AnimPlane::kEsper && !player_->plane().crusader) {
        // The esper stands where mode 7 first shows its picture, so that the one takes the place of the other.
        AttackAnimPlayer probe = *player_;
        for (int frame = 0; frame < FF6_ATTACK_PROBE_FRAMES && !probe.mode7().on && probe.tick(); ++frame) {}
        if (probe.mode7().on) {
            esperX_ = -probe.mode7().scrollX;
            esperY_ = player_->plane().row * FF6_MENU_TILE_SIZE - probe.mode7().scrollY;
            esperPlaced_ = true;
        }
    }
    // The picture of most espers is a sheet of parts that their animation puts together; only a whole one stands before it.
    if (esperGfx_ >= 0) esperStands_ = esperPlaced_ || (player_->esperSheet().layer < 0 && player_->plane().kind == AnimPlane::kNone);
    animated_ = animated_ || !player_->done();
}

void BattleScreen::playAnimation(int attack) {
    Message message;
    message.attack = attack;
    const bool summon = attack >= FF6_FIRST_GENJU_ATTACK && attack < FF6_FIRST_NAMED_ATTACK;
    const bool fromCharacter = summon || (attack >= FF6_ATTACK_FIRST_MAGITEK && attack < FF6_ATTACK_FIRST_MAGITEK + FF6_MAGITEK_ATTACKS);  // a character fires it on the monsters
    message.actor = fromCharacter ? 0 : FF6_BATTLE_PARTY_SLOTS;
    if (fromCharacter) {
        for (size_t i = 0; i < formation_.monsters.size(); ++i) {
            if (battle_->combatants()[FF6_BATTLE_PARTY_SLOTS + i].alive()) message.animTargets.push_back(FF6_BATTLE_PARTY_SLOTS + static_cast<int>(i));
        }
    } else {
        for (int slot = 0; slot < FF6_BATTLE_PARTY_SLOTS; ++slot) {
            if (battle_->combatants()[static_cast<size_t>(slot)].present) message.animTargets.push_back(slot);
        }
    }
    startAnimation(message);
}

// The blow of a weapon: it swings in the hand of the attacker and its hit shows on the target.
void BattleScreen::startBlow(const Message& message) {
    animated_ = false;
    if (!player_ || !anim_ || message.attack < 0) return;
    AnimBody attacker, target;
    if (!body(message.actor, attacker) || !body(message.subject, target)) return;
    const bool flipped = attackFlips(attacker, target);
    player_->startWeapon(anim_->weapon(message.attack), attacker, target, flipped, message.leftHand, false);
    player_->presetHidden(keptHidden_);
    animated_ = !player_->done();
}

bool BattleScreen::body(int combatant, AnimBody& out) const {
    if (combatant < 0 || combatant >= FF6_BATTLE_COMBATANTS) return false;
    int x = 0, y = 0, width = FF6_CHAR_WIDTH, height = FF6_CHAR_HEIGHT;
    if (combatant < FF6_BATTLE_PARTY_SLOTS) {
        out.monster = false;
        out.slot = combatant;
        out.backRow = battle_->combatants()[static_cast<size_t>(combatant)].backRow;
        x = characterX(combatant);
        y = characterTop(combatant);
    } else {
        if (!gfx_) return false;
        const size_t slot = static_cast<size_t>(combatant - FF6_BATTLE_PARTY_SLOTS);
        const BattleImage& image = monsterImage(slot);
        out.monster = true;
        out.slot = static_cast<int>(slot);
        width = image.width;
        height = image.height;
        x = monsterX(slot, width);
        y = monsterTop(slot);
    }
    out.centerX = x + width / 2;
    out.centerY = y + height / 2;
    out.topY = y;
    out.bottomY = y + height;
    out.width = width;
    return true;
}

// The plane of the picture, made when the animation needs it: from the tiles of the ROM, whose colors may turn, or from the picture
// of an esper that starts from a row of tiles. The crusader has four rows of its right half moved under its left half.
void BattleScreen::updatePlane() {
    const AnimPlane& source = player_->plane();
    if (source.kind == AnimPlane::kTiles && anim_) {
        const int cycle = player_->cycle(1);
        if (plane_.rgba.empty() || cycle != planeCycle_) {
            plane_ = anim_->mode7Plane(source.tileSet, source.mapSet, source.palette, cycle);
            planeCycle_ = cycle;
        }
        return;
    }
    if (source.kind != AnimPlane::kEsper || !gfx_ || !plane_.rgba.empty()) return;
    const BattleImage& image = gfx_->monster(FF6_ESPER_GFX_FIRST + source.esper);
    plane_.width = plane_.height = FF6_MODE7_SIDE;
    plane_.rgba.assign(static_cast<size_t>(FF6_MODE7_SIDE) * FF6_MODE7_SIDE * FF6_BYTES_PER_PIXEL, 0);
    const size_t row = static_cast<size_t>(FF6_BYTES_PER_PIXEL);
    const auto at = [&](int x, int y) { return (static_cast<size_t>(y) * FF6_MODE7_SIDE + static_cast<size_t>(x)) * row; };
    for (int y = 0; y < image.height && source.row * FF6_MENU_TILE_SIZE + y < FF6_MODE7_SIDE; ++y) {
        const int width = std::min(image.width, FF6_MODE7_SIDE);
        std::copy(&image.rgba[static_cast<size_t>(y) * image.width * row], &image.rgba[static_cast<size_t>(y) * image.width * row] + static_cast<size_t>(width) * row,
                  plane_.rgba.begin() + static_cast<std::ptrdiff_t>(at(0, source.row * FF6_MENU_TILE_SIZE + y)));
    }
    if (!source.crusader) return;
    constexpr int kMovedRows = 4, kHalfColumns = 8, kTile = FF6_MENU_TILE_SIZE;
    for (int tileRow = 0; tileRow < kMovedRows; ++tileRow) {
        for (int tileColumn = 0; tileColumn < kHalfColumns; ++tileColumn) {
            for (int y = 0; y < kTile; ++y) {
                const size_t from = at((kHalfColumns + tileColumn) * kTile, (source.row + tileRow) * kTile + y);
                const size_t to = at(tileColumn * kTile, (source.row + kMovedRows + tileRow) * kTile + y);
                std::copy(plane_.rgba.begin() + static_cast<std::ptrdiff_t>(from), plane_.rgba.begin() + static_cast<std::ptrdiff_t>(from) + kTile * static_cast<std::ptrdiff_t>(row),
                          plane_.rgba.begin() + static_cast<std::ptrdiff_t>(to));
                std::fill(plane_.rgba.begin() + static_cast<std::ptrdiff_t>(from), plane_.rgba.begin() + static_cast<std::ptrdiff_t>(from) + kTile * static_cast<std::ptrdiff_t>(row), 0);
            }
        }
    }
}

// The screen of mode 7: each pixel takes the one of the plane that the matrix gives, from the scroll and the center, and the plane
// repeats every 1024 pixels but for its 256 x 256 that hold the picture. The lines count from 1, which the game makes up for.
std::vector<uint8_t> BattleScreen::mode7Pixels() const {
    const AnimMode7& mode = player_->mode7();
    std::vector<uint8_t> screen(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL, 0);
    if (plane_.rgba.empty()) return screen;
    const auto signed13 = [](int value) {
        value &= 0x1FFF;
        return value >= 0x1000 ? value - 0x2000 : value;
    };
    const auto clipped = [](int value) { return (value & 0x2000) ? (value | ~0x3FF) : (value & 0x3FF); };
    const int centerX = signed13(mode.x), centerY = signed13(mode.y);
    const int offsetX = clipped(signed13(mode.scrollX) - centerX), offsetY = clipped(signed13(mode.scrollY - 1) - centerY);
    for (int y = 0; y < FF6_BATTLE_MODE7_LINES; ++y) {
        const int line = mode.flipY ? 255 - (y + 1) : y + 1;
        const long startX = static_cast<long>(mode.a) * offsetX + static_cast<long>(mode.b) * (offsetY + line) + (static_cast<long>(centerX) << 8);
        const long startY = static_cast<long>(mode.c) * offsetX + static_cast<long>(mode.d) * (offsetY + line) + (static_cast<long>(centerY) << 8);
        for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
            const int column = mode.flipX ? 255 - x : x;
            const long planeX = ((startX + static_cast<long>(mode.a) * column) >> 8) & (FF6_MODE7_PLANE - 1);
            const long planeY = ((startY + static_cast<long>(mode.c) * column) >> 8) & (FF6_MODE7_PLANE - 1);
            if (planeX >= FF6_MODE7_SIDE || planeY >= FF6_MODE7_SIDE) continue;
            const uint8_t* from = &plane_.rgba[(static_cast<size_t>(planeY) * FF6_MODE7_SIDE + static_cast<size_t>(planeX)) * FF6_BYTES_PER_PIXEL];
            std::copy(from, from + FF6_BYTES_PER_PIXEL, &screen[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + static_cast<size_t>(x)) * FF6_BYTES_PER_PIXEL]);
        }
    }
    return screen;
}

// The frames of the threads: the layers of the backgrounds first, the sprites over them.
void BattleScreen::drawAnimation() {
    const bool mode7 = mode7Shown();
    if (esperGfx_ >= 0 && esperStands_ && gfx_ && esperShown_ > 0 && !mode7) {  // the esper comes out of white, and goes by fading
        const BattleImage& image = gfx_->monster(esperGfx_);
        BattleImage faded = image;
        for (size_t i = 0; i + FF6_BYTES_PER_PIXEL <= faded.rgba.size(); i += FF6_BYTES_PER_PIXEL) {
            if (faded.rgba[i + FF6_ALPHA_OFFSET] == 0) continue;
            if (esperLeaving_) {
                faded.rgba[i + FF6_ALPHA_OFFSET] = static_cast<uint8_t>(faded.rgba[i + FF6_ALPHA_OFFSET] * esperShown_ / 100);
                continue;
            }
            for (size_t channel = 0; channel < 3; ++channel) {
                faded.rgba[i + channel] = static_cast<uint8_t>(faded.rgba[i + channel] + (255 - faded.rgba[i + channel]) * (100 - esperShown_) / 100);
            }
        }
        const int x = esperPlaced_ ? esperX_ : (FF6_SCREEN_WIDTH - image.width) / 2;
        const int y = esperPlaced_ ? esperY_ : std::max(0, (FF6_BATTLE_FIELD_HEIGHT - image.height) / 2);
        screen_.image(x, y, image.width, image.height, faded.rgba.data());
    }
    if (!player_ || !anim_ || player_->done()) return;
    // bg3 is first drawn apart, since bg1 adds it as the subscreen; with the high priority (the usual one) its tiles are over
    // bg1 and the sprites, and it goes on the screen last, with the low one it is behind the field and does not show.
    const bool justice = player_->behindBg1Limit() > 0;  // its sprites stop where its window does, and the picture is a little wider
    static constexpr int kPasses[4] = {2, 1, 0, 2}, kPassesUnder[4] = {2, 0, 1, 2};
    std::vector<const AnimSprite*> drawOrder;  // the sprites that go behind the combatants come first
    for (const AnimSprite& sprite : player_->sprites()) {
        if (sprite.back) drawOrder.push_back(&sprite);
    }
    const size_t backCount = drawOrder.size();
    for (const AnimSprite& sprite : player_->sprites()) {
        if (!sprite.back) drawOrder.push_back(&sprite);
    }
    for (int pass = 0; pass < 4; ++pass) {
        const int layer = justice ? kPassesUnder[pass] : kPasses[pass];
        const bool last = pass == 3;
        if (mode7 && layer > 0) continue;  // bg1 is the plane of mode 7, and there is no bg3
        if (!justice && pass == 2 && !mode7 && frontShown_) {  // the combatants that a script put in front of bg1
            screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, frontPixels_.data());
        }
        if (justice && pass == 3 && !mode7) {  // what is in front of bg1 goes over it again
            screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, objectPixels_.data());
        }
        const AnimTint& tint = player_->tint(layer == 0 ? kTintSprites : layer == 1 ? kTintBg1 : kTintBg3);
        const AnimBlend blend = player_->blend(layer);
        const int mosaicLayers = layer == 1 ? 0x01 : layer == 2 ? 0x04 : 0;  // the mosaic register names bg1 and bg3 by these bits
        const int mosaicSize = (player_->mosaic() & mosaicLayers) ? (player_->mosaic() >> 4) + 1 : 1;
        // A layer of background is first drawn apart. If it adds the subscreen, where it has a pixel the pixel replaces the one
        // of the scene, plus what the subscreen (its layers, in priority) shows there; a half addition halves only where the
        // subscreen has a pixel, as the hardware does.
        const bool added = layer > 0 && layerPixels_[layer].size() == bgPixels_.size();
        if (added && !last) std::fill(layerPixels_[layer].begin(), layerPixels_[layer].end(), 0);
        bool objectsOver = false;  // the combatants have been put back over the sprites that go behind them
        for (const AnimSprite* ordered : drawOrder) {
            const AnimSprite& sprite = *ordered;
            if (last || sprite.layer != layer) continue;
            if (layer == 0 && !sprite.back && backCount > 0 && !objectsOver) {
                screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, objectPixels_.data());
                objectsOver = true;
            }
            const AnimEsperSheet& sheet = player_->esperSheet();
            const int order = sprite.esper >= 0 ? sprite.esper : sheet.esper;
            const BattleImage* esper = gfx_ && (sprite.esper >= 0 || sheet.layer == sprite.layer) ? &gfx_->monster(FF6_ESPER_GFX_FIRST + order) : nullptr;
            const AttackFrame& frame = anim_->frame(sprite.script, sprite.frame, esper ? FF6_ESPER_SHEET_PALETTE + order : sprite.palette,
                                                    sprite.flipped, player_->cycle(layer), esper);
            if (frame.image.rgba.empty()) continue;
            const std::vector<uint8_t> pixels = tint.active ? colored(frame.image.rgba, tint) : std::vector<uint8_t>();
            const uint8_t* source = tint.active ? pixels.data() : frame.image.rgba.data();
            const AnimWave wave = layer > 0 ? player_->wave(layer) : AnimWave();
            static const AnimLineScroll noLines;
            const AnimLineScroll& lines = layer > 0 ? player_->lineScroll(layer) : noLines;
            const auto paint = [&](int x, int y, int w, int h, const uint8_t* rgba) {
                if (layer == 0 && justice) h = std::min(h, FF6_JUSTICE_WINDOW_LINES - y);
                if (h <= 0) return;
                std::vector<uint8_t> blocky;  // the mosaic: each block of the screen takes the color of its first pixel
                if (mosaicSize > 1) {
                    blocky.resize(static_cast<size_t>(w) * h * FF6_BYTES_PER_PIXEL);
                    for (int row = 0; row < h; ++row) {
                        for (int column = 0; column < w; ++column) {
                            const int fromColumn = std::max(column - (((x + column) % mosaicSize) + mosaicSize) % mosaicSize, 0);
                            const int fromRow = std::max(row - (((y + row) % mosaicSize) + mosaicSize) % mosaicSize, 0);
                            const uint8_t* from = rgba + (static_cast<size_t>(fromRow) * w + fromColumn) * FF6_BYTES_PER_PIXEL;
                            std::copy(from, from + FF6_BYTES_PER_PIXEL, &blocky[(static_cast<size_t>(row) * w + column) * FF6_BYTES_PER_PIXEL]);
                        }
                    }
                    rgba = blocky.data();
                }
                // The sprites of Justice are under bg1, and the dark pocket of the picture where the mouth sits lets them through.
                std::vector<uint8_t> pocket;
                if (layer == 1 && justice) {
                    pocket.assign(rgba, rgba + static_cast<size_t>(w) * h * FF6_BYTES_PER_PIXEL);
                    for (int row = 0; row < h; ++row) {
                        if (y + row < 0 || y + row >= FF6_BATTLE_FIELD_HEIGHT) continue;
                        for (int column = 0; column < w; ++column) {
                            if (x + column < 0 || x + column >= FF6_SCREEN_WIDTH) continue;
                            uint8_t* pixel = &pocket[(static_cast<size_t>(row) * w + column) * FF6_BYTES_PER_PIXEL];
                            const uint8_t* sprite = &layerPixels_[0][(static_cast<size_t>(y + row) * FF6_SCREEN_WIDTH + x + column) * FF6_BYTES_PER_PIXEL];
                            const int luma = (pixel[0] * 299 + pixel[1] * 587 + pixel[2] * 114) / 1000;
                            if (sprite[FF6_ALPHA_OFFSET] != 0 && luma < FF6_BATTLE_POCKET_LUMA) pixel[FF6_ALPHA_OFFSET] = 0;
                        }
                    }
                    rgba = pocket.data();
                }
                if (layer == 0) {  // kept apart, for the layer that adds the subscreen over them
                    for (int row = 0; row < h; ++row) {
                        if (y + row < 0 || y + row >= FF6_BATTLE_FIELD_HEIGHT) continue;
                        for (int column = 0; column < w; ++column) {
                            if (x + column < 0 || x + column >= FF6_SCREEN_WIDTH) continue;
                            const uint8_t* from = rgba + (static_cast<size_t>(row) * w + column) * FF6_BYTES_PER_PIXEL;
                            if (from[FF6_ALPHA_OFFSET] == 0) continue;
                            std::copy(from, from + FF6_BYTES_PER_PIXEL, &layerPixels_[0][(static_cast<size_t>(y + row) * FF6_SCREEN_WIDTH + x + column) * FF6_BYTES_PER_PIXEL]);
                        }
                    }
                }
                if (added) {
                    for (int row = 0; row < h; ++row) {
                        if (y + row < 0 || y + row >= FF6_BATTLE_FIELD_HEIGHT) continue;
                        for (int column = 0; column < w; ++column) {
                            if (x + column < 0 || x + column >= FF6_SCREEN_WIDTH) continue;
                            const uint8_t* from = rgba + (static_cast<size_t>(row) * w + column) * FF6_BYTES_PER_PIXEL;
                            if (from[FF6_ALPHA_OFFSET] == 0) continue;
                            std::copy(from, from + FF6_BYTES_PER_PIXEL, &layerPixels_[layer][(static_cast<size_t>(y + row) * FF6_SCREEN_WIDTH + x + column) * FF6_BYTES_PER_PIXEL]);
                        }
                    }
                } else if (blend.math) {
                    screen_.imageMath(x, y, w, h, rgba, blend.subtract, blend.half);
                } else {
                    screen_.image(x, y, w, h, rgba);
                }
            };
            int width = frame.image.width;
            const int height = frame.image.height;
            int baseTop = sprite.y + frame.y;
            std::vector<int> tops{baseTop};
            if (layer > 0 && height >= FF6_MODE7_SIDE) {  // the frame fills the four quarters of the map, so it goes round the other way too
                baseTop = ((baseTop % height) + height) % height;
                tops = {baseTop - height, baseTop};
            }
            int left = sprite.x + frame.x;
            const int pivot = layer == 1 ? FF6_BATTLE_BG1_PIVOT : FF6_SCREEN_WIDTH / 2;
            if (layer == 0 && justice) left = FF6_BATTLE_BG1_PIVOT + (left - FF6_BATTLE_BG1_PIVOT) * FF6_BATTLE_VEIL_STRETCH / 16;
            std::vector<uint8_t> widened;
            if (sprite.veil && (layer == 1 || sprite.frame >= FF6_BATTLE_VEIL_FIRST_FRAME)) {  // the game shows these wider, from a column
                const int wider = width * FF6_BATTLE_VEIL_STRETCH / 16;
                widened.resize(static_cast<size_t>(wider) * height * FF6_BYTES_PER_PIXEL);
                for (int row = 0; row < height; ++row) {
                    for (int column = 0; column < wider; ++column) {
                        const uint8_t* from = source + (static_cast<size_t>(row) * width + column * 16 / FF6_BATTLE_VEIL_STRETCH) * FF6_BYTES_PER_PIXEL;
                        std::copy(from, from + FF6_BYTES_PER_PIXEL, &widened[(static_cast<size_t>(row) * wider + column) * FF6_BYTES_PER_PIXEL]);
                    }
                }
                source = widened.data();
                left = pivot + (left - pivot) * FF6_BATTLE_VEIL_STRETCH / 16;
                width = wider;
            }
            std::vector<uint8_t> stretched;
            if (sprite.veil && width >= FF6_BATTLE_VEIL_MIN_WIDTH && width < FF6_SCREEN_WIDTH) {
                stretched.resize(static_cast<size_t>(FF6_SCREEN_WIDTH) * height * FF6_BYTES_PER_PIXEL);
                for (int row = 0; row < height; ++row) {
                    for (int column = 0; column < FF6_SCREEN_WIDTH; ++column) {
                        const uint8_t* from = source + (static_cast<size_t>(row) * width + std::clamp(column - left, 0, width - 1)) * FF6_BYTES_PER_PIXEL;
                        std::copy(from, from + FF6_BYTES_PER_PIXEL, &stretched[(static_cast<size_t>(row) * FF6_SCREEN_WIDTH + column) * FF6_BYTES_PER_PIXEL]);
                    }
                }
                source = stretched.data();
                width = FF6_SCREEN_WIDTH;
                left = 0;
            }
            std::vector<int> origins{left};
            if (layer > 0 && width >= FF6_SCREEN_WIDTH) {  // a layer of background goes round the screen as it scrolls
                // bg1 and bg3 have tiles of 16 x 16 on a map of 32 x 32, so they go round every 512 pixels: a frame as wide as the screen
                // (the beams and their glow) shows once, cut at the edge, and its other end does not come back on the other side.
                left = ((left % FF6_BATTLE_BG_PERIOD) + FF6_BATTLE_BG_PERIOD) % FF6_BATTLE_BG_PERIOD;
                origins = {left - FF6_BATTLE_BG_PERIOD, left};
            }
            const bool windowed = player_->layerWindowed(layer);
            for (const int top : tops) {
            if (!wave.active && !windowed && !lines.active && mosaicSize == 1) {
                for (const int origin : origins) paint(origin, top, width, height, source);
                continue;
            }
            // The layer is scrolled line by line: a sine of the amplitude of the command, one turn over the field.
            const auto swing = [](int amplitude, int phase, int line) {
                const int pixels = ((amplitude >> 1) & 7) * 2 + 2;
                return static_cast<int>(std::lround(pixels * std::sin(2 * 3.14159265358979 * ((phase + line) % 32) / 32)));
            };
            std::vector<uint8_t> clipped;
            for (const int origin : origins) {
                for (int row = 0; row < height; ++row) {
                    const int blockRow = mosaicSize > 1 ? row - (((top + row) % mosaicSize) + mosaicSize) % mosaicSize : row;  // the first line of its block
                    const int line = std::clamp(top + blockRow, 0, FF6_BATTLE_FIELD_HEIGHT - 1) * 32 / FF6_BATTLE_FIELD_HEIGHT;
                    const int scrollLine = std::clamp(top + blockRow, 0, FF6_ANIM_SCROLL_LINES - 1);
                    const int shiftX = (wave.horizontal ? swing(wave.hAmplitude, wave.hShown, line) : 0) - (lines.active ? lines.h[scrollLine] % width : 0);
                    const int from = std::clamp(blockRow + (wave.vertical ? swing(wave.vAmplitude, wave.vShown, line) : 0) + (lines.active ? lines.v[scrollLine] : 0), 0, height - 1);
                    const uint8_t* rowPixels = source + static_cast<size_t>(from) * width * FF6_BYTES_PER_PIXEL;
                    if (windowed) {  // the pixels outside the window of the layer are cut
                        clipped.assign(rowPixels, rowPixels + static_cast<size_t>(width) * FF6_BYTES_PER_PIXEL);
                        for (int column = 0; column < width; ++column) {
                            if (!player_->layerVisible(layer, origin + shiftX + column, top + row)) clipped[static_cast<size_t>(column) * FF6_BYTES_PER_PIXEL + FF6_ALPHA_OFFSET] = 0;
                        }
                        rowPixels = clipped.data();
                    }
                    paint(origin + shiftX, top + row, width, 1, rowPixels);
                }
            }
            }
        }
        if (layer == 0 && backCount > 0 && !objectsOver && !last) screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, objectPixels_.data());
        if (!added || (layer == 2 && pass == 0) || (last && !player_->bg3High())) continue;
        const std::vector<uint8_t>& own = layerPixels_[layer];
        if (!blend.math) {
            screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, own.data());
            continue;
        }
        const int subscreen = player_->subscreen();
        std::vector<uint8_t> result(own.size(), 0);
        for (size_t at = 0; at + FF6_BYTES_PER_PIXEL <= own.size(); at += FF6_BYTES_PER_PIXEL) {
            if (own[at + FF6_ALPHA_OFFSET] == 0) continue;
            int under[3] = {0, 0, 0};
            bool present = false;  // the subscreen has a pixel here (otherwise it is the backdrop, and nothing is halved)
            const auto take = [&](const std::vector<uint8_t>& from, bool opaque) {
                if (!opaque && from[at + FF6_ALPHA_OFFSET] == 0) return;
                std::copy(&from[at], &from[at] + 3, under);
                present = true;
            };
            if (subscreen & 0x02) take(bgPixels_, true);  // the lowest first, the ones in front over them
            if (subscreen & 0x10) take(behindPixels_, false);
            if (justice && layer == 2 && (subscreen & 0x10)) take(layerPixels_[0], false);  // under bg1
            if ((subscreen & 0x04) && layer != 2) take(layerPixels_[2], false);
            if ((subscreen & 0x01) && layer != 1) take(layerPixels_[1], false);
            if (subscreen & 0x10) {
                take(objectPixels_, false);
                if (layer == 2 && !justice) take(layerPixels_[0], false);
            }
            for (int channel = 0; channel < 3; ++channel) {
                int value = blend.subtract ? own[at + static_cast<size_t>(channel)] - under[channel] : own[at + static_cast<size_t>(channel)] + under[channel];
                if (blend.half && present) value /= 2;
                result[at + static_cast<size_t>(channel)] = static_cast<uint8_t>(std::clamp(value, 0, 255));
            }
            result[at + FF6_ALPHA_OFFSET] = 255;
        }
        screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, result.data());
    }
}

void BattleScreen::addFx(Fx::Kind kind, int combatant, int length, const int* color) {
    Fx fx;
    fx.kind = kind;
    fx.length = length;
    for (int channel = 0; channel < 3; ++channel) fx.color[channel] = color[channel];
    if (center(combatant, fx.x, fx.y)) fx_.push_back(fx);
}

// The effects are drawn by hand: the game's animations for every weapon and spell are scripts of their own.
void BattleScreen::drawFx() {
    const auto dot = [&](int x, int y, const int* color, int size) {
        for (int dy = 0; dy < size; ++dy) {
            for (int dx = 0; dx < size; ++dx) screen_.plot(x + dx, y + dy, color[0], color[1], color[2]);
        }
    };
    for (const Fx& fx : fx_) {
        const int steps = std::max(fx.length, 1);
        switch (fx.kind) {
            case Fx::Kind::Slash: {
                const int reach = FF6_BATTLE_SLASH_SIZE * 2 * std::min(fx.frame + 1, steps / 2) / (steps / 2);
                for (int i = 0; i < reach; ++i) dot(fx.x - FF6_BATTLE_SLASH_SIZE + i, fx.y - FF6_BATTLE_SLASH_SIZE + i, fx.color, 2);
                break;
            }
            case Fx::Kind::Ring: {
                const int radius = 2 + fx.frame * FF6_BATTLE_RING_RADIUS / steps;
                for (int ring = 0; ring < 2; ++ring) {
                    const int r = ring == 0 ? radius : radius / 2;
                    for (int step = 0; step < FF6_BATTLE_RING_POINTS; ++step) {
                        const double angle = 2 * 3.14159265358979 * step / FF6_BATTLE_RING_POINTS;
                        dot(fx.x + static_cast<int>(std::lround(r * std::cos(angle))), fx.y + static_cast<int>(std::lround(r * std::sin(angle))), fx.color, 2);
                    }
                }
                break;
            }
            case Fx::Kind::Sparkle:
                for (int k = 0; k < FF6_BATTLE_SPARKLES; ++k) {
                    const int x = fx.x - FF6_BATTLE_SPARKLES * 2 + k * 4 + (k * 7 + fx.frame) % 3;
                    const int y = fx.y + FF6_BATTLE_SPARKLE_RISE / 2 - fx.frame * FF6_BATTLE_SPARKLE_RISE / steps - (k % 3) * 4;
                    static constexpr int kWhite[3] = {255, 255, 255};
                    dot(x, y, (k + fx.frame / 2) % 2 ? fx.color : kWhite, 2);
                }
                break;
        }
    }
}

void BattleScreen::trigger(const Message& message) {
    const auto pose = [&](int who, int value, int frames, int step) {
        if (who < 0 || who >= FF6_BATTLE_COMBATANTS) return;
        poseValue_[static_cast<size_t>(who)] = value;
        poseTimer_[static_cast<size_t>(who)] = frames;
        stepTimer_[static_cast<size_t>(who)] = step;
    };
    const auto hurt = [&](int who) {
        if (who < 0 || who >= FF6_BATTLE_COMBATANTS) return;
        if (who < FF6_BATTLE_PARTY_SLOTS) pose(who, FF6_CHAR_POSE_HURT, FF6_BATTLE_POSE_FRAMES / 2, 0);
        else if (!animated_) blinkTimer_[static_cast<size_t>(who)] = FF6_BATTLE_BLINK_FRAMES;  // the animation of the game has its own show of the hit
    };
    static constexpr int kColors[8][3] = FF6_BATTLE_ELEMENT_COLORS;
    static constexpr int kWhite[3] = {255, 255, 255}, kGreen[3] = {96, 255, 96};
    const auto elementColor = [&](int element) {
        int bit = 0;
        while (bit < 7 && !(element & (1 << bit))) ++bit;
        return element ? kColors[bit] : kWhite;
    };
    switch (message.effect) {
        case Message::Effect::Cast: {
            lastElement_ = message.element;
            if (message.actor >= FF6_BATTLE_PARTY_SLOTS) monsterFlash_[static_cast<size_t>(message.actor)] = FF6_MONSTER_FLASH_FRAMES;  // it acts
            // The name of the attack stays alone in its window for the time of the message speed, then the animation comes.
            nameTimer_ = message.text.empty() ? 0 : message.largeName ? FF6_MONSTER_SPECIAL_NAME_FRAMES : FF6_BATTLE_MSG_SPEED_BASE + FF6_BATTLE_MSG_SPEED_STEP * messageSpeed_;
            const int nameLead = nameTimer_ > 0 ? nameTimer_ + FF6_BATTLE_CAST_NAME_GAP : 0;
            if (message.actor < FF6_BATTLE_PARTY_SLOTS && !magitekMode()) {
                const int frames = FF6_BATTLE_CAST_LEAD_FRAMES + FF6_BATTLE_POSE_FRAMES * 2;
                pose(message.actor, FF6_CHAR_POSE_CAST, nameLead + frames, nameLead + frames);
                castPending_ = message;
                castLead_ = nameLead + FF6_BATTLE_CAST_LEAD_FRAMES;
            } else if (nameLead > 0) {
                castPending_ = message;
                castLead_ = nameLead;
            } else {
                startAnimation(message);
            }
            break;
        }
        case Message::Effect::Strike:
            animated_ = false;
            if (message.actor >= FF6_BATTLE_PARTY_SLOTS && player_ && anim_) {  // the monster flashes, then its blow comes (FightCmdAnim)
                monsterFlash_[static_cast<size_t>(message.actor)] = FF6_MONSTER_FLASH_FRAMES;
                blowPending_ = message;
                blowLead_ = FF6_MONSTER_BLOW_LEAD;
                break;
            }
            if (message.actor < FF6_BATTLE_PARTY_SLOTS) pose(message.actor, FF6_CHAR_POSE_ATTACK, FF6_BATTLE_POSE_FRAMES / 2, FF6_BATTLE_POSE_FRAMES / 2);
            startBlow(message);
            hurt(message.subject);
            if (!animated_) {
                addFx(Fx::Kind::Slash, message.subject, FF6_BATTLE_SLASH_FRAMES, kWhite);
                if (message.flash) addFx(Fx::Kind::Slash, message.subject, FF6_BATTLE_SLASH_FRAMES + 4, kWhite);
            }
            break;
        case Message::Effect::Defend: pose(message.actor, FF6_CHAR_POSE_DEFEND, FF6_BATTLE_POSE_FRAMES, 0); break;
        case Message::Effect::Hurt:
            hurt(message.subject);
            if (!animated_) addFx(Fx::Kind::Ring, message.subject, FF6_BATTLE_RING_FRAMES, elementColor(lastElement_));
            break;
        case Message::Effect::Heal:
            if (!animated_) addFx(Fx::Kind::Sparkle, message.subject, FF6_BATTLE_SPARKLE_FRAMES, kGreen);
            break;
        case Message::Effect::Fade:
            if (message.subject >= FF6_BATTLE_PARTY_SLOTS) fadeTimer_[static_cast<size_t>(message.subject)] = FF6_BATTLE_DEATH_FRAMES;
            break;
        case Message::Effect::Escape:
            if (message.actor >= 0 && message.actor < FF6_BATTLE_PARTY_SLOTS) escapeTimer_[static_cast<size_t>(message.actor)] = FF6_BATTLE_ESCAPE_FRAMES;
            break;
        case Message::Effect::Scene: startScene(message); break;
        case Message::Effect::Visible:
            if (message.actor >= 0 && message.actor < FF6_BATTLE_PARTY_SLOTS) {
                keptHidden_[message.actor] = message.subject == 0;
                sceneControlled_[message.actor] = true;
            }
            break;
        case Message::Effect::None: break;
        case Message::Effect::Animate:
            startAnimation(message);
            if (animated_ && message.moves) sceneActive_ = true;  // flushScene keeps where the characters ended up
            if (animated_ && message.actor >= FF6_BATTLE_PARTY_SLOTS && !message.moves) {
                persistSlot_ = message.actor;
                persistLast_ = AnimShift();
            }
            break;
    }
    if (message.sfx >= 0) sound_.command(FF6_SOUND_GAME_SFX, static_cast<uint8_t>(message.sfx), FF6_SFX_PAN_CENTER);
    if (message.flash) {
        int bit = 0;
        while (bit < 7 && !(message.element & (1 << bit))) ++bit;
        const bool plain = message.element == 0;
        for (int channel = 0; channel < 3; ++channel) flashColor_[channel] = plain ? 255 : kColors[bit][channel];
        flashTimer_ = FF6_BATTLE_FLASH_FRAMES;
    }
}

// A dialog of a scene: its lines are messages, and the name of who talks goes with the first one.
void BattleScreen::queueDialog(int dialog, std::string& speaker) {
    for (const std::string& part : dialogLines(data_->battleDialog(dialog))) {
        if (!part.empty() && part.back() == ':') {
            speaker = part + " ";
            continue;
        }
        queue(text(speaker + part));
        speaker.clear();
    }
}

// A battle event (F7) as its script says: the dialogs, and what the characters do between them, which plays when the script asks.
void BattleScreen::queueScene(int event) {
    std::string speaker;
    if (!anim_ || !player_ || event < 0 || event >= FF6_BATTLE_EVENT_COUNT) {
        for (const int dialog : battleEventDialogs(event)) queueDialog(dialog, speaker);
        return;
    }
    const std::vector<uint8_t>& code = anim_->scripts;
    const auto word = [&](size_t at) { return at + 1 < code.size() ? code[at] | (code[at + 1] << 8) : 0; };
    size_t p = static_cast<size_t>(word(FF6_BATTLE_EVENT_TABLE + 2 * static_cast<size_t>(event)));
    const auto find = [&](int actor) {  // the place of a character of the script, by its actor, among the ones that fight
        for (int slot = 0; slot < FF6_BATTLE_PARTY_SLOTS; ++slot) {
            const Combatant& c = battle_->combatants()[static_cast<size_t>(slot)];
            if (c.present && party_->characters[static_cast<size_t>(c.id)].actor == actor) return slot;
        }
        return -1;
    };
    static constexpr int kAttacks[20] = FF6_EVENT_ANIM_ATTACKS;
    // The script says who comes and who goes: the ones who come are not there at first, the ones who go are.
    bool first[FF6_BATTLE_PARTY_SLOTS] = {false, false, false, false};
    for (size_t at = p; at + 1 < code.size() && code[at] != 0xFF && at < p + 4000;) {
        const int command = code[at];
        if (command == 19) {
            const int slot = find(code[at + 1] & 0x7F);
            if (slot >= 0 && !first[slot]) {
                first[slot] = true;
                Message start;
                start.effect = Message::Effect::Visible;
                start.instant = true;
                start.actor = slot;
                start.subject = (code[at + 1] & 0x80) ? 1 : 0;
                messages_.push_back(start);
            }
        }
        at += command <= 1 || command == 2 || command == 19 || command == 20 ? 2 : command >= 3 && command <= 13 ? 4 : command == 18 ? 49 : 1;
    }
    std::vector<SceneThread> threads;  // with the place of the combatant (a slot) where the body will go, when it is time
    std::vector<SceneAttack> attacks;
    for (int guard = 0; guard < 4000 && p + 1 < code.size(); ++guard) {
        const int command = code[p];
        if (command == 0xFF) break;
        if (command <= 1) {
            queueDialog(code[p + 1], speaker);
            p += 2;
        } else if (command >= 3 && command <= 6) {  // a character walks, poses...: the script is in the data
            const int slot = find(code[p + 1]);
            AnimBody body;
            if (slot >= 0) {
                body.slot = slot;
                threads.push_back({word(p + 2), body});
            }
            p += 4;
        } else if (command >= 7 && command <= 12) {  // a monster of the scene (a place among the monsters): its script has sparkles and sounds
            const int slot = FF6_BATTLE_PARTY_SLOTS + command - 7;
            if (battle_->combatants()[static_cast<size_t>(slot)].present) {
                AnimBody body;
                body.slot = slot;
                threads.push_back({word(p + 2), body});
            }
            p += 4;
        } else if (command == 13) {  // an attack of the table, over the characters or the monsters
            SceneAttack attack;
            attack.attack = code[p + 1] < 20 ? kAttacks[code[p + 1]] : 0;
            int slot = find(code[p + 2]);
            if (slot < 0) slot = 0;
            const int mask = code[p + 3];
            attack.attacker.slot = slot;
            for (int target = 0; target < 6; ++target) {
                const bool monsters = (mask & 0x80) != 0;
                if (!(mask & (1 << target)) || (!monsters && target >= FF6_BATTLE_PARTY_SLOTS)) continue;
                const int who = monsters ? FF6_BATTLE_PARTY_SLOTS + target : target;
                if (!battle_->combatants()[static_cast<size_t>(who)].present) continue;
                AnimBody place;  // who it is: the body comes when the animation starts
                place.monster = monsters;
                place.slot = target;
                attack.targets.push_back(place);
            }
            if (!attack.targets.empty()) attacks.push_back(attack);
            p += 4;
        } else if (command == 14) {
            threads.clear();
            attacks.clear();
            p += 1;
        } else if (command == 15) {
            if (!threads.empty() || !attacks.empty()) {
                Message scene;
                scene.effect = Message::Effect::Scene;
                scene.sceneThreads = threads;
                scene.sceneAttacks = attacks;
                messages_.push_back(scene);
            }
            threads.clear();
            attacks.clear();
            p += 1;
        } else if (command == 18) {
            p += 1 + 2 * 24;
        } else if (command == 19) {  // a character becomes a target (and is seen) or stops being one
            const int slot = find(code[p + 1] & 0x7F);
            if (slot >= 0) {
                Message shown;
                shown.effect = Message::Effect::Visible;
                shown.instant = true;
                shown.actor = slot;
                shown.subject = (code[p + 1] & 0x80) ? 0 : 1;
                messages_.push_back(shown);
            }
            p += 2;
        } else if (command == 2 || command == 20) {
            p += 2;
        } else {
            p += 1;
        }
    }
}

void BattleScreen::startScene(const Message& message) {
    animated_ = false;
    if (!player_) return;
    const int type = battle_->battleType();
    std::vector<SceneThread> threads;  // the places of the characters now, not when the script was read
    for (const SceneThread& spec : message.sceneThreads) {
        AnimBody who;
        if (body(spec.body.slot, who)) threads.push_back({spec.script, who});
    }
    std::vector<SceneAttack> attacks;
    for (const SceneAttack& spec : message.sceneAttacks) {
        SceneAttack attack;
        attack.attack = spec.attack;
        if (!body(spec.attacker.slot, attack.attacker)) continue;
        for (const AnimBody& place : spec.targets) {
            AnimBody reached;
            if (body(place.monster ? FF6_BATTLE_PARTY_SLOTS + place.slot : place.slot, reached)) attack.targets.push_back(reached);
        }
        if (!attack.targets.empty()) attacks.push_back(attack);
    }
    player_->startScene(threads, attacks, type == FF6_BATTLE_TYPE_BACK, type);
    player_->presetHidden(keptHidden_);
    plane_ = BattleImage();
    planeCycle_ = -1;
    esperAfterBolts_ = false;
    esperPlaced_ = false;
    animated_ = sceneActive_ = !player_->done();
}

// The characters that a scene moved stay where it left them, in the pose that it gave them.
void BattleScreen::flushScene() {
    sceneActive_ = false;
    if (!player_) return;
    for (int slot = 0; slot < FF6_BATTLE_PARTY_SLOTS; ++slot) {
        const AnimShift& shift = player_->shift(false, slot);
        keptShift_[slot].x += shift.x;
        keptShift_[slot].y += shift.y;
        keptHidden_[slot] = shift.hidden;
        if (shift.pose >= 0) {
            keptPose_[slot] = shift.pose;
            keptFlip_[slot] = shift.flip;
        }
    }
}

void BattleScreen::flushPersist() {
    if (persistSlot_ < 0) return;
    keptShift_[static_cast<size_t>(persistSlot_)].x += persistLast_.x;
    keptShift_[static_cast<size_t>(persistSlot_)].y += persistLast_.y;
    persistSlot_ = -1;
}

void BattleScreen::tickEffects() {
    ++animation_;
    switch (background_) {  // UpdateBattleBG, at each frame
        case FF6_BG_CLOUDS:
        case FF6_BG_WATERFALL: scrollY_ += FF6_BG_FALL_SPEED; break;
        case FF6_BG_TRAIN_TOP:
        case FF6_BG_TRAIN_TRACKS: scrollX_ -= FF6_BG_TRAIN_SPEED; break;
        case FF6_BG_MAGITEK_CAR:  // it shakes by a pixel or three
            scrollX_ -= FF6_BG_MAGITEK_SPEED;
            scrollY_ = -static_cast<int>((static_cast<unsigned>(animation_) * 2654435761u >> 24) & 3);
            break;
        default: break;
    }
    if (stepBack_ > 0) --stepBack_;
    if (afterAnim_ > 0) --afterAnim_;
    if (player_ && !player_->done()) {
        for (int slot = 0; slot < FF6_BATTLE_PARTY_SLOTS; ++slot) {  // where the characters stand, for their way back
            const AnimShift moved = player_->shift(false, slot);
            stepFrom_[static_cast<size_t>(slot)] = {moved.x, moved.y};
        }
        player_->tick();
        if (player_->done() && !sceneActive_) {
            bool away = false;
            for (const std::pair<int, int>& from : stepFrom_) away = away || from.first != 0 || from.second != 0;
            if (away) {
                stepBack_ = FF6_BATTLE_STEP_BACK_FRAMES;
                afterAnim_ = FF6_BATTLE_STEP_BACK_FRAMES + FF6_BATTLE_AFTER_STEP_FRAMES;  // the numbers come after it
            }
        }
        if (persistSlot_ >= 0) persistLast_ = player_->shift(true, persistSlot_ - FF6_BATTLE_PARTY_SLOTS);
        for (const int sfx : player_->takeSounds()) sound_.command(FF6_SOUND_GAME_SFX, static_cast<uint8_t>(sfx), FF6_SFX_PAN_CENTER);
        for (const std::array<int, 3>& command : player_->takeSoundCommands()) {
            sound_.command(static_cast<uint8_t>(command[0]), static_cast<uint8_t>(command[1]), static_cast<uint8_t>(command[2]));
        }
        for (const std::pair<int, int>& change : player_->takeGfxChanges()) {
            for (int slot = 0; slot < FF6_BATTLE_PARTY_SLOTS; ++slot) {
                const Combatant& c = battle_->combatants()[static_cast<size_t>(slot)];
                if (c.present && party_->characters[static_cast<size_t>(c.id)].actor == change.first) gfxOverride_[slot] = change.second;
            }
        }
        const int shake = player_->takeShake();
        if (shake >= 0) bgShake_ = shake;
        const int sparkle = player_->takeEventPalette();
        if (sparkle >= 0) eventPalette_ = sparkle;
    }
    if (sceneActive_ && (!player_ || player_->done())) flushScene();
    if (persistSlot_ >= 0 && (!player_ || player_->done())) flushPersist();
    // The esper that has a bg3 thread (Raiden's lightning) comes when that thread is over.
    const bool waiting = esperAfterBolts_ && player_ && !player_->done() && player_->layerActive(2);
    if (esperGfx_ >= 0 && !waiting) {  // the esper comes, stays while its animation plays, and goes
        ++esperAge_;
        const bool staying = (player_ && !player_->done()) || esperAge_ < FF6_ESPER_FRAMES;
        esperShown_ = staying ? std::min(100, esperShown_ + FF6_ESPER_FADE_PERCENT) : esperShown_ - FF6_ESPER_FADE_PERCENT;
        esperLeaving_ = !staying;
        if (!staying && esperShown_ <= 0) esperGfx_ = -1;
    }
    if (nameTimer_ > 0) --nameTimer_;
    if (blowLead_ > 0 && --blowLead_ == 0) {
        startBlow(blowPending_);
        static constexpr int kWhiteColor[3] = {255, 255, 255};
        if (!animated_) addFx(Fx::Kind::Slash, blowPending_.subject, FF6_BATTLE_SLASH_FRAMES, kWhiteColor);
    }
    if (castLead_ > 0 && --castLead_ == 0) {
        startAnimation(castPending_);
        if (!animating() && messageTimer_ > 0) messageTimer_ = FF6_BATTLE_MESSAGE_FRAMES;
    }
    for (Fx& fx : fx_) ++fx.frame;
    fx_.erase(std::remove_if(fx_.begin(), fx_.end(), [](const Fx& fx) { return fx.frame >= fx.length; }), fx_.end());
    for (size_t i = 0; i < poseTimer_.size(); ++i) {
        if (poseTimer_[i] > 0) --poseTimer_[i];
        if (stepTimer_[i] > 0) --stepTimer_[i];
        if (blinkTimer_[i] > 0) --blinkTimer_[i];
        if (monsterFlash_[i] > 0) --monsterFlash_[i];
        if (fadeTimer_[i] > 0 && --fadeTimer_[i] == 0) dying_[i] = false;
        if (escapeTimer_[i] > 0) --escapeTimer_[i];
    }
    // A number stays for a little over a second, and goes with the monster that it is over when this one is no longer shown.
    for (Numeral& numeral : numerals_) ++numeral.frame;
    numerals_.erase(std::remove_if(numerals_.begin(), numerals_.end(),
                                   [&](const Numeral& numeral) {
                                       const size_t target = static_cast<size_t>(numeral.target);
                                       const bool gone = numeral.target >= FF6_BATTLE_PARTY_SLOTS && !battle_->combatants()[target].alive() && !dying_[target];
                                       return numeral.frame >= FF6_BATTLE_NUMERAL_FRAMES || gone;
                                   }),
                    numerals_.end());
    if (flashTimer_ > 0) --flashTimer_;
}

void BattleScreen::collectEvents() {
    const std::vector<BattleEvent> events = battle_->takeEvents();
    // The game says nothing of the damage, the recoveries and the monsters that fall: the numbers jump over the combatants. When the
    // next event is the fall of the monster that took the blow, its fade starts a few frames after the number shows.
    const auto soon = [&](size_t index, int target) {
        size_t next = index + 1;
        while (next < events.size() && events[next].kind == BattleEvent::Kind::StatusSet && !(events[next].status & ~FF6_ST_DEAD)) ++next;  // says nothing
        if (target >= FF6_BATTLE_PARTY_SLOTS && next < events.size() && events[next].kind == BattleEvent::Kind::Defeated &&
            events[next].target == target) {
            messages_.back().wait = FF6_BATTLE_DEATH_DELAY;
        }
    };
    const auto points = [&](size_t index, int target, long amount, const char* unit, bool harm) {
        queueWith(Codes(), target, static_cast<int>(amount), static_cast<int>(harm ? Message::Effect::Hurt : Message::Effect::Heal), -1, target, 0, false);
        if (std::string(unit) == " HP") messages_.back().hpAfter = advanceHp(target, static_cast<int>(harm ? -amount : amount));
        soon(index, target);
    };
    // The animation plays on the combatants that the attack goes on to reach.
    const auto reached = [&](size_t index) {
        for (size_t next = index + 1; next < events.size(); ++next) {
            const BattleEvent& after = events[next];
            const bool effect = after.kind == BattleEvent::Kind::Damage || after.kind == BattleEvent::Kind::Heal ||
                                after.kind == BattleEvent::Kind::MpDamage || after.kind == BattleEvent::Kind::MpHeal ||
                                after.kind == BattleEvent::Kind::StatusSet || after.kind == BattleEvent::Kind::StatusCleared ||
                                after.kind == BattleEvent::Kind::Reflected || after.kind == BattleEvent::Kind::Hit ||
                                after.kind == BattleEvent::Kind::Critical || after.kind == BattleEvent::Kind::Miss;
            if (!effect) break;
            std::vector<int>& list = messages_.back().animTargets;
            if (after.target >= 0 && std::find(list.begin(), list.end(), after.target) == list.end()) list.push_back(after.target);
        }
    };
    for (size_t index = 0; index < events.size(); ++index) {
        const BattleEvent& event = events[index];
        switch (event.kind) {
            case BattleEvent::Kind::Hit:
            case BattleEvent::Kind::Critical: {
                const bool critical = event.kind == BattleEvent::Kind::Critical;
                queueWith(Codes(), event.target, event.damage, static_cast<int>(Message::Effect::Strike), event.attacker, event.target, 0, critical);
                messages_.back().hpAfter = advanceHp(event.target, -event.damage);
                soon(index, event.target);
                // The animation of the blow is the entry of the table that follows the item in the hand (the first entry is for bare
                // hands); a monster strikes with the item that its properties name, 0xFF being none.
                int weapon = FF6_ATTACK_WEAPON_FISTS;
                const Combatant& attacker = battle_->combatants()[static_cast<size_t>(event.attacker)];
                if (!attacker.monster) {
                    weapon = (party_->characters[static_cast<size_t>(attacker.id)].equipment[static_cast<size_t>(event.hand)] + 1) & 0xFF;
                } else if (attacker.charIndex < 0) {
                    weapon = (data_->monster(attacker.id)[FF6_MONSTER_PROP_ATTACK_ANIM] + 1) & 0xFF;
                }
                if (weapon >= FF6_ATTACK_WEAPON_ENTRIES) weapon = FF6_ATTACK_WEAPON_FISTS;
                if (event.item >= 0 && event.item < FF6_ATTACK_WEAPON_ENTRIES - 1) weapon = event.item + 1;  // a weapon that is thrown
                messages_.back().attack = weapon;
                messages_.back().leftHand = event.hand != 0;
                break;
            }
            case BattleEvent::Kind::Miss: queue(Codes(), event.target, FF6_BATTLE_POPUP_MISS); break;
            case BattleEvent::Kind::Defeated:
                if (event.target >= FF6_BATTLE_PARTY_SLOTS) dying_[static_cast<size_t>(event.target)] = true;
                queueWith(Codes(), -1, -1, static_cast<int>(Message::Effect::Fade), -1, event.target, 0, false);
                if (event.target >= FF6_BATTLE_PARTY_SLOTS) messages_.back().wait = FF6_BATTLE_DEATH_FRAMES;  // the monster fades before the next message
                break;
            case BattleEvent::Kind::Escaped:
                if (event.attacker >= FF6_BATTLE_PARTY_SLOTS) dying_[static_cast<size_t>(event.attacker)] = true;
                queueWith(join({nameOf(event.attacker), text(" ran away")}), -1, -1, static_cast<int>(Message::Effect::Escape), event.attacker, -1, 0, false);
                break;
            case BattleEvent::Kind::Cast: {
                Codes name = event.attack < FF6_SPELL_COUNT ? screen_.data().spellName(event.attack) : data_->attackName(event.attack);
                if (event.attack >= FF6_ATTACK_FIRST_BUSHIDO && event.attack < FF6_ATTACK_FIRST_BUSHIDO + FF6_TECH_ATTACKS) {
                    name = screen_.data().bushidoName(event.attack - FF6_ATTACK_FIRST_BUSHIDO);
                } else if (event.attack >= FF6_ATTACK_FIRST_BLITZ && event.attack < FF6_ATTACK_FIRST_BLITZ + FF6_TECH_ATTACKS) {
                    name = screen_.data().blitzName(event.attack - FF6_ATTACK_FIRST_BLITZ);
                }
                // MONSTER_SPECIAL: the special attack of a monster has the name of its own table, in the large font.
                const Combatant& caster = battle_->combatants()[static_cast<size_t>(std::max(event.attacker, 0))];
                const bool special = event.attack == FF6_AI_ATTACK_SPECIAL && event.attacker >= FF6_BATTLE_PARTY_SLOTS && caster.charIndex < 0 &&
                                     data_->monsterSpecialNames.size() >= static_cast<size_t>(caster.id + 1) * FF6_MONSTER_NAME_BYTES;
                if (special) {
                    const auto first = data_->monsterSpecialNames.begin() + static_cast<std::ptrdiff_t>(caster.id) * FF6_MONSTER_NAME_BYTES;
                    name.assign(first, first + FF6_MONSTER_NAME_BYTES);
                }
                name.erase(std::find(name.begin(), name.end(), FF6_BATTLE_NAME_PAD_CODE), name.end());
                const int element = rules_->attack(event.attack)[FF6_ATTACK_ELEMENT];
                if (name.empty()) break;
                queueWith(name, -1, -1, static_cast<int>(Message::Effect::Cast), event.attacker, -1, element, false);
                messages_.back().largeName = special;
                // The animation plays on the combatants that the attack goes on to reach.
                messages_.back().attack = event.attack;
                reached(index);
                break;
            }
            case BattleEvent::Kind::UseItem: {
                const uint8_t* name = screen_.data().itemNames.data() + static_cast<size_t>(event.item) * FF6_ITEM_NAME_BYTES;
                if (event.attack < 0 && itemAnimation(event.item) < 0) {
                    queue(Codes(name, name + FF6_ITEM_NAME_BYTES));
                    break;
                }
                queueWith(Codes(name, name + FF6_ITEM_NAME_BYTES), -1, -1, static_cast<int>(Message::Effect::Cast), event.attacker, -1, 0, false);
                messages_.back().attack = event.attack >= 0 ? event.attack : itemAnimation(event.item);  // a tool, or an item: its animation
                reached(index);
                break;
            }
            case BattleEvent::Kind::Defend:
                queueWith(join({nameOf(event.attacker), text(" defends")}), -1, -1, static_cast<int>(Message::Effect::Defend), event.attacker, -1, 0, false);
                break;
            case BattleEvent::Kind::Damage:
            case BattleEvent::Kind::Poison:
            case BattleEvent::Kind::Sap: points(index, event.target, event.damage, " HP", true); break;
            case BattleEvent::Kind::Heal:
            case BattleEvent::Kind::Regen: points(index, event.target, event.damage, " HP", false); break;
            case BattleEvent::Kind::MpDamage: points(index, event.target, event.damage, " MP", true); break;
            case BattleEvent::Kind::MpHeal: points(index, event.target, event.damage, " MP", false); break;
            case BattleEvent::Kind::StatusSet:
                if (event.status & ~FF6_ST_DEAD) queue(join({nameOf(event.target), text(": "), statusName(event.status & ~FF6_ST_DEAD)}));
                break;
            case BattleEvent::Kind::StatusCleared:
                if (event.status & FF6_ST_DEAD) {
                    queue(join({nameOf(event.target), text(" is back")}));
                } else {
                    queue(join({nameOf(event.target), text(" is cured of "), statusName(event.status)}));
                }
                break;
            case BattleEvent::Kind::NoEffect: queue(text("No effect")); break;
            case BattleEvent::Kind::Stole: {
                const uint8_t* name = screen_.data().itemNames.data() + static_cast<size_t>(event.item) * FF6_ITEM_NAME_BYTES;
                queue(join({text("Stole "), Codes(name, name + FF6_ITEM_NAME_BYTES)}));
                break;
            }
            case BattleEvent::Kind::StealFailed: queue(text("Couldn't steal anything")); break;
            case BattleEvent::Kind::Seized: {
                static constexpr const char* kHolds[5] = {" is seized", " is charmed", " is released", " is protected", " is stuck to"};
                if (event.damage == 4) queue(join({nameOf(event.attacker), text(kHolds[4]), nameOf(event.target)}));
                else queue(join({nameOf(event.target), text(kHolds[event.damage % 4])}));
                break;
            }
            case BattleEvent::Kind::Forcefield: {
                static constexpr const char* kElements[8] = {"Fire", "Ice", "Bolt", "Poison", "Wind", "Pearl", "Earth", "Water"};
                queue(join({text(kElements[event.damage & 7]), text(" is nullified")}));
                break;
            }
            case BattleEvent::Kind::GotItem: {
                const uint8_t* name = screen_.data().itemNames.data() + static_cast<size_t>(event.item) * FF6_ITEM_NAME_BYTES;
                queue(join({text("Got "), Codes(name, name + FF6_ITEM_NAME_BYTES)}));
                break;
            }
            case BattleEvent::Kind::ChangedRow:
                queue(join({nameOf(event.attacker), text(event.damage ? " moves back" : " moves forward")}));
                break;
            case BattleEvent::Kind::NotEnoughMp: queue(text("Not enough MP")); break;
            case BattleEvent::Kind::GilToss:
                queue(join({nameOf(event.attacker), text(" throws gil")}));
                break;
            case BattleEvent::Kind::Possess:
                queue(join({nameOf(event.attacker), text(" possesses "), nameOf(event.target)}));
                break;
            case BattleEvent::Kind::Runic:
                queueWith(join({nameOf(event.attacker), text(" readies Runic")}), -1, -1, static_cast<int>(Message::Effect::Defend), event.attacker, -1, 0, false);
                break;
            case BattleEvent::Kind::Learned: {
                Codes name = data_->attackName(event.attack);
                name.erase(std::find(name.begin(), name.end(), FF6_BATTLE_NAME_PAD_CODE), name.end());
                queue(join({nameOf(event.target), text(" learned "), name}));
                break;
            }
            case BattleEvent::Kind::Dialog:
                for (const std::string& part : dialogLines(data_->monsterDialog(event.damage))) queue(text(part));
                break;
            case BattleEvent::Kind::ChangeBattle:
                formation_ = data_->formation(event.damage);
                dying_.fill(false);
                break;
            case BattleEvent::Kind::Entry:
                if (event.attacker >= FF6_BATTLE_PARTY_SLOTS) dying_[static_cast<size_t>(event.attacker)] = event.damage == 1 || event.damage >= 3;
                break;
            case BattleEvent::Kind::Rage: {
                Codes name = data_->name(event.attack);
                name.erase(std::find(name.begin(), name.end(), FF6_BATTLE_NAME_PAD_CODE), name.end());
                queueWith(join({nameOf(event.attacker), text(" rages as "), name}), -1, -1, static_cast<int>(Message::Effect::Cast), event.attacker, -1, 0, false);
                break;
            }
            case BattleEvent::Kind::Leap: queue(join({nameOf(event.attacker), text(" leaps into the veldt")})); break;
            case BattleEvent::Kind::Dance: {
                static constexpr const char* kDances[FF6_DANCE_COUNT] = {"Wind Song", "Forest Suite", "Desert Aria", "Love Sonata",
                                                                         "Earth Blues", "Water Rondo", "Dusk Requiem", "Snowman Jazz"};
                queueWith(join({nameOf(event.attacker), text(": "), text(kDances[event.damage & (FF6_DANCE_COUNT - 1)])}), -1, -1,
                          static_cast<int>(Message::Effect::Cast), event.attacker, -1, 0, false);
                break;
            }
            case BattleEvent::Kind::DanceFailed: queue(join({nameOf(event.attacker), text(" stumbles")})); break;
            case BattleEvent::Kind::LearnedDance: victory_.push_back(text("Learned a dance")); break;
            case BattleEvent::Kind::Background: background_ = event.damage; scrollX_ = scrollY_ = 0; break;
            case BattleEvent::Kind::Slot: {
                static constexpr const char* kResults[8] = {"Joker Doom!", "Joker Doom!", "Dragon", "Bar", "Airship", "Chocobo", "Diamond", "Rabbit"};
                queue(text(kResults[event.damage & 7]));
                break;
            }
            case BattleEvent::Kind::Umaro:
                if (event.damage == FF6_UMARO_MOVE_THROW) queue(join({nameOf(event.attacker), text(" throws "), nameOf(event.target)}));
                else queue(join({nameOf(event.attacker), text(" charges")}));
                break;
            case BattleEvent::Kind::Scan: {
                const Combatant& scanned = battle_->combatants()[static_cast<size_t>(event.target)];
                queue(join({nameOf(event.target), text(" Lv "), text(std::to_string(scanned.level)), text("  HP "), text(std::to_string(scanned.hp)),
                            text("/"), text(std::to_string(scanned.maxHp)), text("  MP "), text(std::to_string(scanned.mp))}));
                break;
            }
            // WinBattle: what the victory brings is told after the experience, once the battle is over (see finish).
            case BattleEvent::Kind::GainLevel: victory_.push_back(join({nameOf(event.attacker), text(" gained a level")})); break;
            case BattleEvent::Kind::MagicPoints:
                victory_.insert(victory_.begin(), join({text("Got "), text(std::to_string(event.damage)), text(" Magic Point(s)")}));
                break;
            case BattleEvent::Kind::LearnedSpell: {
                Codes name = screen_.data().spellName(event.attack);
                name.erase(std::find(name.begin(), name.end(), FF6_BATTLE_NAME_PAD_CODE), name.end());
                victory_.push_back(join({nameOf(event.attacker), text(" learned "), name}));
                break;
            }
            case BattleEvent::Kind::Reflected: queue(join({nameOf(event.target), text(" reflects")})); break;
            case BattleEvent::Kind::Scene: queueScene(event.damage); break;
            case BattleEvent::Kind::Interceptor: queue(text("Interceptor blocks!")); break;
            case BattleEvent::Kind::MonsterAnim: {
                Message shown;
                if (event.damage == FF6_MONSTER_ANIM_SFX) {
                    shown.sfx = event.attack;
                } else if (player_ && event.target >= FF6_BATTLE_PARTY_SLOTS) {
                    shown.effect = Message::Effect::Animate;
                    shown.actor = event.target;
                    shown.attack = event.attack;
                    shown.animTargets = {event.target};
                } else if (player_ && event.target < 0 && event.attacker >= FF6_BATTLE_PARTY_SLOTS) {  // FA 5 and 6: all the characters run
                    shown.effect = Message::Effect::Animate;
                    shown.actor = event.attacker;
                    shown.attack = event.attack;
                    shown.moves = true;
                    for (int slot = 0; slot < FF6_BATTLE_PARTY_SLOTS; ++slot) {
                        if (battle_->combatants()[static_cast<size_t>(slot)].present) shown.animTargets.push_back(slot);
                    }
                } else {
                    break;
                }
                messages_.push_back(shown);
                break;
            }
        }
    }
}

// The message stays a while, or until the button is pressed; the next one replaces it.
// In a pincer attack the attackers turn to the side where their target stands.
bool BattleScreen::attackFlips(const AnimBody& attacker, const AnimBody& target) const {
    const int type = battle_->battleType();
    if (type == FF6_BATTLE_TYPE_PINCER && attacker.monster != target.monster) return target.centerX > attacker.centerX;
    return attacker.monster != (type == FF6_BATTLE_TYPE_BACK);
}

bool BattleScreen::magitekMode() const {
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        const Combatant& member = battle_->combatants()[static_cast<size_t>(i)];
        if (member.present && (member.status & FF6_ST_MAGITEK)) return true;
    }
    return false;
}

// A monster stays in the list of names until its message says that it is defeated.
bool BattleScreen::monsterListed(int combatant) const {
    const size_t i = static_cast<size_t>(combatant);
    return battle_->combatants()[i].alive() || (dying_[i] && fadeTimer_[i] == 0);
}

int BattleScreen::shownHp(int combatant) const {
    const size_t i = static_cast<size_t>(combatant);
    return heldHp_[i] >= 0 ? heldHp_[i] : battle_->combatants()[i].hp;
}

// The hit points once a message has shown its damage, from those that the earlier messages leave.
int BattleScreen::advanceHp(int target, int delta) {
    if (target < 0 || target >= FF6_BATTLE_COMBATANTS) return -1;
    const size_t i = static_cast<size_t>(target);
    if (heldHp_[i] < 0) heldHp_[i] = prevHp_[i];
    if (queuedHp_[i] < 0) queuedHp_[i] = heldHp_[i];
    queuedHp_[i] = std::clamp(queuedHp_[i] + delta, 0, std::max(battle_->combatants()[i].maxHp, queuedHp_[i]));
    return queuedHp_[i];
}

void BattleScreen::addNumeral(int target, int damage, bool green) {
    if (numerals_.size() >= 4) numerals_.erase(numerals_.begin());  // the game has four of them, and the oldest makes way
    numerals_.push_back({target, damage == FF6_BATTLE_POPUP_MISS ? -1 : damage, 0, green});
}

void BattleScreen::revealPopup() {
    if (pendingPopup_.target < 0 && pendingPopup_.hp < 0 && pendingPopup_.damage < 0) return;
    if (animating()) return;
    popupTarget_ = pendingPopup_.target;
    popupDamage_ = pendingPopup_.damage;
    if (pendingPopup_.target >= 0 && pendingPopup_.hp >= 0) heldHp_[static_cast<size_t>(pendingPopup_.target)] = pendingPopup_.hp;
    if (pendingPopup_.target >= 0 && pendingPopup_.damage >= 0) addNumeral(pendingPopup_.target, pendingPopup_.damage, pendingPopup_.green);
    if (messageTimer_ > 0 && pendingPopup_.damage >= 0) messageTimer_ = pendingPopup_.wait >= 0 ? pendingPopup_.wait : FF6_BATTLE_MESSAGE_FRAMES;
    pendingPopup_ = PendingPopup();
}

void BattleScreen::advanceMessage(bool skip) {
    if (animating()) {  // the damage waits for the end of the animation, unless the button cuts it short
        if (!skip || sceneActive_) return;  // a scene is not cut: the characters would stop half way
        castLead_ = 0;
        if (player_) player_->stop();
        flushPersist();
    }
    revealPopup();
    if (currentKeyWait_) {  // the message stays until a button is pressed
        if (!skip) return;
    } else if (currentFixed_) {
        skip = false;
    }
    if (messageTimer_ > 0 && !skip && --messageTimer_ > 0) return;
    messageTimer_ = 0;
    current_.clear();
    currentKeyWait_ = currentFixed_ = false;
    popupTarget_ = popupDamage_ = -1;
    pendingPopup_ = PendingPopup();
    while (!messages_.empty() && messages_.front().instant) {
        trigger(messages_.front());
        messages_.pop_front();
    }
    if (messages_.empty()) {
        animated_ = false;
        return;
    }
    current_ = messages_.front().text;
    currentName_ = messages_.front().effect == Message::Effect::Cast;
    currentLarge_ = messages_.front().largeName;
    currentKeyWait_ = messages_.front().keyWait;
    currentFixed_ = messages_.front().fixed;
    trigger(messages_.front());
    pendingPopup_ = {messages_.front().target, messages_.front().damage, messages_.front().hpAfter, messages_.front().wait,
                     messages_.front().effect == Message::Effect::Heal};
    const int wait = messages_.front().wait;
    messages_.pop_front();
    messageTimer_ = animating() ? 1 : wait >= 0 ? wait : FF6_BATTLE_MESSAGE_FRAMES;
    revealPopup();
}

// WinBattle: after a pause, the experience (until a button is pressed: its text ends with the code 0x07), then the magic
// points, the levels and what was learned, the items and the gil, each one for the time of the message speed (MsgSpeedTbl),
// with the window closed in between; the battle ends a few frames after the last one.
void BattleScreen::finish() {
    if (battle_->outcome() == BattleOutcome::Won && !finishing_) {
        finishing_ = true;
        const auto pause = [&](int frames) {
            Message gap;
            gap.wait = frames;
            gap.fixed = true;
            messages_.push_back(gap);
        };
        const auto say = [&](const Codes& words, bool keyWait) {
            if (!messages_.empty() && messages_.back().text.empty() == false) pause(FF6_BATTLE_WIN_GAP_FRAMES);
            Message shown{words};
            shown.wait = FF6_BATTLE_MSG_SPEED_BASE + FF6_BATTLE_MSG_SPEED_STEP * messageSpeed_;
            shown.fixed = true;
            shown.keyWait = keyWait;
            messages_.push_back(shown);
        };
        pause(FF6_BATTLE_WIN_DELAY);
        const BattleReward& reward = battle_->reward();
        if (reward.exp > 0) say(join({text("Got "), text(std::to_string(reward.exp)), text(" Exp. point(s)")}), true);
        for (const Codes& line : victory_) say(line, false);
        for (const int item : reward.items) {
            const uint8_t* name = screen_.data().itemNames.data() + static_cast<size_t>(item) * FF6_ITEM_NAME_BYTES;
            Codes words = Codes(name, name + FF6_ITEM_NAME_BYTES);
            words.erase(std::find(words.begin(), words.end(), FF6_BATTLE_NAME_PAD_CODE), words.end());
            say(join({text("Got "), words}), false);
        }
        if (reward.gil > 0) say(join({text("Got "), text(std::to_string(reward.gil)), text(" GP")}), false);
        pause(FF6_BATTLE_WIN_END_FRAMES);
        return;
    }
    if (battle_->outcome() == BattleOutcome::Won) {
        mode_ = BattleMode::Done;
    } else if (battle_->outcome() != BattleOutcome::Fled) {
        mode_ = BattleMode::Result;
    } else if (!finishing_) {
        finishing_ = true;
        queue(text("Escaped safely!"));
    } else {
        mode_ = BattleMode::Done;
    }
}

// One frame of the battle, as the game runs it (BattleLoop and UpdateBattleTime):
// - the time: every other frame the gauges fill and the actions that wait count their delay (CmdDelayTbl); with the wait mode,
//   the time stops while the list of a command (and the target of an entry of it) is open;
// - the action queue: one action at a time, once the one before has been shown; the gauge of its combatant stays full until then;
// - the menus: a character whose gauge is full gets its menu in turn, also while the actions are shown.
void BattleScreen::update(const InputState& input) {
    if (!battle_ || mode_ == BattleMode::Done) return;
    ++introFrame_;
    tickEffects();
    if (!presenting() && actions_.empty()) {  // nothing is left to show
        heldHp_.fill(-1);
        queuedHp_.fill(-1);
    }
    for (size_t i = 0; i < prevHp_.size(); ++i) {
        if (heldHp_[i] < 0) prevHp_[i] = battle_->combatants()[i].hp;
    }
    const int direction = repeat_.update(input.direction);
    if (mode_ == BattleMode::Result) {  // a lost battle: the screen of the end waits for the button
        if (input.confirm) {
            sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
            mode_ = BattleMode::Done;
        }
        return;
    }
    const bool menuOpen = mode_ != BattleMode::Fighting;
    if (battle_->outcome() == BattleOutcome::Fighting) {
        if (!presenting()) battle_->stepRandom();  // the main loop turns while no action is shown
        tickTime();
        runActions();
    }
    if (!messages_.empty() || messageTimer_ > 0) advanceMessage(input.confirm && !menuOpen);
    if (battle_->outcome() != BattleOutcome::Fighting) {
        if (menuOpen && mode_ != BattleMode::Done) mode_ = BattleMode::Fighting;
        if (!presenting()) finish();
        return;
    }
    switch (mode_) {
        case BattleMode::Fighting: updateFighting(input); break;
        case BattleMode::Command: updateCommand(input, direction); break;
        case BattleMode::Magic:
        case BattleMode::Item:
        case BattleMode::Tech: updateList(input, direction); break;
        case BattleMode::Target: updateTarget(input, direction); break;
        case BattleMode::Slot: updateSlot(input); break;
        case BattleMode::Result:
        case BattleMode::Done: break;
    }
}

bool BattleScreen::presenting() const {
    return !messages_.empty() || messageTimer_ > 0 || pendingPopup_.target >= 0 || animating();
}

// UpdateBattleTime: stopped in the wait mode while a list is open, else a tick every FF6_BATTLE_TICK_FRAMES frames.
void BattleScreen::tickTime() {
    if (introFrame_ < FF6_BATTLE_ATB_DELAY) return;  // the time starts a few frames after the fade in
    const bool inList = mode_ == BattleMode::Magic || mode_ == BattleMode::Item || mode_ == BattleMode::Tech || mode_ == BattleMode::Slot ||
                        (mode_ == BattleMode::Target && targetFromList_);
    if (wait_ && inList) return;
    if (++frames_ % FF6_BATTLE_TICK_FRAMES != 0) return;
    battle_->tick();
    collectEvents();
    for (int turn = battle_->takePendingTurn(); turn >= 0; turn = battle_->takePendingTurn()) {  // a monster, or one that acts alone
        Queued queued;
        queued.actor = turn;
        queued.run = [this, turn] { battle_->runTurn(turn); };
        waiting_.push_back(std::move(queued));
    }
    // _c21193: the delay of an action grows by half of the gauge constant of its combatant at each tick.
    for (size_t k = 0; k < waiting_.size();) {
        Queued& queued = waiting_[k];
        queued.counter += battle_->combatants()[static_cast<size_t>(queued.actor)].atbConstant / 2;
        if ((queued.counter >> FF6_BATTLE_DELAY_SHIFT) >= queued.delay) {
            actions_.push_back(std::move(queued));
            waiting_.erase(waiting_.begin() + static_cast<std::ptrdiff_t>(k));
        } else {
            ++k;
        }
    }
    for (int slot = 0; slot < FF6_BATTLE_PARTY_SLOTS; ++slot) {
        const bool ready = battle_->combatants()[static_cast<size_t>(slot)].ready;
        if (ready && !wasReady_[static_cast<size_t>(slot)]) readyTimer_[static_cast<size_t>(slot)] = FF6_BATTLE_READY_FRAMES;
        wasReady_[static_cast<size_t>(slot)] = ready;
    }
}

// ExecAction: the next action of the queue, once the one before has been shown; then the gauge of its combatant empties.
void BattleScreen::runActions() {
    if (presenting()) return;
    if (acting_ >= 0 && actionEnd_ < FF6_BATTLE_ACTION_END_FRAMES) {  // the end of the action (EndAction) takes a few frames
        ++actionEnd_;
        return;
    }
    actionEnd_ = 0;
    if (acting_ >= 0) {
        battle_->endAction(acting_);
        if (acting_ < FF6_BATTLE_PARTY_SLOTS) commanded_[static_cast<size_t>(acting_)] = false;
        acting_ = -1;
    }
    while (!actions_.empty()) {
        Queued queued = std::move(actions_.front());
        actions_.pop_front();
        const Combatant& c = battle_->combatants()[static_cast<size_t>(queued.actor)];
        if (!c.alive()) {  // it fell while it waited
            if (queued.actor < FF6_BATTLE_PARTY_SLOTS) commanded_[static_cast<size_t>(queued.actor)] = false;
            continue;
        }
        acting_ = queued.actor;
        battle_->beginAction(acting_);
        queued.run();
        collectEvents();
        break;
    }
}

// A command chosen in the menu waits for its delay (CmdDelayTbl), then for its turn in the queue of the actions.
void BattleScreen::queueCommand(int command, std::function<void()> run) {
    static constexpr int kDelay[FF6_CMD_COUNT] = FF6_CMD_DELAY_TABLE;
    Queued queued;
    queued.actor = active_;
    queued.run = std::move(run);
    queued.delay = command >= 0 && command < FF6_CMD_COUNT ? kDelay[command] : 0;
    waiting_.push_back(std::move(queued));
    commanded_[static_cast<size_t>(active_)] = true;
    mode_ = BattleMode::Fighting;
}

// The menus: the next character whose gauge has been full for a while, in the order they filled, gets its own.
void BattleScreen::updateFighting(const InputState& input) {
    if (running_ && input.cancel) {  // the buttons are let go
        running_ = false;
        battle_->setRunning(false);
    }
    for (int slot = 0; slot < FF6_BATTLE_PARTY_SLOTS; ++slot) {
        int& timer = readyTimer_[static_cast<size_t>(slot)];
        if (timer > 0 && --timer == 0) menuQueue_.push_back(slot);
    }
    if (running_) return;
    if (menuDelay_ > 0) {
        if (--menuDelay_ == 0) openCommands();
        return;
    }
    while (!menuQueue_.empty()) {
        const int slot = menuQueue_.front();
        const Combatant& c = battle_->combatants()[static_cast<size_t>(slot)];
        if (!c.ready || !c.alive() || commanded_[static_cast<size_t>(slot)]) {
            menuQueue_.pop_front();
            continue;
        }
        menuQueue_.pop_front();
        active_ = slot;
        menuDelay_ = FF6_BATTLE_MENU_OPEN_FRAMES;  // the window of the commands comes a few frames after the gauge turns yellow
        return;
    }
}

// The menu closes if its character cannot act any more.
bool BattleScreen::keepsMenu() {
    const Combatant& character = battle_->combatants()[static_cast<size_t>(active_)];
    if (battle_->outcome() == BattleOutcome::Fighting && character.alive() && character.ready && !commanded_[static_cast<size_t>(active_)]) return true;
    mode_ = BattleMode::Fighting;
    return false;
}

// The commands of the character come from its actor; the ones that the port does not have yet are shown dimmed.
void BattleScreen::openCommands() {
    commands_.clear();
    xMagic_ = 0;
    const Combatant& c = battle_->combatants()[static_cast<size_t>(active_)];
    const uint8_t* actor = rules_->actor(party_->characters[static_cast<size_t>(c.id)].actor);
    for (int slot = 0; slot < FF6_CHAR_COMMANDS; ++slot) {
        int id = actor[FF6_CHAR_PROP_COMMANDS + slot] == FF6_CMD_MORPH && (c.status & FF6_ST_MORPH) ? FF6_CMD_REVERT
                                                                                                : actor[FF6_CHAR_PROP_COMMANDS + slot];
        // The relics that change a command: Fight to Jump, Magic to X Magic, Sketch to Control, Slot to GP Rain, Steal to Capture.
        if (id == FF6_CMD_FIGHT && (c.relic2 & FF6_RELIC2_JUMP)) id = FF6_CMD_JUMP;
        else if (id == FF6_CMD_MAGIC && (c.relic2 & FF6_RELIC2_X_MAGIC)) id = FF6_CMD_X_MAGIC;
        else if (id == FF6_CMD_SKETCH && (c.relic2 & FF6_RELIC2_CONTROL)) id = FF6_CMD_CONTROL;
        else if (id == FF6_CMD_SLOT && (c.relic2 & FF6_RELIC2_GIL_TOSS)) id = FF6_CMD_GP_RAIN;
        else if (id == FF6_CMD_STEAL && (c.relic2 & FF6_RELIC2_CAPTURE)) id = FF6_CMD_CAPTURE;
        // In a magitek armor or in a battle of magic only, Fight becomes Magitek (or Magic), and what is not Magic, Item or Mimic is gone.
        const bool armored = (c.status & FF6_ST_MAGITEK) != 0, magicOnly = battle_->magicOnly();
        if ((armored || magicOnly) && id != FF6_CMD_ITEM && id != FF6_CMD_MIMIC) {
            if (id == FF6_CMD_RAGE) id = FF6_CMD_FIGHT;
            id = id == FF6_CMD_FIGHT ? FF6_CMD_MAGITEK : id == FF6_CMD_MAGIC ? id : FF6_CMD_COUNT;
            if (magicOnly) id = id == FF6_CMD_MAGIC ? FF6_CMD_COUNT : id == FF6_CMD_MAGITEK ? FF6_CMD_MAGIC : id;
        }
        Entry entry;
        if (id >= FF6_CMD_COUNT || !kCommandNames[id][0] || id == FF6_CMD_DEFEND || id == FF6_CMD_ROW) {  // a gap, where the command was
            entry.value = -1;
            entry.enabled = false;
            commands_.push_back(entry);
            continue;
        }
        entry.name = text(kCommandNames[id]);
        entry.value = id;
        entry.enabled = id == FF6_CMD_FIGHT || id == FF6_CMD_ITEM || id == FF6_CMD_DEFEND || id == FF6_CMD_STEAL ||
                        id == FF6_CMD_BUSHIDO || id == FF6_CMD_BLITZ || (id == FF6_CMD_MAGIC && battle_->canCast(active_)) ||
                        (id == FF6_CMD_X_MAGIC && battle_->canCast(active_)) || id == FF6_CMD_SHOCK || id == FF6_CMD_HEALTH ||
                        (id == FF6_CMD_TOOLS && hasTools()) || (id == FF6_CMD_LORE && party_->lores != 0) || (id == FF6_CMD_MORPH && battle_->canMorph()) ||
                        id == FF6_CMD_REVERT || id == FF6_CMD_RUNIC || id == FF6_CMD_JUMP || id == FF6_CMD_MAGITEK || id == FF6_CMD_SKETCH || id == FF6_CMD_CONTROL || id == FF6_CMD_CAPTURE || id == FF6_CMD_POSSESS || id == FF6_CMD_GP_RAIN ||
                        (id == FF6_CMD_THROW && hasWeapons()) || (id == FF6_CMD_MIMIC && battle_->canMimic()) || id == FF6_CMD_SLOT ||
                        (id == FF6_CMD_RAGE && knowsRage()) || (id == FF6_CMD_LEAP && battle_->canLeap()) || (id == FF6_CMD_DANCE && party_->dances != 0);
        commands_.push_back(entry);
    }
    if (party_->characters[static_cast<size_t>(c.id)].esper >= 0) {  // the esper that the character carries
        Entry summon;
        summon.name = text(kCommandNames[FF6_CMD_SUMMON]);
        summon.value = FF6_CMD_SUMMON;
        summon.enabled = !battle_->summoned();
        commands_.push_back(summon);
    }
    Entry row;
    row.name = text(kCommandNames[FF6_CMD_ROW]);
    row.value = FF6_CMD_ROW;
    Entry defend;
    defend.name = text(kCommandNames[FF6_CMD_DEFEND]);
    defend.value = FF6_CMD_DEFEND;
    rowDefend_ = {row, defend};
    Entry run;
    run.name = text("Run");
    run.value = FF6_BATTLE_COMMAND_RUN;
    run_ = {run};
    page_ = mainCommand_ = command_ = 0;
    while (command_ < static_cast<int>(commands_.size()) - 1 && commands_[static_cast<size_t>(command_)].value < 0) ++command_;
    mode_ = BattleMode::Command;
}

// The spells that the character knows, with their cost, or the items of the inventory that a battle can use.
void BattleScreen::openList(BattleMode mode, bool tools) {
    throwList_ = false;
    list_.clear();
    const Combatant& c = battle_->combatants()[static_cast<size_t>(active_)];
    if (mode == BattleMode::Magic) {
        for (int spell = 0; spell < FF6_SPELL_COUNT; ++spell) {
            if (!party_->spells[static_cast<size_t>(c.id)][static_cast<size_t>(spell)]) continue;
            Entry entry;
            entry.name = screen_.data().spellName(spell);
            entry.value = spell;
            entry.amount = rules_->attack(spell)[FF6_MAGIC_PROP_MP];
            entry.enabled = c.mp >= entry.amount;
            list_.push_back(entry);
        }
    } else {
        for (const InventorySlot& slot : party_->inventory) {
            if (slot.item == FF6_ITEM_EMPTY || slot.quantity <= 0) continue;
            const uint8_t* prop = rules_->item(slot.item);
            const int type = prop[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK;
            if (tools ? type != FF6_ITEM_TYPE_TOOL : (type != FF6_ITEM_TYPE_CONSUMABLE || !(prop[FF6_ITEM_PROP_USAGE] & FF6_ITEM_USAGE_BATTLE))) {
                continue;
            }
            const uint8_t* name = screen_.data().itemNames.data() + static_cast<size_t>(slot.item) * FF6_ITEM_NAME_BYTES;
            Entry entry;
            entry.name = Codes(name, name + FF6_ITEM_NAME_BYTES);
            entry.value = slot.item;
            entry.amount = tools ? -1 : slot.quantity;
            list_.push_back(entry);
        }
    }
    listMode_ = mode;
    listCursor_ = listTop_ = 0;
    mode_ = mode;
}

// The tools of Edgar are items of the inventory that the command Tools shows.
bool BattleScreen::hasTools() const {
    for (const InventorySlot& slot : party_->inventory) {
        if (slot.item != FF6_ITEM_EMPTY && slot.quantity > 0 && (rules_->item(slot.item)[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_TOOL) return true;
    }
    return false;
}

void BattleScreen::updateCommand(const InputState& input, int direction) {
    if (!keepsMenu()) return;
    // Left brings the window of Row and Defend, right the one of Run, and the other side or cancel goes back.
    const int side = direction == Left ? -1 : direction == Right ? 1 : 0;
    if ((side != 0 && page_ == 0) || (page_ != 0 && (side == -page_ || input.cancel))) {
        if (page_ == 0) mainCommand_ = command_;
        page_ = page_ == 0 ? side : 0;
        command_ = page_ == 0 ? mainCommand_ : 0;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        return;
    }
    const std::vector<Entry>& shown = shownCommands();
    const int count = static_cast<int>(shown.size());
    if (direction == Up || direction == Down) {  // the gaps are skipped
        int next = command_;
        for (int step = 0; step < count; ++step) {
            next = (next + (direction == Down ? 1 : count - 1)) % count;
            if (shown[static_cast<size_t>(next)].value >= 0) break;
        }
        command_ = next;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (!input.confirm) return;
    const Entry& entry = shown[static_cast<size_t>(command_)];
    if (!entry.enabled) return;
    sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
    controlList_ = rageList_ = danceList_ = false;
    chosenCommand_ = entry.value;
    columns_ = 1;
    listOpening_ = FF6_BATTLE_LIST_OPEN_FRAMES;  // the window of a list rises (_c15729)
    switch (entry.value) {
        case FF6_CMD_RAGE: openRage(); break;
        case FF6_CMD_DANCE: openDance(); break;
        case FF6_CMD_LEAP: {
            const int user = active_;
            queueCommand(entry.value, [this, user] { battle_->leap(user); });
            break;
        }
        case FF6_CMD_SLOT:
            for (int reel = 0; reel < 3; ++reel) {
                reels_[static_cast<size_t>(reel)] = (frames_ + reel * 2) % FF6_SLOT_ICONS;
                reelSpeed_[static_cast<size_t>(reel)] = FF6_SLOT_SPEED_BASE + reel * FF6_SLOT_SPEED_STEP;
            }
            slotStopped_ = slotFrame_ = 0;
            mode_ = BattleMode::Slot;
            break;
        case FF6_CMD_FIGHT: {
            Attack fight;
            fight.target = FF6_TARGET_MANUAL | FF6_TARGET_ONE_SIDE | FF6_TARGET_ENEMY;
            const Combatant& fighter = battle_->combatants()[static_cast<size_t>(active_)];
            if (fighter.relic4 & FF6_RELIC4_X_FIGHT) fight.target = FF6_TARGET_ONE_SIDE | FF6_TARGET_ENEMY | FF6_TARGET_INIT_ALL;  // the offering strikes at random
            beginTarget(Pending::Fight, 0, fight);
            break;
        }
        case FF6_CMD_MAGIC: openList(BattleMode::Magic); break;
        case FF6_CMD_X_MAGIC:
            xMagic_ = 1;
            openList(BattleMode::Magic);
            break;
        case FF6_CMD_SHOCK:
        case FF6_CMD_HEALTH: {
            listMode_ = BattleMode::Command;  // the target goes back to the commands
            const int attack = entry.value == FF6_CMD_SHOCK ? FF6_ATTACK_SHOCK : FF6_ATTACK_CURA;
            beginTarget(Pending::Spell, attack, battle_->spellAttack(attack));
            break;
        }
        case FF6_CMD_BUSHIDO: openTechs(false); break;
        case FF6_CMD_BLITZ: openTechs(true); break;
        case FF6_CMD_LORE: openLore(); break;
        case FF6_CMD_THROW: openThrow(); break;
        case FF6_CMD_SUMMON: openSummon(); break;
        case FF6_CMD_MAGITEK: openMagitek(); break;
        case FF6_CMD_JUMP: {
            Attack jump;
            jump.target = FF6_TARGET_MANUAL | FF6_TARGET_ONE_SIDE | FF6_TARGET_ENEMY;
            beginTarget(Pending::Jump, 0, jump);
            break;
        }
        case FF6_CMD_SKETCH:
        case FF6_CMD_CONTROL:
        case FF6_CMD_CAPTURE:
        case FF6_CMD_POSSESS:
        case FF6_CMD_GP_RAIN: {
            Attack aimed;
            aimed.target = FF6_TARGET_MANUAL | FF6_TARGET_ONE_SIDE | FF6_TARGET_ENEMY;
            if (entry.value == FF6_CMD_GP_RAIN) aimed.target |= FF6_TARGET_MULTI;
            const Pending which = entry.value == FF6_CMD_SKETCH ? Pending::Sketch : entry.value == FF6_CMD_CONTROL ? Pending::Control
                                : entry.value == FF6_CMD_CAPTURE ? Pending::Capture : entry.value == FF6_CMD_POSSESS ? Pending::Possess
                                                                                                              : Pending::GpRain;
            beginTarget(which, 0, aimed);
            break;
        }
        case FF6_CMD_MIMIC: {
            const int user = active_;
            queueCommand(entry.value, [this, user] { battle_->mimic(user); });
            break;
        }
        case FF6_CMD_MORPH:
        case FF6_CMD_REVERT:
        case FF6_CMD_RUNIC: {
            const int user = active_, command = entry.value;
            queueCommand(command, [this, user, command] {
                if (command == FF6_CMD_MORPH) battle_->morph(user);
                else if (command == FF6_CMD_REVERT) battle_->revert(user);
                else battle_->runic(user);
            });
            break;
        }
        case FF6_CMD_STEAL: {
            Attack steal;
            steal.target = FF6_TARGET_MANUAL | FF6_TARGET_ONE_SIDE | FF6_TARGET_ENEMY;
            beginTarget(Pending::Steal, 0, steal);
            break;
        }
        case FF6_CMD_ROW: {
            const int user = active_;
            queueCommand(entry.value, [this, user] { battle_->changeRow(user); });
            break;
        }
        case FF6_CMD_ITEM: openList(BattleMode::Item); break;
        case FF6_CMD_TOOLS: openList(BattleMode::Item, true); break;
        case FF6_CMD_DEFEND: {
            const int user = active_;
            queueCommand(entry.value, [this, user] { battle_->defend(user); });
            break;
        }
        default:
            if (!battle_->canRun()) {  // a monster holds the party back
                queue(text("Can't escape!"));
                break;
            }
            running_ = true;
            battle_->setRunning(true);
            mode_ = BattleMode::Fighting;
            break;
    }
}

void BattleScreen::updateList(const InputState& input, int direction) {
    if (!keepsMenu()) return;
    if (listOpening_ > 0) {  // the window rises over the others; the hand comes once it is in place
        --listOpening_;
        return;
    }
    const int count = static_cast<int>(list_.size());
    if (count > 0 && columns_ > 1 && direction != FF6_NO_DIRECTION) {  // a grid: the places that are empty are passed over
        const int step = direction == Down ? columns_ : direction == Up ? -columns_ : direction == Right ? 1 : -1;
        int next = listCursor_;
        for (int tries = 0; tries < count; ++tries) {
            next = ((next + step) % count + count) % count;
            if (list_[static_cast<size_t>(next)].value >= 0) break;
        }
        if (next != listCursor_) sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        listCursor_ = next;
    } else if (count > 0 && (direction == Up || direction == Down)) {
        listCursor_ = (listCursor_ + (direction == Down ? 1 : count - 1)) % count;
        listTop_ = std::clamp(listTop_, std::max(listCursor_ - FF6_BATTLE_LIST_ROWS + 1, 0), listCursor_);
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (input.cancel) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        mode_ = BattleMode::Command;
        xMagic_ = 0;
        return;
    }
    if (!input.confirm || count == 0 || !list_[static_cast<size_t>(listCursor_)].enabled) return;
    sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
    const int value = list_[static_cast<size_t>(listCursor_)].value;
    if (rageList_ || danceList_) {  // no target: the character goes on alone
        const int user = active_;
        const bool rage = rageList_;
        queueCommand(rage ? FF6_CMD_RAGE : FF6_CMD_DANCE, [this, user, rage, value] {
            if (rage) battle_->rage(user, value);
            else battle_->dance(user, value);
        });
        return;
    }
    if (controlList_) {  // an attack of the monster, at the party unless it heals its own side
        Attack given = battle_->spellAttack(value);
        given.target ^= FF6_TARGET_ENEMY;
        beginTarget(Pending::ControlAttack, value, given);
    } else if (listMode_ == BattleMode::Item && throwList_) {
        Attack thrown;
        thrown.target = FF6_TARGET_MANUAL | FF6_TARGET_ONE_SIDE | FF6_TARGET_ENEMY;
        beginTarget(Pending::Throw, value, thrown);
    } else if (listMode_ == BattleMode::Item) {
        beginTarget(Pending::Item, value, attackFromItem(rules_->item(value)));
    } else {
        beginTarget(Pending::Spell, value, battle_->spellAttack(value));
    }
}

// The lores that Strago knows: attacks of the table that cost mp and go through the silence.
void BattleScreen::openLore() {
    list_.clear();
    const Combatant& c = battle_->combatants()[static_cast<size_t>(active_)];
    for (int lore = 0; lore < FF6_LORE_COUNT; ++lore) {
        if (!(party_->lores >> lore & 1)) continue;
        const int attack = FF6_ATTACK_FIRST_LORE + lore;
        Entry entry;
        entry.name = data_->attackName(attack);
        entry.name.erase(std::find(entry.name.begin(), entry.name.end(), FF6_BATTLE_NAME_PAD_CODE), entry.name.end());
        entry.value = attack;
        entry.amount = rules_->attack(attack)[FF6_MAGIC_PROP_MP];
        entry.enabled = c.mp >= entry.amount;
        list_.push_back(entry);
    }
    listMode_ = BattleMode::Tech;
    listCursor_ = listTop_ = 0;
    mode_ = BattleMode::Tech;
}

bool BattleScreen::hasWeapons() const {
    for (const InventorySlot& slot : party_->inventory) {
        if (slot.item != FF6_ITEM_EMPTY && slot.quantity > 0 && (rules_->item(slot.item)[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_WEAPON) return true;
    }
    return false;
}

// The weapons of the inventory, one of which is thrown away.
void BattleScreen::openThrow() {
    list_.clear();
    for (const InventorySlot& slot : party_->inventory) {
        if (slot.item == FF6_ITEM_EMPTY || slot.quantity <= 0) continue;
        if ((rules_->item(slot.item)[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK) != FF6_ITEM_TYPE_WEAPON) continue;
        const uint8_t* name = screen_.data().itemNames.data() + static_cast<size_t>(slot.item) * FF6_ITEM_NAME_BYTES;
        Entry entry;
        entry.name = Codes(name, name + FF6_ITEM_NAME_BYTES);
        entry.value = slot.item;
        entry.amount = slot.quantity;
        list_.push_back(entry);
    }
    listMode_ = BattleMode::Item;
    throwList_ = true;
    listCursor_ = listTop_ = 0;
    mode_ = BattleMode::Item;
}

// A list of attacks of the table, with their cost.
void BattleScreen::addAttack(const Combatant& c, int attack) {
    Entry entry;
    entry.name = data_->attackName(attack);
    entry.name.erase(std::find(entry.name.begin(), entry.name.end(), FF6_BATTLE_NAME_PAD_CODE), entry.name.end());
    entry.value = attack;
    entry.amount = rules_->attack(attack)[FF6_MAGIC_PROP_MP];
    entry.enabled = c.mp >= entry.amount;
    list_.push_back(entry);
}

// The esper that the character carries: the attack of the table that the esper is.
void BattleScreen::openSummon() {
    list_.clear();
    const Combatant& c = battle_->combatants()[static_cast<size_t>(active_)];
    const int esper = party_->characters[static_cast<size_t>(c.id)].esper;
    if (esper >= 0) addAttack(c, FF6_FIRST_GENJU_ATTACK + esper);
    listMode_ = BattleMode::Tech;
    listCursor_ = listTop_ = 0;
    mode_ = BattleMode::Tech;
}

// The attacks that the controlled monster knows, from the table of the game.
void BattleScreen::openControl(int monster) {
    list_.clear();
    const int id = battle_->combatants()[static_cast<size_t>(monster)].id;
    for (int slot = 0; slot < FF6_CONTROL_BYTES; ++slot) {
        const int attack = data_->controlAttack(id, slot);
        if (attack >= FF6_AI_ATTACK_NOTHING) continue;
        Entry entry;
        if (attack == FF6_AI_ATTACK_BATTLE) entry.name = text("Fight");
        else if (attack == FF6_AI_ATTACK_SPECIAL) entry.name = text("Special");
        else if (attack < FF6_SPELL_COUNT) entry.name = screen_.data().spellName(attack);
        else entry.name = data_->attackName(attack);
        entry.name.erase(std::find(entry.name.begin(), entry.name.end(), FF6_BATTLE_NAME_PAD_CODE), entry.name.end());
        entry.value = attack;
        list_.push_back(entry);
    }
    listMode_ = BattleMode::Tech;
    controlList_ = true;
    listCursor_ = listTop_ = 0;
    mode_ = BattleMode::Tech;
}

bool BattleScreen::knowsRage() const {
    return std::find(party_->rages.begin(), party_->rages.end(), true) != party_->rages.end();
}

// The monsters whose rage Gau knows.
void BattleScreen::openRage() {
    list_.clear();
    for (int monster = 0; monster < FF6_RAGE_MONSTERS; ++monster) {
        if (!party_->rages[static_cast<size_t>(monster)]) continue;
        Entry entry;
        entry.name = data_->name(monster);
        entry.name.erase(std::find(entry.name.begin(), entry.name.end(), FF6_BATTLE_NAME_PAD_CODE), entry.name.end());
        entry.value = monster;
        list_.push_back(entry);
    }
    rageList_ = true;
    listMode_ = BattleMode::Tech;
    listCursor_ = listTop_ = 0;
    mode_ = BattleMode::Tech;
}

// The dances that Mog knows.
void BattleScreen::openDance() {
    static constexpr const char* kDances[FF6_DANCE_COUNT] = {"Wind Song", "Forest Suite", "Desert Aria", "Love Sonata",
                                                             "Earth Blues", "Water Rondo", "Dusk Requiem", "Snowman Jazz"};
    list_.clear();
    for (int dance = 0; dance < FF6_DANCE_COUNT; ++dance) {
        if (!(party_->dances >> dance & 1)) continue;
        Entry entry;
        entry.name = text(kDances[dance]);
        entry.value = dance;
        list_.push_back(entry);
    }
    danceList_ = true;
    listMode_ = BattleMode::Tech;
    listCursor_ = listTop_ = 0;
    mode_ = BattleMode::Tech;
}

// The slot: three reels turn, and the button stops them one after the other.
void BattleScreen::updateSlot(const InputState& input) {
    if (!keepsMenu()) return;
    ++slotFrame_;
    for (int reel = slotStopped_; reel < 3; ++reel) {
        if (slotFrame_ % reelSpeed_[static_cast<size_t>(reel)] == 0) reels_[static_cast<size_t>(reel)] = (reels_[static_cast<size_t>(reel)] + 1) % FF6_SLOT_ICONS;
    }
    if (input.cancel) {
        mode_ = BattleMode::Command;
        return;
    }
    if (!input.confirm) return;
    sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
    if (++slotStopped_ < 3) return;
    const int user = active_;
    const std::array<int, 3> reels = reels_;
    queueCommand(FF6_CMD_SLOT, [this, user, reels] { battle_->slot(user, reels); });
}

void BattleScreen::drawSlot() {
    static constexpr const char* kIcons[FF6_SLOT_ICONS] = {"7", "Dragon", "Bar", "Airship", "Chocobo", "Diamond", "Rabbit"};
    screen_.window(FF6_BATTLE_SLOT_X, FF6_BATTLE_SLOT_Y, FF6_BATTLE_SLOT_W, FF6_BATTLE_SLOT_H);
    for (int reel = 0; reel < 3; ++reel) {
        put(FF6_BATTLE_SLOT_X + 2 + reel * FF6_BATTLE_SLOT_STEP, FF6_BATTLE_SLOT_Y + 2, text(kIcons[reels_[static_cast<size_t>(reel)]]),
            reel < slotStopped_ ? FF6_MENU_BG3_YELLOW : FF6_MENU_BG3_DEFAULT);
    }
}

// DrawMagitekListText: two to a line, at their places; Terra has the eight, the others only some (TerraMagitekAttackTbl and
// DefaultMagitekAttackTbl), the rest of the places stay empty. No cost is shown.
void BattleScreen::openMagitek() {
    list_.clear();
    const Combatant& c = battle_->combatants()[static_cast<size_t>(active_)];
    const int gfx = characterGfx_[static_cast<size_t>(c.id)] >= 0 ? characterGfx_[static_cast<size_t>(c.id)] : characterSprite(c.id);
    static constexpr int kDefault[FF6_MAGITEK_ATTACKS] = FF6_MAGITEK_DEFAULT_LIST;
    for (int i = 0; i < FF6_MAGITEK_ATTACKS; ++i) {
        const int attack = gfx == FF6_CHAR_GFX_TERRA_MAGITEK ? i : kDefault[i];
        if (attack < 0) {
            Entry gap;
            gap.value = -1;
            gap.enabled = false;
            list_.push_back(gap);
            continue;
        }
        addAttack(c, FF6_ATTACK_FIRST_MAGITEK + attack);
        list_.back().amount = -1;
    }
    listMode_ = BattleMode::Tech;
    listCursor_ = listTop_ = 0;
    columns_ = FF6_BATTLE_MAGITEK_COLUMNS;
    mode_ = BattleMode::Tech;
}

// The swordtechs or the blitzes that the character has learned, as attacks of the table.
void BattleScreen::openTechs(bool blitz) {
    list_.clear();
    const unsigned known = blitz ? party_->blitzes : party_->swordtechs;
    for (int tech = 0; tech < FF6_TECH_ATTACKS; ++tech) {
        if (!(known >> tech & 1)) continue;
        Entry entry;
        entry.name = blitz ? screen_.data().blitzName(tech) : screen_.data().bushidoName(tech);
        entry.value = (blitz ? FF6_ATTACK_FIRST_BLITZ : FF6_ATTACK_FIRST_BUSHIDO) + tech;
        list_.push_back(entry);
    }
    listMode_ = BattleMode::Tech;
    listCursor_ = listTop_ = 0;
    mode_ = BattleMode::Tech;
}

// What an attack can be aimed at: a cursor on one combatant, or on a side, or on everybody; the targets that need no
// choice are taken at once.
void BattleScreen::beginTarget(Pending pending, int value, const Attack& attack) {
    targetFromList_ = mode_ == BattleMode::Magic || mode_ == BattleMode::Item || mode_ == BattleMode::Tech;
    pending_ = pending;
    pendingValue_ = value;
    pendingAttack_ = attack;
    const int init = attack.target & FF6_TARGET_INIT_MASK;
    startsOnMonsters_ = (attack.target & FF6_TARGET_ENEMY) != 0;
    group_ = init != 0;
    mode_ = BattleMode::Target;
    if (!(attack.target & FF6_TARGET_MANUAL)) {
        if (init == 0) target_ = active_;
        else target_ = startsOnMonsters_ ? FF6_BATTLE_PARTY_SLOTS : 0;
        if (!targetable(target_)) moveTarget(1);
        confirmTarget();
        return;
    }
    target_ = startsOnMonsters_ ? FF6_BATTLE_COMBATANTS - 1 : FF6_BATTLE_PARTY_SLOTS - 1;
    moveTarget(1);
    if (startsOnMonsters_ && gfx_) {  // _c17922: the monster furthest to the right (to the left when the screen is turned round)
        const bool back = battle_->battleType() == FF6_BATTLE_TYPE_BACK;
        int best = -1, bestX = 0;
        for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {
            if (!targetable(i)) continue;
            const size_t slot = static_cast<size_t>(i - FF6_BATTLE_PARTY_SLOTS);
            const int width = monsterImage(slot).width;
            const int x = back ? -monsterX(slot, width) : monsterX(slot, width) + width;
            if (best < 0 || x > bestX) best = i, bestX = x;
        }
        if (best >= 0) target_ = best;
    }
    if (!targetable(target_)) switchSide();
}

bool BattleScreen::targetable(int combatant) const {
    if (!battle_->canTarget(pendingAttack_, combatant)) return false;
    if (pendingAttack_.target & FF6_TARGET_ONE_SIDE) return (combatant >= FF6_BATTLE_PARTY_SLOTS) == startsOnMonsters_;
    return true;
}

std::vector<int> BattleScreen::selection() const {
    if (!group_) return {target_};
    const bool everybody = (pendingAttack_.target & FF6_TARGET_INIT_MASK) == FF6_TARGET_INIT_ALL;
    std::vector<int> covered;
    for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
        if (targetable(i) && (everybody || (i >= FF6_BATTLE_PARTY_SLOTS) == (target_ >= FF6_BATTLE_PARTY_SLOTS))) covered.push_back(i);
    }
    return covered;
}

// The cursor goes through the combatants of its side.
void BattleScreen::moveTarget(int step) {
    const bool monsters = target_ >= FF6_BATTLE_PARTY_SLOTS;
    const int first = monsters ? FF6_BATTLE_PARTY_SLOTS : 0, count = monsters ? FF6_BATTLE_COMBATANTS - FF6_BATTLE_PARTY_SLOTS : FF6_BATTLE_PARTY_SLOTS;
    int at = target_;
    for (int tried = 0; tried < count; ++tried) {
        at = first + (at - first + step + count) % count;
        if (targetable(at)) {
            if (at != target_) sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
            target_ = at;
            return;
        }
    }
}

// Left and right: to the other side, if the attack can go there.
void BattleScreen::switchSide() {
    if ((pendingAttack_.target & FF6_TARGET_ONE_SIDE) || (pendingAttack_.target & FF6_TARGET_INIT_MASK) == FF6_TARGET_INIT_ALL) return;
    const bool toMonsters = target_ < FF6_BATTLE_PARTY_SLOTS;
    const int first = toMonsters ? FF6_BATTLE_PARTY_SLOTS : 0, last = toMonsters ? FF6_BATTLE_COMBATANTS : FF6_BATTLE_PARTY_SLOTS;
    for (int i = first; i < last; ++i) {
        if (!targetable(i)) continue;
        target_ = i;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        return;
    }
}

void BattleScreen::confirmTarget() {
    const std::vector<int> targets = selection();
    if (targets.empty()) return;
    if (pending_ == Pending::Control) {  // whether the monster obeys is known at once: the attack that it is given is next
        if (battle_->control(active_, targets.front())) {
            collectEvents();
            openControl(targets.front());
            return;
        }
        collectEvents();
        mode_ = BattleMode::Fighting;
        return;
    }
    const int user = active_, value = pendingValue_;
    const bool keepTurn = xMagic_ == 1;
    const Pending pending = pending_;
    const int command = chosenCommand_;
    const auto run = [this, user, value, keepTurn, pending, targets] {
        switch (pending) {
            case Pending::Fight: battle_->fight(user, targets.front()); break;
            case Pending::Spell: battle_->cast(user, value, targets, keepTurn); break;
            case Pending::Item: battle_->useItem(user, value, targets); break;
            case Pending::Steal: battle_->steal(user, targets.front()); break;
            case Pending::Throw: battle_->throwWeapon(user, value, targets.front()); break;
            case Pending::Jump: battle_->jump(user, targets.front()); break;
            case Pending::Sketch: battle_->sketch(user, targets.front()); break;
            case Pending::Capture: battle_->capture(user, targets.front()); break;
            case Pending::Possess: battle_->possess(user, targets.front()); break;
            case Pending::GpRain: battle_->gpRain(user, targets); break;
            case Pending::Control: break;
            case Pending::ControlAttack: {
                uint16_t mask = 0;
                for (const int target : targets) mask = static_cast<uint16_t>(mask | (1u << target));
                battle_->controlAttack(user, value, mask);
                break;
            }
        }
    };
    if (xMagic_ == 1) {  // the second spell of X Magic: both are cast as one action
        xMagicFirst_ = run;
        xMagic_ = 2;
        openList(BattleMode::Magic);
        return;
    }
    if (xMagic_ == 2) {
        const std::function<void()> first = xMagicFirst_;
        xMagic_ = 0;
        queueCommand(FF6_CMD_X_MAGIC, [first, run] {
            first();
            run();
        });
        return;
    }
    queueCommand(command, run);
}

void BattleScreen::updateTarget(const InputState& input, int direction) {
    if (!keepsMenu()) return;
    if (!targetable(target_)) {
        moveTarget(1);
        if (!targetable(target_)) switchSide();
    }
    if (!group_ || (pendingAttack_.target & FF6_TARGET_INIT_MASK) == FF6_TARGET_INIT_HALF) {
        if (direction == Up) moveTarget(-1);
        if (direction == Down) moveTarget(1);
    }
    if (direction == Left || direction == Right) switchSide();
    if (input.menu && (pendingAttack_.target & FF6_TARGET_MULTI)) {
        group_ = !group_;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (input.cancel) {
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
        mode_ = pending_ == Pending::Fight || pending_ == Pending::Steal || pending_ == Pending::Jump || pending_ == Pending::Sketch ||
                pending_ == Pending::Control || pending_ == Pending::Capture || pending_ == Pending::Possess || pending_ == Pending::GpRain ? BattleMode::Command : listMode_;
    } else if (input.confirm) {
        sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
        confirmTarget();
    }
}

void BattleScreen::put(int x, int y, const Codes& codes, int color) { screen_.codes(x, y, codes, Layer::Bg3, color); }

// A back attack turns the whole battle round; in a pincer attack the monsters whose right edge is past x = 0x68 go to the other side.
bool BattleScreen::mirrored(size_t monster, int width) const {
    const int type = battle_->battleType();
    if (type == FF6_BATTLE_TYPE_PINCER) return (formation_.positions[monster] >> 4) * FF6_BATTLE_MONSTER_X_UNIT + width >= FF6_BATTLE_PINCER_SPLIT;
    return type == FF6_BATTLE_TYPE_BACK;
}

// A scene may put a combatant where it wants it: the place is in pixels, from the top left.
int BattleScreen::monsterTop(size_t monster) const {
    const Combatant& c = battle_->combatants()[FF6_BATTLE_PARTY_SLOTS + monster];
    return (c.placeY >= 0 ? c.placeY : (formation_.positions[monster] & 0x0F) * FF6_BATTLE_MONSTER_Y_UNIT) + keptShift_[FF6_BATTLE_PARTY_SLOTS + monster].y;
}

int BattleScreen::characterTop(int slot) const {
    static constexpr int kCharY[FF6_BATTLE_PARTY_SLOTS] = FF6_BATTLE_CHAR_Y;
    const Combatant& c = battle_->combatants()[static_cast<size_t>(slot)];
    return (c.placeY >= 0 ? c.placeY : kCharY[slot]) + keptShift_[slot].y;
}

int BattleScreen::monsterX(size_t monster, int width) const {
    const Combatant& placed = battle_->combatants()[FF6_BATTLE_PARTY_SLOTS + monster];
    if (placed.placeX >= 0) return placed.placeX + keptShift_[FF6_BATTLE_PARTY_SLOTS + monster].x;
    const int x = (formation_.positions[monster] >> 4) * FF6_BATTLE_MONSTER_X_UNIT;
    // CalcMonsterPos: a back attack mirrors the screen; in a pincer attack the ones on the right are mirrored and moved 64 pixels to the right.
    const int mirroredX = FF6_SCREEN_WIDTH - x - width + (battle_->battleType() == FF6_BATTLE_TYPE_PINCER ? FF6_BATTLE_PINCER_SHIFT : 0);
    return (mirrored(monster, width) ? mirroredX : x) + keptShift_[FF6_BATTLE_PARTY_SLOTS + monster].x;
}

int BattleScreen::characterX(int slot) const {
    static constexpr int kCharY[FF6_BATTLE_PARTY_SLOTS] = FF6_BATTLE_CHAR_Y;
    const Combatant& c = battle_->combatants()[static_cast<size_t>(slot)];
    if (c.placeX >= 0) return c.placeX + keptShift_[slot].x;
    // CharXAngleTbl and CharRowOffsetTbl: where the line of the characters leans, by the type of the battle (normal, back, pincer,
    // side); a negative angle is to the left of the middle.
    static constexpr int kAngle[FF6_BATTLE_TYPE_COUNT][FF6_BATTLE_PARTY_SLOTS] = {
        {FF6_BATTLE_CHAR_ANGLE, FF6_BATTLE_CHAR_ANGLE, FF6_BATTLE_CHAR_ANGLE, FF6_BATTLE_CHAR_ANGLE},
        {-FF6_BATTLE_CHAR_ANGLE, -FF6_BATTLE_CHAR_ANGLE, -FF6_BATTLE_CHAR_ANGLE, -FF6_BATTLE_CHAR_ANGLE},
        {0, 0, 0, 0},
        {FF6_BATTLE_CHAR_ANGLE, FF6_BATTLE_CHAR_ANGLE, -FF6_BATTLE_CHAR_ANGLE, -FF6_BATTLE_CHAR_ANGLE}};
    static constexpr int kRow[FF6_BATTLE_TYPE_COUNT][FF6_BATTLE_PARTY_SLOTS] = {
        {FF6_BATTLE_CHAR_ROW_ANGLE, FF6_BATTLE_CHAR_ROW_ANGLE, FF6_BATTLE_CHAR_ROW_ANGLE, FF6_BATTLE_CHAR_ROW_ANGLE},
        {-FF6_BATTLE_CHAR_ROW_ANGLE, -FF6_BATTLE_CHAR_ROW_ANGLE, -FF6_BATTLE_CHAR_ROW_ANGLE, -FF6_BATTLE_CHAR_ROW_ANGLE},
        {0, 0, 0, 0},
        {FF6_BATTLE_CHAR_ROW_ANGLE, FF6_BATTLE_CHAR_ROW_ANGLE, -FF6_BATTLE_CHAR_ROW_ANGLE, -FF6_BATTLE_CHAR_ROW_ANGLE}};
    const int type = std::clamp(battle_->battleType(), 0, FF6_BATTLE_TYPE_COUNT - 1);
    const int angle = kAngle[type][slot] + (c.backRow ? kRow[type][slot] : 0);
    const int reach = (std::abs(angle) * (kCharY[slot] + FF6_BATTLE_CHAR_APEX)) >> 8;
    return FF6_BATTLE_CHAR_CENTER_X + (angle < 0 ? -reach : reach) + keptShift_[slot].x;
}

// CharFlipTbl: the characters that face right, by the type of the battle: all of them in a back attack, every other one in a pincer
// attack and the two last in a side attack.
bool BattleScreen::facesRight(int slot) const {
    static constexpr bool kFlip[FF6_BATTLE_TYPE_COUNT][FF6_BATTLE_PARTY_SLOTS] = {
        {false, false, false, false}, {true, true, true, true}, {true, false, true, false}, {false, false, true, true}};
    return kFlip[std::clamp(battle_->battleType(), 0, FF6_BATTLE_TYPE_COUNT - 1)][slot];
}

namespace {

// The picture the other way round.
BattleImage turned(const BattleImage& image) {
    BattleImage result = image;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const uint8_t* from = &image.rgba[(static_cast<size_t>(y) * image.width + x) * FF6_BYTES_PER_PIXEL];
            std::copy(from, from + FF6_BYTES_PER_PIXEL, &result.rgba[(static_cast<size_t>(y) * image.width + image.width - 1 - x) * FF6_BYTES_PER_PIXEL]);
        }
    }
    return result;
}

// The picture upside down.
BattleImage upended(const BattleImage& image) {
    BattleImage result = image;
    const size_t stride = static_cast<size_t>(image.width) * FF6_BYTES_PER_PIXEL;
    for (int y = 0; y < image.height; ++y) {
        std::copy(&image.rgba[static_cast<size_t>(y) * stride], &image.rgba[static_cast<size_t>(y) * stride] + stride,
                  &result.rgba[static_cast<size_t>(image.height - 1 - y) * stride]);
    }
    return result;
}

// UpdateCharPal: petrify turns the palette to stone, poison, zombie and berserk change the skin, and the other statuses make the
// outline glow (the first one of the list: reflect, safe, shell, stop, haste, slow).
void statusColors(std::array<uint16_t, 16>& colors, uint32_t status, int frame) {
    static constexpr uint16_t kPetrify[11] = FF6_STATUS_PETRIFY_COLORS;
    static constexpr uint16_t kSkin[3][2] = FF6_STATUS_SKIN_COLORS;
    static constexpr uint16_t kOutline[8] = FF6_STATUS_OUTLINE_COLORS;
    static constexpr int kDelay[4] = FF6_STATUS_OUTLINE_DELAYS;
    if (status & FF6_ST_PETRIFY) {
        for (int i = 0; i < 11; ++i) colors[static_cast<size_t>(1 + i)] = kPetrify[i];
        return;
    }
    const int skin = (status & FF6_ST_POISON) ? 0 : (status & FF6_ST_ZOMBIE) ? 1 : (status & FF6_ST_BERSERK) ? 2 : -1;
    if (skin >= 0) {
        colors[6] = kSkin[skin][0];
        colors[7] = kSkin[skin][1];
        return;
    }
    int outline = -1;
    if (status & FF6_ST_REFLECT) outline = 0;
    else if (status & FF6_ST_SAFE) outline = 1;
    else if (status & FF6_ST_SHELL) outline = 2;
    else if (status & FF6_ST_STOP) outline = 7;
    else if (status & FF6_ST_HASTE) outline = 3;
    else if (status & FF6_ST_SLOW) outline = 4;
    if (outline < 0) return;
    const int t = (frame + kDelay[frame & 3]) & 0xFF;
    const int amount = (t & 0x40) ? 31 - ((t & 0x3C) >> 1) : (t & 0x3C) >> 1;  // how much is taken from the color
    const uint16_t base = kOutline[outline];
    const int red = std::max((base & 0x1F) - amount, 0), green = std::max(((base >> 5) & 0x1F) - amount, 0), blue = std::max(((base >> 10) & 0x1F) - amount, 0);
    colors[1] = static_cast<uint16_t>(red | (green << 5) | (blue << 10));
}

// UpdateStatusSpriteIndex: the sprite above a character, by the first status of the list.
int statusSpriteKind(uint32_t status) {
    if (status & FF6_ST_SLEEP) return 6;
    if (status & FF6_ST_CONFUSE) return 2;
    if (status & (FF6_ST_BERSERK | FF6_ST_RAGE)) return 4;
    if (status & FF6_ST_POISON) return 1;
    if (status & FF6_ST_BLIND) return 3;
    if (status & FF6_ST_SILENCE) return 5;
    return 0;
}

}  // namespace

const BattleImage& BattleScreen::monsterImage(size_t slot) const {
    const Combatant& c = battle_->combatants()[FF6_BATTLE_PARTY_SLOTS + slot];
    if (c.monster && c.gfx >= 0) {  // a character of a scene among the monsters, facing the party
        sceneImage_ = turned(gfx_->character(c.gfx, FF6_CHAR_POSE_FIGHT));
        return sceneImage_;
    }
    return gfx_->monster(formation_.monsters[slot]);
}

// The monsters at their places, as the formation gives them: the nibbles of the position are in units of 8 pixels.
void BattleScreen::drawField() {
    if (!gfx_) return;
    const bool scripted = player_ && !player_->done();  // an animation that moves, hides and colors what it reaches
    const int fieldScrollX = scripted ? player_->fieldScrollX() : 0, fieldScrollY = scripted ? player_->fieldScrollY() : 0;
    const auto shiftOf = [&](bool monster, int slot) {
        AnimShift shift = scripted ? player_->shift(monster, slot) : AnimShift();
        if (!monster && !scripted) shift.hidden = keptHidden_[slot];  // a scene took the character off the field
        if (!monster && !scripted && stepBack_ > 0) {  // STEP_BACK: it walks back from where the animation left it
            shift.x = stepFrom_[static_cast<size_t>(slot)].first * stepBack_ / FF6_BATTLE_STEP_BACK_FRAMES;
            shift.y = stepFrom_[static_cast<size_t>(slot)].second * stepBack_ / FF6_BATTLE_STEP_BACK_FRAMES;
        }
        return shift;
    };
    const auto tintOf = [&](int group) { return scripted ? player_->tint(group) : AnimTint(); };
    const uint8_t* lights = scripted ? player_->lightsPalette() : nullptr;  // the palettes that Chadarnook's lights loaded
    const BattleImage& field = gfx_->background(background_, BattleGfx::backgroundStep(background_, animation_), lights, lights ? 1 + player_->lights() : 0);
    const AnimTint fieldTint = tintOf(kTintField);
    const BattleImage lit = fieldTint.active && !field.rgba.empty() ? BattleImage{field.width, field.height, colored(field.rgba, fieldTint)} : BattleImage();
    const BattleImage& shownField = lit.rgba.empty() ? field : lit;
    const bool mode7 = mode7Shown();  // bg2 does not exist in mode 7
    const bool showField = !field.rgba.empty() && !mode7;
    if (showField && gfx_->wavy(background_)) {  // the heat sways the lines; two fields only move up and down
        const bool verticalOnly = background_ == FF6_BG_CYANS_DREAM || background_ == FF6_BG_TENTACLES;
        const int amplitude = background_ == FF6_BG_CYANS_DREAM ? FF6_BG_WAVE_AMPLITUDE_DREAM : FF6_BG_WAVE_AMPLITUDE;
        for (int y = 0; y < field.height; ++y) {
            const double phase = 2 * 3.14159265358979 * (static_cast<double>(y) / FF6_BG_WAVE_LINES + static_cast<double>(animation_ % FF6_BG_WAVE_FRAMES) / FF6_BG_WAVE_FRAMES);
            const int shift = static_cast<int>(std::lround(amplitude * std::sin(phase)));
            const int sourceY = std::clamp(y + shift, 0, field.height - 1);
            const int shiftX = verticalOnly ? 0 : shift;
            const uint8_t* row = shownField.rgba.data() + static_cast<size_t>(sourceY) * field.width * FF6_BYTES_PER_PIXEL;
            screen_.image(shiftX, y, field.width, 1, row);
            if (shiftX != 0) screen_.image(shiftX + (shiftX > 0 ? -field.width : field.width), y, field.width, 1, row);
        }
    } else if (showField && (scrollX_ != 0 || scrollY_ != 0 || bgShake_ != 0 || fieldScrollX != 0 || fieldScrollY != 0)) {  // the field wraps around as it scrolls
        static constexpr int kShake[17] = {-1, -3, -2, -3, -4, -2, -1, -3, -4, -2, -3, -1, -3, -2, -1, -2, -1};  // ScreenShakeTbl
        const int scrollX = scrollX_ + fieldScrollX + (bgShake_ ? kShake[animation_ & 15] : 0);
        const int scrollY = scrollY_ + fieldScrollY + (bgShake_ && bgShake_ != 0x80 ? kShake[(animation_ & 15) + 1] : 0);
        const int width = field.width, height = field.height;
        const int column = ((scrollX % width) + width) % width;
        for (int y = 0; y < height && y < FF6_BATTLE_FIELD_HEIGHT; ++y) {
            const int sourceY = ((y + scrollY) % height + height) % height;
            const uint8_t* row = shownField.rgba.data() + static_cast<size_t>(sourceY) * width * FF6_BYTES_PER_PIXEL;
            screen_.image(-column, y, width, 1, row);
            screen_.image(width - column, y, width, 1, row);
        }
    } else if (showField) {
        screen_.image(0, 0, field.width, field.height, shownField.rgba.data());
    }
    if (scripted && showField && (player_->mosaic() & 0x02) && (player_->mosaic() >> 4) > 0) {  // the mosaic of bg2: the field
        screen_.mosaic(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, (player_->mosaic() >> 4) + 1);
    }
    // The fixed color of each line is added to the backdrop (all of it in mode 7) and to bg2 (the field), or taken away.
    const int fixedMath = scripted ? player_->fixedMath() : 0;
    if (mode7 ? (fixedMath & 0x20) != 0 : (fixedMath & 0x02) != 0) {
        const int lines = mode7 ? FF6_BATTLE_MODE7_LINES : FF6_BATTLE_FIELD_HEIGHT;
        std::vector<uint8_t> gradient(static_cast<size_t>(FF6_SCREEN_WIDTH) * lines * FF6_BYTES_PER_PIXEL, 0);
        for (int y = 0; y < lines; ++y) {
            const AnimFixedColor& color = player_->fixedColor(y);
            for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
                uint8_t* pixel = &gradient[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + static_cast<size_t>(x)) * FF6_BYTES_PER_PIXEL];
                pixel[0] = static_cast<uint8_t>(color.r * 8);
                pixel[1] = static_cast<uint8_t>(color.g * 8);
                pixel[2] = static_cast<uint8_t>(color.b * 8);
                pixel[FF6_ALPHA_OFFSET] = 255;
            }
        }
        screen_.imageMath(0, 0, FF6_SCREEN_WIDTH, lines, gradient.data(), (fixedMath & 0x80) != 0, (fixedMath & 0x40) != 0);
    }
    // The scene in two pictures: the field, and what stands on it. The animations that add the subscreen need them apart.
    const size_t fieldBytes = static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_BATTLE_FIELD_HEIGHT * FF6_BYTES_PER_PIXEL;
    bgPixels_.assign(screen_.pixels().begin(), screen_.pixels().begin() + static_cast<std::ptrdiff_t>(fieldBytes));
    objectPixels_.assign(fieldBytes, 0);
    behindPixels_.assign(fieldBytes, 0);
    for (std::vector<uint8_t>& layer : layerPixels_) layer.assign(fieldBytes, 0);
    frontPixels_.assign(fieldBytes, 0);
    frontShown_ = false;
    bool frontDraw = false;  // the combatant being drawn is in front of bg1 (a script gave it the top priority)
    const int behindLimit = scripted ? player_->behindBg1Limit() : 0;
    const auto place = [&](int x, int y, int w, int h, const uint8_t* rgba, bool behind) {
    std::vector<uint8_t>& target = behind ? behindPixels_ : objectPixels_;
        for (int row = 0; row < h; ++row) {
            if (y + row < 0 || y + row >= FF6_BATTLE_FIELD_HEIGHT) continue;
            for (int column = 0; column < w; ++column) {
                if (x + column < 0 || x + column >= FF6_SCREEN_WIDTH) continue;
                const uint8_t* from = rgba + (static_cast<size_t>(row) * w + column) * FF6_BYTES_PER_PIXEL;
                if (from[FF6_ALPHA_OFFSET] == 0) continue;
                const size_t at = (static_cast<size_t>(y + row) * FF6_SCREEN_WIDTH + x + column) * FF6_BYTES_PER_PIXEL;
                std::copy(from, from + FF6_BYTES_PER_PIXEL, &target[at]);
                if (frontDraw && !behind) {
                    std::copy(from, from + FF6_BYTES_PER_PIXEL, &frontPixels_[at]);
                    frontShown_ = true;
                }
            }
        }
    };
    const AnimTint monsterTint = tintOf(kTintMonsters), characterTint = tintOf(kTintCharacters);
    for (size_t i = 0; i < formation_.monsters.size(); ++i) {
        const size_t slot = FF6_BATTLE_PARTY_SLOTS + i;
        const Combatant& monster = battle_->combatants()[slot];
        const AnimShift shift = shiftOf(true, static_cast<int>(i));
        frontDraw = scripted && shift.priority == FF6_ANIM_PRIORITY_FRONT;
        const bool blinkOff = blinkTimer_[slot] > 0 && (blinkTimer_[slot] / FF6_BATTLE_BLINK_PERIOD) % 2 == 0;
        bool shown = monster.alive() && !(monster.status & FF6_ST_HIDE) && !blinkOff;
        const bool entering = battle_->battleType() == FF6_BATTLE_TYPE_NORMAL && monster.placeX < 0 && introFrame_ < FF6_BATTLE_ENTRY_START + FF6_BATTLE_ENTRY_FRAMES;
        if (entering && introFrame_ < FF6_BATTLE_ENTRY_START) shown = false;  // the monsters come once the fade in is over
        if (dying_[slot]) shown = true;
        if (!shown || shift.hidden) continue;
        const BattleImage& stock = monsterImage(i);
        if (stock.rgba.empty()) continue;
        BattleImage swapped;  // a script gave the monster the colors of the animation
        if (shift.animPalette && anim_ && i < formation_.monsters.size()) {
            swapped = gfx_->monsterWith(formation_.monsters[i], anim_->paletteWords(player_->palette()));
        } else if (i == 0 && eventPalette_ >= 0 && anim_) {  // the first monster sparkles (the frozen esper of the first battle)
            swapped = gfx_->monsterWith(formation_.monsters[i], anim_->eventPaletteWords(eventPalette_));
        }
        const int flash = monsterFlash_[slot];  // before it acts: four frames black and white, four of its colors, twice
        if (flash > 0 && ((FF6_MONSTER_FLASH_FRAMES - flash) / FF6_MONSTER_FLASH_STEP) % 2 == 0 && monster.charIndex < 0) {
            swapped = gfx_->monsterFlash(formation_.monsters[i]);
        }
        const BattleImage& image = swapped.rgba.empty() || swapped.width != stock.width || swapped.height != stock.height ? stock : swapped;
        const BattleImage flipped = mirrored(i, image.width) != shift.flipX ? turned(image) : BattleImage();
        const BattleImage& sideways = mirrored(i, image.width) != shift.flipX ? flipped : image;
        const BattleImage overturned = shift.flipY ? upended(sideways) : BattleImage();
        const BattleImage& whole = shift.flipY ? overturned : sideways;
        const int monsterTop = this->monsterTop(i);
        static constexpr int kTremble[8] = FF6_KEFKA_TREMBLE;
        int tremble = scripted && i == 0 && player_->attack() == FF6_KEFKA_DEATH_ANIM ? kTremble[animation_ & 7] : 0;
        if (entering) {  // they slide in from the left side of the screen
            const int left = FF6_BATTLE_ENTRY_START + FF6_BATTLE_ENTRY_FRAMES - introFrame_;
            tremble -= (monsterX(i, image.width) + image.width - FF6_BATTLE_ENTRY_EDGE) * left / FF6_BATTLE_ENTRY_FRAMES;
        }
        if (dying_[slot] && fadeTimer_[slot] > 0 && monster.gfx < 0) {
            // A monster that falls turns to purple, in a palette that loses a step of each color at every frame, and its sprite is added
            // to the field (MonsterDeathAnim): it fades out while the field shows through.
            const BattleImage purple = gfx_->monsterFading(formation_.monsters[i], FF6_BATTLE_DEATH_FRAMES - fadeTimer_[slot]);
            if (purple.rgba.empty() || purple.width != image.width || purple.height != image.height) continue;
            const BattleImage purpleSide = mirrored(i, image.width) != shift.flipX ? turned(purple) : BattleImage();
            const BattleImage& purpleTurned = mirrored(i, image.width) != shift.flipX ? purpleSide : purple;
            const BattleImage purpleOver = shift.flipY ? upended(purpleTurned) : BattleImage();
            screen_.imageMath(monsterX(i, image.width) + shift.x + shift.vx + tremble, monsterTop + shift.y + shift.vy, image.width, image.height,
                              (shift.flipY ? purpleOver : purpleTurned).rgba.data(), false, false);
            continue;
        }
        const std::vector<uint8_t> lit = monsterTint.active ? colored(whole.rgba, monsterTint) : std::vector<uint8_t>();
        place(monsterX(i, image.width) + shift.x + shift.vx + tremble, monsterTop + shift.y + shift.vy, image.width, image.height,
              monsterTint.active ? lit.data() : whole.rgba.data(), behindLimit > 0 && monsterTop + image.height - 8 < behindLimit);
    }
    // The characters stand along a line that leans to the right, one below the other.
    if (!(scripted && player_->echo().on)) {
        for (EchoFrame& echo : echoHistory_) echo = EchoFrame();
    }
    // A battle in which anyone is in a magitek armor (wMagitekModeEnabled): all of the party are drawn riding one.
    const bool armored = magitekMode();
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        const Combatant& c = battle_->combatants()[static_cast<size_t>(i)];
        if (!c.present || (c.offstage && !sceneControlled_[i]) || (c.escaped && escapeTimer_[static_cast<size_t>(i)] == 0)) continue;
        const int hp = shownHp(i);
        int pose = FF6_CHAR_POSE_FIGHT;
        bool poseFlip = false;
        const AnimShift shift = shiftOf(false, i);
        frontDraw = scripted && shift.priority == FF6_ANIM_PRIORITY_FRONT;
        if (c.escaped) {
            pose = FF6_CHAR_POSE_RUN;
        } else if (hp == 0) {
            pose = FF6_CHAR_POSE_DEAD;
        } else if (battle_->outcome() == BattleOutcome::Won && resolved()) {
            pose = FF6_CHAR_POSE_VICTORY;
        } else if (poseTimer_[static_cast<size_t>(i)] > 0) {
            pose = poseValue_[static_cast<size_t>(i)];
        } else if (c.ready && !running_) {
            pose = FF6_CHAR_POSE_READY;
        } else if (keptPose_[i] >= 0) {  // where a scene left the character
            pose = keptPose_[i];
            poseFlip = keptFlip_[i];
        } else if (hp * FF6_NEAR_FATAL_DIVISOR < c.maxHp || (c.status & (FF6_ST_SLEEP | FF6_ST_POISON | FF6_ST_NEAR_FATAL))) {
            pose = FF6_CHAR_POSE_NEAR_FATAL;
        } else if (c.status & FF6_ST_CONTROL) {
            pose = FF6_CHAR_POSE_CAST;
        }
        int actor = c.gfx >= 0 ? c.gfx : characterGfx_[static_cast<size_t>(c.id)] >= 0 ? characterGfx_[static_cast<size_t>(c.id)] : characterSprite(c.id);
        if (gfxOverride_[i] >= 0) actor = gfxOverride_[i];
        const uint32_t status = c.status ^ shift.statusXor;  // the statuses, and the ones that a script toggled for the picture
        if (c.status & FF6_ST_MORPH) actor = FF6_CHAR_GFX_ESPER_TERRA;
        else if (((status & FF6_ST_IMP) != 0) != shift.imp) actor = FF6_CHAR_GFX_IMP;  // the status, or a script that toggles it
        const bool posed = hp > 0 && !c.escaped && shift.pose >= 0;
        const int drawnPose = posed ? shift.pose : pose;
        if (posed) poseFlip = shift.flip;
        std::array<uint16_t, 16> colors = gfx_->characterPalette(actor);
        if (shift.animPalette && anim_) {  // the colors of the animation, in place of the first 8
            const std::array<uint16_t, 8> words = anim_->paletteWords(player_->palette());
            for (size_t k = 1; k < words.size(); ++k) colors[k] = words[k];
        }
        const std::array<uint16_t, 16> original = gfx_->characterPalette(actor);
        if (hp > 0) statusColors(colors, status, animation_);
        const bool vanished = hp > 0 && ((status & FF6_ST_VANISH) != 0 || shift.vanish);  // only the outline is left
        const BattleImage recolored = colors != original || vanished ? gfx_->characterWith(actor, drawnPose, colors, vanished) : BattleImage();
        const BattleImage& image = recolored.rgba.empty() ? gfx_->character(actor, drawnPose) : recolored;
        if (image.rgba.empty()) continue;
        if (shift.hidden) continue;
        const int step = stepTimer_[static_cast<size_t>(i)] > 0 ? FF6_BATTLE_STEP_PIXELS : 0;
        const bool back = facesRight(i);
        const bool turnedPose = (back != poseFlip) != shift.turnAround;  // the pose faces the other way, or the whole battle does
        const BattleImage flipped = turnedPose ? turned(image) : BattleImage();
        const int run = c.escaped ? (FF6_BATTLE_ESCAPE_FRAMES - escapeTimer_[static_cast<size_t>(i)]) * FF6_BATTLE_ESCAPE_SPEED : 0;
        static constexpr int kFloat[8] = FF6_FLOAT_OFFSETS;
        const int lift = (status & FF6_ST_FLOAT) && !(status & FF6_ST_MAGITEK) && shift.pose < 0 && poseTimer_[static_cast<size_t>(i)] == 0
                             ? kFloat[(animation_ & 0x38) >> 3] : 0;
        // A character that runs along the line of the party, or goes to the other row, turns on the angle that the line leans by.
        static constexpr int kCharRowY[FF6_BATTLE_PARTY_SLOTS] = FF6_BATTLE_CHAR_Y;
        const int rowAngle = shift.rowAngle;
        const int walked = (shift.angle + rowAngle) * (kCharRowY[i] + FF6_BATTLE_CHAR_APEX) / 256;
        const int x = characterX(i) + (back ? step - run - walked : run - step + walked) + shift.x + shift.vx;
        const BattleImage upside = shift.flipY ? upended(turnedPose ? flipped : image) : BattleImage();  // turned over by a script
        const BattleImage& faced = shift.flipY ? upside : (turnedPose ? flipped : image);
        const std::vector<uint8_t> lit = characterTint.active ? colored(faced.rgba, characterTint) : std::vector<uint8_t>();
        const bool behindBg1 = behindLimit > 0 && characterTop(i) + FF6_CHAR_HEIGHT < behindLimit;
        const BattleImage& shadow = gfx_->statusSprite(FF6_STATUS_SPRITE_SHADOW, 0);  // under the feet, behind the character
        if (!shadow.rgba.empty() && !armored) place(x, characterTop(i) + FF6_CHAR_SPRITE_DY + FF6_CHAR_HEIGHT + shift.y, shadow.width, shadow.height, shadow.rgba.data(), behindBg1);
        const std::vector<uint8_t>& drawnPixels = characterTint.active ? lit : faced.rgba;
        const int drawnTop = characterTop(i) + FF6_CHAR_SPRITE_DY + shift.y + shift.vy - lift;  // CharSpriteData: the sprite starts 8 lines above PosY
        if (scripted && player_->echo().on && player_->echo().slot == i) {  // the echoes come first, behind the character
            if ((animation_ & player_->echo().mask) == 0) {
                for (size_t k = echoHistory_.size() - 1; k > 0; --k) echoHistory_[k] = echoHistory_[k - 1];
                echoHistory_[0] = {x, drawnTop, image.width, image.height, drawnPixels};
            }
            for (size_t k = static_cast<size_t>(animation_ & 1); k < echoHistory_.size(); k += 2) {
                const EchoFrame& echo = echoHistory_[k];
                if (!echo.rgba.empty()) place(echo.x, echo.y, echo.width, echo.height, echo.rgba.data(), behindBg1);
            }
        }
        place(x, drawnTop - (armored && hp > 0 ? FF6_MAGITEK_LIFT : 0), image.width, image.height, drawnPixels.data(), behindBg1);
        if (armored && hp > 0) {  // the armor covers the legs of the rider
            static constexpr int kWalk[4] = {0, 1, 0, 2};
            const BattleImage& armor = gfx_->magitekArmor(shift.armor == 2 ? 3 : shift.armor == 1 || step != 0 ? kWalk[(animation_ >> 2) & 3] : 0);
            if (!armor.rgba.empty()) {
                const BattleImage mirrored = back ? turned(armor) : BattleImage();
                place(back ? x + FF6_CHAR_WIDTH - FF6_MAGITEK_X - armor.width : x + FF6_MAGITEK_X, drawnTop + FF6_MAGITEK_Y, armor.width,
                      armor.height, (back ? mirrored : armor).rgba.data(), behindBg1);
            }
        }
        const int kind = hp > 0 && !armored ? statusSpriteKind(status) : 0;  // a sprite over the head: the bubbles, the dots, the zzz
        if (kind) {
            static constexpr int kX[7] = {0, 0, 0, -1, 0, -8, -16}, kY[7] = {0, -8, -8, -6, -8, -8, -8}, kKneelY[7] = {0, -3, -3, -1, -3, -3, -3};
            static constexpr int kFlipX[7] = {0, 0, 0, 2, 0, 16, 32};  // the sprite goes to the other side of a character that faces left
            const BattleImage& sprite = gfx_->statusSprite(kind, (animation_ >> 3) & 3);
            const BattleImage mirror = back ? turned(sprite) : BattleImage();
            const int y = characterTop(i) + 8 + shift.y - lift + (pose == FF6_CHAR_POSE_NEAR_FATAL ? kKneelY[kind] : kY[kind]);
            if (!sprite.rgba.empty()) place(x + kX[kind] + (back ? kFlipX[kind] : 0), y, sprite.width, sprite.height, (back ? mirror : sprite).rgba.data(), false);
        }
    }
    if (mode7) {  // the combatants are in front of bg1 or behind it, by their priority
        updatePlane();
        std::vector<uint8_t> picture = player_->bg1Shown() ? mode7Pixels() : std::vector<uint8_t>();
        const AnimTint& tint = player_->tint(kTintBg1);
        if (tint.active && !picture.empty()) picture = colored(picture, tint);
        const bool behind = player_->spritePriority() < 2;
        if (!behind && !picture.empty()) screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT, picture.data());
        screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, behindPixels_.data());
        screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, objectPixels_.data());
        if (behind && !picture.empty()) screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT, picture.data());
    } else {
        screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, behindPixels_.data());
        screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, objectPixels_.data());
    }
    if (flashTimer_ > 0) {
        screen_.tint(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, flashColor_[0], flashColor_[1], flashColor_[2],
                     flashTimer_ * FF6_BATTLE_FLASH_PERCENT);
    }
    drawAnimation();
    drawFx();
    drawPopup();
}

// The numbers of damage, as the sprites of the game draw them: over the middle of the combatant that took it, at its feet (a monster's
// 8 pixels above its bottom), and each one jumps and falls back by its own table.
void BattleScreen::drawPopup() {
    static constexpr int kBounce[FF6_BATTLE_NUMERAL_FRAMES] = FF6_BATTLE_NUMERAL_BOUNCE;
    if (!gfx_) return;
    for (const Numeral& numeral : numerals_) {
        int x = 0, y = 0;
        if (numeral.target < FF6_BATTLE_PARTY_SLOTS) {
            x = characterX(numeral.target) + FF6_CHAR_WIDTH / 2;
            y = characterTop(numeral.target) + FF6_CHAR_HEIGHT;
        } else {
            const size_t slot = static_cast<size_t>(numeral.target - FF6_BATTLE_PARTY_SLOTS);
            if (slot >= formation_.monsters.size()) continue;
            const BattleImage& image = monsterImage(slot);
            x = monsterX(slot, image.width) + image.width / 2;
            y = monsterTop(slot) + image.height - FF6_MENU_TILE_SIZE;
        }
        const BattleImage& glyph = gfx_->numeral(numeral.value, numeral.green);
        if (glyph.rgba.empty()) continue;
        y -= kBounce[std::min(numeral.frame, FF6_BATTLE_NUMERAL_FRAMES - 1)];
        if (y >= FF6_BATTLE_NUMERAL_HIDE_Y) continue;
        // The digits are centered on the spot; the word Miss starts from it, as the game places it.
        const int left = numeral.value < 0 ? x : x - glyph.width / 2;
        screen_.image(left, y + 1, glyph.width, glyph.height, glyph.rgba.data());  // a sprite shows one line under its y
    }
}

// UpdateMonsterGfxBuf and DrawMonsterNames: each name once, in the order of the places, four at most.
std::vector<int> BattleScreen::monsterNames() const {
    std::vector<int> firsts;
    for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {
        if (!monsterListed(i)) continue;
        bool known = false;
        for (const int first : firsts) known = known || nameOf(first) == nameOf(i);
        if (!known && static_cast<int>(firsts.size()) < FF6_BATTLE_MONSTER_NAMES) firsts.push_back(i);
    }
    return firsts;
}

void BattleScreen::drawMonsters() {
    screen_.windowPx(FF6_BATTLE_MONSTER_WINDOW_X, FF6_BATTLE_WINDOW_TOP, FF6_BATTLE_MONSTER_WINDOW_W, FF6_BATTLE_WINDOW_HEIGHT, FF6_BATTLE_GRADIENT_START, FF6_BATTLE_GRADIENT_LINES);
    const std::vector<int> names = monsterNames();
    for (size_t row = 0; row < names.size(); ++row) {
        putAt(FF6_BATTLE_MONSTER_NAME_PX, FF6_BATTLE_TEXT_TOP + static_cast<int>(row) * FF6_BATTLE_TEXT_PITCH, nameOf(names[row]), FF6_MENU_BG3_DEFAULT);
    }
}

// Text at a pixel of the screen.
void BattleScreen::putAt(int xPx, int yPx, const Codes& codes, int color) {
    screen_.setOriginX(xPx % FF6_MENU_TILE_SIZE);
    putPx(xPx / FF6_MENU_TILE_SIZE, yPx, codes, color);
    screen_.setOriginX(0);
}

void BattleScreen::putPx(int x, int yPx, const Codes& codes, int color) {
    screen_.setTextOffset(yPx % FF6_MENU_TILE_SIZE);
    put(x, yPx / FF6_MENU_TILE_SIZE, codes, color);
    screen_.setTextOffset(0);
}

int BattleScreen::monsterRowPx(int row) const { return FF6_BATTLE_TEXT_TOP + row * FF6_BATTLE_TEXT_PITCH; }

void BattleScreen::drawParty() {
    screen_.windowPx(FF6_BATTLE_PARTY_WINDOW_X, FF6_BATTLE_WINDOW_TOP, FF6_BATTLE_PARTY_WINDOW_W, FF6_BATTLE_WINDOW_HEIGHT, FF6_BATTLE_GRADIENT_START, FF6_BATTLE_GRADIENT_LINES);
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        const Combatant& c = battle_->combatants()[static_cast<size_t>(i)];
        if (!c.present || c.offstage) continue;
        const int y = FF6_BATTLE_TEXT_TOP + i * FF6_BATTLE_TEXT_PITCH;
        // _c14780: the names are white, but for the one of the character whose menu is open, in yellow.
        const bool menuOpen = mode_ != BattleMode::Fighting && mode_ != BattleMode::Result && mode_ != BattleMode::Done;
        const int color = menuOpen && i == active_ ? FF6_MENU_BG3_YELLOW : FF6_MENU_BG3_DEFAULT;
        putAt(FF6_BATTLE_CHAR_NAME_PX, y, battle_->hideNames() ? Codes() : text(characterName(c.id)), color);
        putAt(FF6_BATTLE_HP_PX, y, text(number(shownHp(i), 4)), FF6_MENU_BG3_DEFAULT);
        // DrawGaugeText: the high byte of the gauge, a pixel for every eight of it and one more, in four cells between the ends.
        int value = std::min(c.gauge >> FF6_BATTLE_GAUGE_SHIFT, FF6_BATTLE_GAUGE_VALUE_FULL);
        if (readyTimer_[static_cast<size_t>(i)] > 0) value = FF6_BATTLE_GAUGE_VALUE_FULL - 1;  // white for a while before it turns yellow
        if (i == acting_) value = FF6_BATTLE_GAUGE_VALUE_FULL;  // it stays full until the action is over
        const int pixels = (value >> FF6_BATTLE_GAUGE_STEP_SHIFT) + 1;
        Codes bar = {FF6_MENU_CODE_GAUGE_LEFT};
        for (int cell = 0; cell < FF6_BATTLE_GAUGE_TILES; ++cell) {
            bar.push_back(static_cast<uint8_t>(FF6_MENU_CODE_GAUGE_EMPTY + std::clamp(pixels - cell * FF6_BATTLE_GAUGE_TILE_STEPS, 0, FF6_BATTLE_GAUGE_TILE_STEPS)));
        }
        bar.push_back(FF6_MENU_CODE_GAUGE_RIGHT);
        putAt(FF6_BATTLE_GAUGE_PX, y, bar, value == FF6_BATTLE_GAUGE_VALUE_FULL ? FF6_BATTLE_GAUGE_FULL : FF6_BATTLE_GAUGE_FILLING);
    }
}

// The commands, or the spells or the items, in a window that takes the place of the ones at the bottom.
void BattleScreen::drawLists() {
    // The list stays while its target is chosen.
    const bool listShown = mode_ == BattleMode::Magic || mode_ == BattleMode::Item || mode_ == BattleMode::Tech || (mode_ == BattleMode::Target && targetFromList_);
    const bool rising = listShown && columns_ > 1 && listOpening_ > 0;  // the commands stay under the window that rises
    if (mode_ == BattleMode::Command || rising) {
        const std::vector<Entry>& entries = shownCommands();
        const int count = static_cast<int>(entries.size());
        const int shown = std::min(count, FF6_BATTLE_TEXT_ROWS);
        const int top = std::clamp(command_ - shown + 1, 0, count - shown);
        screen_.windowPx(FF6_BATTLE_CMD_WINDOW_X, FF6_BATTLE_WINDOW_TOP, FF6_BATTLE_CMD_WINDOW_W, FF6_BATTLE_WINDOW_HEIGHT, FF6_BATTLE_GRADIENT_START, FF6_BATTLE_GRADIENT_LINES);
        for (int row = 0; row < shown; ++row) {
            const Entry& entry = entries[static_cast<size_t>(top + row)];
            putAt(FF6_BATTLE_COMMAND_TEXT_PX, FF6_BATTLE_TEXT_TOP + row * FF6_BATTLE_TEXT_PITCH, entry.name,
                  entry.enabled ? FF6_MENU_BG3_DEFAULT : FF6_MENU_BG3_GRAY);
        }
        if (!rising) screen_.hand(FF6_BATTLE_COMMAND_HAND_PX, FF6_BATTLE_TEXT_TOP + (command_ - top) * FF6_BATTLE_TEXT_PITCH + FF6_ITEM_CURSOR_DY);
    }
    if (listShown && columns_ > 1) {
        // The window rises from the bottom, with its text; its lines are cut where the windows end.
        const int travel = FF6_BATTLE_LIST_WINDOW_BOTTOM - FF6_BATTLE_LIST_WINDOW_TOP;
        const int top = FF6_BATTLE_LIST_WINDOW_TOP + travel * listOpening_ / FF6_BATTLE_LIST_OPEN_FRAMES;
        screen_.windowPx(FF6_BATTLE_LIST_WINDOW_X, top, FF6_BATTLE_LIST_WINDOW_W, FF6_BATTLE_LIST_WINDOW_BOTTOM - FF6_BATTLE_LIST_WINDOW_TOP,
                         FF6_BATTLE_GRADIENT_START, FF6_BATTLE_GRADIENT_LINES);
        static constexpr int kColumns[FF6_BATTLE_MAGITEK_COLUMNS] = FF6_BATTLE_LIST_COLUMN_PX;
        for (size_t index = 0; index < list_.size(); ++index) {
            const Entry& entry = list_[index];
            const int y = top + (FF6_BATTLE_LIST_TEXT_TOP - FF6_BATTLE_LIST_WINDOW_TOP) + static_cast<int>(index) / columns_ * FF6_BATTLE_TEXT_PITCH;
            if (entry.value < 0 || y + FF6_MENU_TILE_SIZE > FF6_BATTLE_LIST_WINDOW_BOTTOM) continue;
            putAt(kColumns[index % columns_], y, entry.name, entry.enabled ? FF6_MENU_BG3_DEFAULT : FF6_MENU_BG3_GRAY);
        }
        if (listOpening_ == 0 && mode_ != BattleMode::Target) {
            screen_.hand(kColumns[listCursor_ % columns_] - FF6_BATTLE_LIST_HAND_DX,
                         FF6_BATTLE_LIST_TEXT_TOP + listCursor_ / columns_ * FF6_BATTLE_TEXT_PITCH + FF6_ITEM_CURSOR_DY);
        }
    } else if (listShown && mode_ != BattleMode::Command) {
        const int width = mode_ == BattleMode::Item ? FF6_BATTLE_ITEM_W : FF6_BATTLE_LIST_W;
        screen_.window(FF6_BATTLE_LIST_X, FF6_BATTLE_LIST_Y, width, FF6_BATTLE_WINDOW_ROWS);
        for (int row = 0; row < FF6_BATTLE_LIST_ROWS; ++row) {
            const size_t index = static_cast<size_t>(listTop_ + row);
            if (index >= list_.size()) break;
            const Entry& entry = list_[index];
            const int y = FF6_BATTLE_ROWS_TOP_PX + row * FF6_BATTLE_ROW_PX;
            const int color = entry.enabled ? FF6_MENU_BG3_DEFAULT : FF6_MENU_BG3_GRAY;
            putPx(FF6_BATTLE_LIST_TEXT_X, y, entry.name, color);
            if (entry.amount >= 0) putPx(FF6_BATTLE_LIST_X + width - 2, y, text(number(entry.amount, 2)), color);
        }
        if (mode_ != BattleMode::Target) screen_.hand(FF6_BATTLE_HAND_X, FF6_BATTLE_ROWS_TOP_PX + (listCursor_ - listTop_) * FF6_BATTLE_ROW_PX + FF6_ITEM_CURSOR_DY);
    }
}

// UpdateTargetCursors: the hands on the combatants that the cursor covers; on a monster at its right side, turned to the left,
// half way down; on a character at its left.
void BattleScreen::drawHands() {
    if (mode_ != BattleMode::Target) return;
    for (const int target : selection()) {
        if (target < FF6_BATTLE_PARTY_SLOTS) {
            screen_.hand(characterX(target) - FF6_BATTLE_CHAR_HAND_DX, characterTop(target) + FF6_BATTLE_CHAR_HAND_DY);
            continue;
        }
        if (!gfx_) continue;
        const size_t slot = static_cast<size_t>(target - FF6_BATTLE_PARTY_SLOTS);
        const BattleImage& image = monsterImage(slot);
        const bool flip = !mirrored(slot, image.width);
        const int x = flip ? monsterX(slot, image.width) + image.width - FF6_BATTLE_MONSTER_HAND_DX : monsterX(slot, image.width) - FF6_BATTLE_CHAR_HAND_DX + FF6_BATTLE_MONSTER_HAND_DX;
        screen_.hand(x, monsterTop(slot) + image.height / 2, flip);
    }
}

void BattleScreen::drawResult() {
    screen_.window(FF6_BATTLE_RESULT_X, FF6_BATTLE_RESULT_Y, FF6_BATTLE_RESULT_W, FF6_BATTLE_RESULT_H);
    const int x = FF6_BATTLE_RESULT_X + 2;
    int y = FF6_BATTLE_RESULT_Y + 1;
    const BattleReward& reward = battle_->reward();
    put(x, y, text("Victory!"), FF6_MENU_BG3_YELLOW);
    y += FF6_BATTLE_ROW_TILES;
    put(x, y, join({text("Exp: "), text(std::to_string(reward.exp))}), FF6_MENU_BG3_DEFAULT);
    y += FF6_BATTLE_ROW_TILES;
    put(x, y, join({text("Gil: "), text(std::to_string(reward.gil))}), FF6_MENU_BG3_DEFAULT);
    for (const int item : reward.items) {
        y += FF6_BATTLE_ROW_TILES;
        if (y >= FF6_BATTLE_RESULT_Y + FF6_BATTLE_RESULT_H) break;
        const uint8_t* name = screen_.data().itemNames.data() + static_cast<size_t>(item) * FF6_ITEM_NAME_BYTES;
        put(x, y, Codes(name, name + FF6_ITEM_NAME_BYTES), FF6_MENU_BG3_TEAL);
    }
}

void BattleScreen::draw() {
    struct FontPalette {  // the text of the battle has its own palettes (BattleFontPal), until the drawing is over
        MenuScreen& screen;
        ~FontPalette() { screen.setBg3Palette(nullptr); }
    } fontPalette{screen_};
    screen_.setBg3Palette(gfx_ ? gfx_->fontPal.data() : nullptr);
    screen_.clear();
    screen_.setTextOffset(0);
    screen_.setClip(0, FF6_SCREEN_HEIGHT);
    if (!battle_) return;
    if (mode_ == BattleMode::Result && battle_->outcome() == BattleOutcome::Lost) {  // nothing is left of the battle
        put(FF6_BATTLE_GAME_OVER_X, FF6_BATTLE_GAME_OVER_Y, text("GAME OVER"), FF6_MENU_BG3_DEFAULT);
        put(FF6_BATTLE_GAME_OVER_X - 4, FF6_BATTLE_GAME_OVER_Y + 3, text("The party was defeated."), FF6_MENU_BG3_GRAY);
        return;
    }
    drawField();
    // The mode of the game is changed by an HDMA table: mode 7 is only for the lines of the field, and the windows are below.
    // The window on top (bg1, InitMsgWindowHDMA): the name of an attack in a box in the middle, the other messages across.
    if (!current_.empty() && !mode7Shown() && (!currentName_ || nameTimer_ > 0)) {
        const int x = currentName_ ? FF6_BATTLE_NAME_BOX_X : FF6_BATTLE_MESSAGE_BOX_X;
        const int w = currentName_ ? FF6_BATTLE_NAME_BOX_W : FF6_BATTLE_MESSAGE_BOX_W;
        screen_.windowPx(x, FF6_BATTLE_MESSAGE_BOX_Y, w, FF6_BATTLE_MESSAGE_BOX_H, FF6_BATTLE_MESSAGE_GRADIENT_START, FF6_BATTLE_MESSAGE_GRADIENT_LINES);
        const int length = static_cast<int>(current_.size()) * FF6_MENU_TILE_SIZE;
        if (currentName_ && currentLarge_) {  // the name of the special attack of a monster, in the middle (_c19887)
            screen_.largeText(x + w / 2, FF6_BATTLE_MESSAGE_LARGE_Y, current_, true);
        } else if (currentName_) {  // the small font of the menus
            putAt(x + (w - length) / 2, FF6_BATTLE_MESSAGE_TEXT_Y, current_, FF6_MENU_BG3_DEFAULT);
        } else {  // DrawLargeText: the font of the dialogues
            screen_.largeText(FF6_BATTLE_MESSAGE_LARGE_X, FF6_BATTLE_MESSAGE_LARGE_Y, current_);
        }
    }
    // DrawCursorSprites: the arrow over the character whose menu is open (higher in a magitek armor).
    const bool menuOpen = mode_ != BattleMode::Fighting && mode_ != BattleMode::Result && mode_ != BattleMode::Done && active_ >= 0;
    if (menuOpen && gfx_) {
        const BattleImage arrow = gfx_->statusSprite(FF6_BATTLE_ARROW_TILE, screen_.data().cursorPalette.data());
        const int y = characterTop(active_) + FF6_BATTLE_ARROW_DY + (magitekMode() ? FF6_BATTLE_ARROW_MAGITEK_DY : 0);
        screen_.image(characterX(active_), y, arrow.width, arrow.height, arrow.rgba.data());
    }
    drawMonsters();  // the window of the commands is drawn over it
    drawParty();
    drawLists();
    drawHands();
    if (mode_ == BattleMode::Slot) drawSlot();
    if (mode_ == BattleMode::Result) drawResult();
    if (player_ && !player_->done() && player_->dimmed() > 0) {  // the brightness of the whole screen, a sixteenth less at each step
        screen_.tint(0, 0, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT, 0, 0, 0, std::min(player_->dimmed(), FF6_ANIM_BRIGHTNESS_STEPS) * 100 / FF6_ANIM_BRIGHTNESS_STEPS);
    }
}

}  // namespace ff6
