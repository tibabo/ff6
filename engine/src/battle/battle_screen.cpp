#include "battle/battle_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "field/map.hpp"

namespace ff6 {
namespace {

using Layer = MenuScreen::Layer;

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
    const std::vector<int>& characters, int battle, uint32_t seed, int battleSpeed, bool wait, int background) {
    data_ = &data;
    rules_ = &rules;
    party_ = &party;
    background_ = background;
    formation_ = data.formation(battle);
    battle_.emplace(data, rules, party, characters, battle, seed, battleSpeed);
    mode_ = BattleMode::Fighting;
    commands_.clear();
    list_.clear();
    group_ = false;
    poseTimer_.fill(0);
    stepTimer_.fill(0);
    blinkTimer_.fill(0);
    fadeTimer_.fill(0);
    escapeTimer_.fill(0);
    dying_.fill(false);
    fx_.clear();
    lastElement_ = 0;
    flashTimer_ = 0;
    messages_.clear();
    current_.clear();
    messageTimer_ = 0;
    popupTarget_ = popupDamage_ = -1;
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
    static constexpr int kCharY[FF6_BATTLE_PARTY_SLOTS] = FF6_BATTLE_CHAR_Y;
    if (combatant < 0 || combatant >= FF6_BATTLE_COMBATANTS) return false;
    if (combatant < FF6_BATTLE_PARTY_SLOTS) {
        x = characterX(combatant) + FF6_CHAR_WIDTH / 2;
        y = kCharY[combatant] + FF6_CHAR_HEIGHT / 2;
        return true;
    }
    const size_t slot = static_cast<size_t>(combatant - FF6_BATTLE_PARTY_SLOTS);
    if (!gfx_) return false;
    const BattleImage& image = gfx_->monster(formation_.monsters[slot]);
    x = monsterX(slot, image.width) + image.width / 2;
    y = (formation_.positions[slot] & 0x0F) * FF6_BATTLE_MONSTER_Y_UNIT + image.height / 2;
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
    const bool flipped = attacker.monster != (type == FF6_BATTLE_TYPE_BACK);
    player_->start(message.attack, attacker, targets, flipped,
                   type == FF6_BATTLE_TYPE_BACK ? 1 : type == FF6_BATTLE_TYPE_PINCER ? 2 : 0);
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
    const bool summon = attack >= FF6_FIRST_GENJU_ATTACK && attack < FF6_FIRST_NAMED_ATTACK;  // a character calls it on the monsters
    message.actor = summon ? 0 : FF6_BATTLE_PARTY_SLOTS;
    if (summon) {
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
    const bool flipped = attacker.monster != (battle_->battleType() == FF6_BATTLE_TYPE_BACK);
    player_->startWeapon(anim_->weapon(message.attack), attacker, target, flipped, message.leftHand, false);
    animated_ = !player_->done();
}

bool BattleScreen::body(int combatant, AnimBody& out) const {
    static constexpr int kCharY[FF6_BATTLE_PARTY_SLOTS] = FF6_BATTLE_CHAR_Y;
    if (combatant < 0 || combatant >= FF6_BATTLE_COMBATANTS) return false;
    int x = 0, y = 0, width = FF6_CHAR_WIDTH, height = FF6_CHAR_HEIGHT;
    if (combatant < FF6_BATTLE_PARTY_SLOTS) {
        out.monster = false;
        out.slot = combatant;
        x = characterX(combatant);
        y = kCharY[combatant];
    } else {
        if (!gfx_) return false;
        const size_t slot = static_cast<size_t>(combatant - FF6_BATTLE_PARTY_SLOTS);
        const BattleImage& image = gfx_->monster(formation_.monsters[slot]);
        out.monster = true;
        out.slot = static_cast<int>(slot);
        width = image.width;
        height = image.height;
        x = monsterX(slot, width);
        y = (formation_.positions[slot] & 0x0F) * FF6_BATTLE_MONSTER_Y_UNIT;
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
    for (int pass = 0; pass < 4; ++pass) {
        const int layer = justice ? kPassesUnder[pass] : kPasses[pass];
        const bool last = pass == 3;
        if (mode7 && layer > 0) continue;  // bg1 is the plane of mode 7, and there is no bg3
        if (justice && pass == 3 && !mode7) {  // what is in front of bg1 goes over it again
            screen_.image(0, 0, FF6_SCREEN_WIDTH, FF6_BATTLE_FIELD_HEIGHT, objectPixels_.data());
        }
        const AnimTint& tint = player_->tint(layer == 0 ? kTintSprites : layer == 1 ? kTintBg1 : kTintBg3);
        const AnimBlend blend = player_->blend(layer);
        // A layer of background is first drawn apart. If it adds the subscreen, where it has a pixel the pixel replaces the one
        // of the scene, plus what the subscreen (its layers, in priority) shows there; a half addition halves only where the
        // subscreen has a pixel, as the hardware does.
        const bool added = layer > 0 && layerPixels_[layer].size() == bgPixels_.size();
        if (added && !last) std::fill(layerPixels_[layer].begin(), layerPixels_[layer].end(), 0);
        for (const AnimSprite& sprite : player_->sprites()) {
            if (last || sprite.layer != layer) continue;
            const AnimEsperSheet& sheet = player_->esperSheet();
            const int order = sprite.esper >= 0 ? sprite.esper : sheet.esper;
            const BattleImage* esper = gfx_ && (sprite.esper >= 0 || sheet.layer == sprite.layer) ? &gfx_->monster(FF6_ESPER_GFX_FIRST + order) : nullptr;
            const AttackFrame& frame = anim_->frame(sprite.script, sprite.frame, esper ? FF6_ESPER_SHEET_PALETTE + order : sprite.palette,
                                                    sprite.flipped, player_->cycle(layer), esper);
            if (frame.image.rgba.empty()) continue;
            const std::vector<uint8_t> pixels = tint.active ? colored(frame.image.rgba, tint) : std::vector<uint8_t>();
            const uint8_t* source = tint.active ? pixels.data() : frame.image.rgba.data();
            const AnimWave wave = layer > 0 ? player_->wave(layer) : AnimWave();
            const auto paint = [&](int x, int y, int w, int h, const uint8_t* rgba) {
                if (layer == 0 && justice) h = std::min(h, FF6_JUSTICE_WINDOW_LINES - y);
                if (h <= 0) return;
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
                left = ((left % width) + width) % width;
                origins = {left - width, left};
            }
            const bool windowed = player_->layerWindowed(layer);
            for (const int top : tops) {
            if (!wave.active && !windowed) {
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
                    const int line = std::clamp(top + row, 0, FF6_BATTLE_FIELD_HEIGHT - 1) * 32 / FF6_BATTLE_FIELD_HEIGHT;
                    const int shiftX = wave.horizontal ? swing(wave.hAmplitude, wave.hShown, line) : 0;
                    const int from = std::clamp(row + (wave.vertical ? swing(wave.vAmplitude, wave.vShown, line) : 0), 0, height - 1);
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
        else blinkTimer_[static_cast<size_t>(who)] = FF6_BATTLE_BLINK_FRAMES;
    };
    static constexpr int kColors[8][3] = FF6_BATTLE_ELEMENT_COLORS;
    static constexpr int kWhite[3] = {255, 255, 255}, kGreen[3] = {96, 255, 96};
    const auto elementColor = [&](int element) {
        int bit = 0;
        while (bit < 7 && !(element & (1 << bit))) ++bit;
        return element ? kColors[bit] : kWhite;
    };
    switch (message.effect) {
        case Message::Effect::Cast:
            lastElement_ = message.element;
            if (message.actor < FF6_BATTLE_PARTY_SLOTS) pose(message.actor, FF6_CHAR_POSE_CAST, FF6_BATTLE_POSE_FRAMES * 2, 0);
            startAnimation(message);
            break;
        case Message::Effect::Strike:
            animated_ = false;
            if (message.actor < FF6_BATTLE_PARTY_SLOTS) pose(message.actor, FF6_CHAR_POSE_ATTACK, FF6_BATTLE_POSE_FRAMES / 2, FF6_BATTLE_POSE_FRAMES / 2);
            hurt(message.subject);
            startBlow(message);
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
            if (message.subject >= FF6_BATTLE_PARTY_SLOTS) fadeTimer_[static_cast<size_t>(message.subject)] = FF6_BATTLE_BLINK_FRAMES;
            break;
        case Message::Effect::Escape:
            if (message.actor >= 0 && message.actor < FF6_BATTLE_PARTY_SLOTS) escapeTimer_[static_cast<size_t>(message.actor)] = FF6_BATTLE_ESCAPE_FRAMES;
            break;
        case Message::Effect::None: break;
    }
    if (message.flash) {
        int bit = 0;
        while (bit < 7 && !(message.element & (1 << bit))) ++bit;
        const bool plain = message.element == 0;
        for (int channel = 0; channel < 3; ++channel) flashColor_[channel] = plain ? 255 : kColors[bit][channel];
        flashTimer_ = FF6_BATTLE_FLASH_FRAMES;
    }
}

void BattleScreen::tickEffects() {
    ++animation_;
    if (player_ && !player_->done()) {
        player_->tick();
        for (const int sfx : player_->takeSounds()) sound_.command(FF6_SOUND_GAME_SFX, static_cast<uint8_t>(sfx), FF6_SFX_PAN_CENTER);
    }
    // The esper that has a bg3 thread (Raiden's lightning) comes when that thread is over.
    const bool waiting = esperAfterBolts_ && player_ && !player_->done() && player_->layerActive(2);
    if (esperGfx_ >= 0 && !waiting) {  // the esper comes, stays while its animation plays, and goes
        ++esperAge_;
        const bool staying = (player_ && !player_->done()) || esperAge_ < FF6_ESPER_FRAMES;
        esperShown_ = staying ? std::min(100, esperShown_ + FF6_ESPER_FADE_PERCENT) : esperShown_ - FF6_ESPER_FADE_PERCENT;
        esperLeaving_ = !staying;
        if (!staying && esperShown_ <= 0) esperGfx_ = -1;
    }
    for (Fx& fx : fx_) ++fx.frame;
    fx_.erase(std::remove_if(fx_.begin(), fx_.end(), [](const Fx& fx) { return fx.frame >= fx.length; }), fx_.end());
    for (size_t i = 0; i < poseTimer_.size(); ++i) {
        if (poseTimer_[i] > 0) --poseTimer_[i];
        if (stepTimer_[i] > 0) --stepTimer_[i];
        if (blinkTimer_[i] > 0) --blinkTimer_[i];
        if (fadeTimer_[i] > 0 && --fadeTimer_[i] == 0) dying_[i] = false;
        if (escapeTimer_[i] > 0) --escapeTimer_[i];
    }
    if (flashTimer_ > 0) --flashTimer_;
}

void BattleScreen::collectEvents() {
    const auto points = [&](int target, long amount, const char* verb, const char* unit, bool harm) {
        queueWith(join({nameOf(target), text(verb), text(std::to_string(amount)), text(unit)}), target, static_cast<int>(amount),
                  static_cast<int>(harm ? Message::Effect::Hurt : Message::Effect::Heal), -1, target, 0, false);
    };
    const std::vector<BattleEvent> events = battle_->takeEvents();
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
                const Codes prefix = critical ? text("Critical! ") : Codes();
                queueWith(join({prefix, nameOf(event.attacker), text(" hits "), nameOf(event.target), text(" for "),
                                text(std::to_string(event.damage))}),
                          event.target, event.damage, static_cast<int>(Message::Effect::Strike), event.attacker, event.target, 0, critical);
                // The weapon in the hand that struck, or bare hands.
                int weapon = FF6_ATTACK_WEAPON_FISTS;
                const Combatant& attacker = battle_->combatants()[static_cast<size_t>(event.attacker)];
                if (!attacker.monster) {
                    const int item = party_->characters[static_cast<size_t>(attacker.id)].equipment[static_cast<size_t>(event.hand)] & 0xFF;
                    if (item < FF6_ATTACK_WEAPON_FISTS) weapon = item;
                }
                if (event.item >= 0 && event.item < FF6_ATTACK_WEAPON_FISTS) weapon = event.item;  // a weapon that is thrown
                messages_.back().attack = weapon;
                messages_.back().leftHand = event.hand != 0;
                break;
            }
            case BattleEvent::Kind::Miss:
                queue(join({nameOf(event.attacker), text(" missed "), nameOf(event.target)}));
                break;
            case BattleEvent::Kind::Defeated:
                if (event.target >= FF6_BATTLE_PARTY_SLOTS) dying_[static_cast<size_t>(event.target)] = true;
                queueWith(join({nameOf(event.target), text(" is defeated")}), -1, -1, static_cast<int>(Message::Effect::Fade), -1, event.target, 0, false);
                break;
            case BattleEvent::Kind::Escaped:
                queueWith(join({nameOf(event.attacker), text(" ran away")}), -1, -1, static_cast<int>(Message::Effect::Escape), event.attacker, -1, 0, false);
                break;
            case BattleEvent::Kind::Cast: {
                Codes name = event.attack < FF6_SPELL_COUNT ? screen_.data().spellName(event.attack) : data_->attackName(event.attack);
                if (event.attack >= FF6_ATTACK_FIRST_BUSHIDO && event.attack < FF6_ATTACK_FIRST_BUSHIDO + FF6_TECH_ATTACKS) {
                    name = screen_.data().bushidoName(event.attack - FF6_ATTACK_FIRST_BUSHIDO);
                } else if (event.attack >= FF6_ATTACK_FIRST_BLITZ && event.attack < FF6_ATTACK_FIRST_BLITZ + FF6_TECH_ATTACKS) {
                    name = screen_.data().blitzName(event.attack - FF6_ATTACK_FIRST_BLITZ);
                }
                name.erase(std::find(name.begin(), name.end(), FF6_BATTLE_NAME_PAD_CODE), name.end());
                const int element = rules_->attack(event.attack)[FF6_ATTACK_ELEMENT];
                if (name.empty()) break;
                queueWith(name, -1, -1, static_cast<int>(Message::Effect::Cast), event.attacker, -1, element, true);
                // The animation plays on the combatants that the attack goes on to reach.
                messages_.back().attack = event.attack;
                reached(index);
                break;
            }
            case BattleEvent::Kind::UseItem: {
                const uint8_t* name = screen_.data().itemNames.data() + static_cast<size_t>(event.item) * FF6_ITEM_NAME_BYTES;
                if (event.attack < 0) {
                    queue(Codes(name, name + FF6_ITEM_NAME_BYTES));
                    break;
                }
                queueWith(Codes(name, name + FF6_ITEM_NAME_BYTES), -1, -1, static_cast<int>(Message::Effect::Cast), event.attacker, -1, 0, false);
                messages_.back().attack = event.attack;  // a tool: the animation of its attack
                reached(index);
                break;
            }
            case BattleEvent::Kind::Defend:
                queueWith(join({nameOf(event.attacker), text(" defends")}), -1, -1, static_cast<int>(Message::Effect::Defend), event.attacker, -1, 0, false);
                break;
            case BattleEvent::Kind::Damage:
            case BattleEvent::Kind::Poison:
            case BattleEvent::Kind::Sap: points(event.target, event.damage, " loses ", " HP", true); break;
            case BattleEvent::Kind::Heal:
            case BattleEvent::Kind::Regen: points(event.target, event.damage, " recovers ", " HP", false); break;
            case BattleEvent::Kind::MpDamage: points(event.target, event.damage, " loses ", " MP", true); break;
            case BattleEvent::Kind::MpHeal: points(event.target, event.damage, " recovers ", " MP", false); break;
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
            case BattleEvent::Kind::Reflected: queue(join({nameOf(event.target), text(" reflects")})); break;
        }
    }
}

// The message stays a while, or until the button is pressed; the next one replaces it.
void BattleScreen::advanceMessage(bool skip) {
    if (animating()) {  // the damage waits for the end of the animation, unless the button cuts it short
        if (!skip) return;
        player_->stop();
    }
    if (messageTimer_ > 0 && !skip && --messageTimer_ > 0) return;
    messageTimer_ = 0;
    current_.clear();
    popupTarget_ = popupDamage_ = -1;
    if (messages_.empty()) {
        animated_ = false;
        return;
    }
    current_ = messages_.front().text;
    trigger(messages_.front());
    popupTarget_ = messages_.front().target;
    popupDamage_ = messages_.front().damage;
    messages_.pop_front();
    messageTimer_ = animating() ? 1 : FF6_BATTLE_MESSAGE_FRAMES;
}

void BattleScreen::finish() {
    if (battle_->outcome() != BattleOutcome::Fled) {
        mode_ = BattleMode::Result;
    } else if (!finishing_) {
        finishing_ = true;
        queue(text("Escaped safely!"));
    } else {
        mode_ = BattleMode::Done;
    }
}

void BattleScreen::update(const InputState& input) {
    if (!battle_ || mode_ == BattleMode::Done) return;
    tickEffects();
    const int direction = repeat_.update(input.direction);
    if (!messages_.empty() || messageTimer_ > 0) {
        advanceMessage(input.confirm);
        return;
    }
    switch (mode_) {
        case BattleMode::Fighting: updateFighting(input); break;
        case BattleMode::Command: updateCommand(input, direction); break;
        case BattleMode::Magic:
        case BattleMode::Item:
        case BattleMode::Tech: updateList(input, direction); break;
        case BattleMode::Target: updateTarget(input, direction); break;
        case BattleMode::Result:
            if (input.confirm) {
                sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
                mode_ = BattleMode::Done;
            }
            break;
        case BattleMode::Done: break;
    }
}

void BattleScreen::updateFighting(const InputState& input) {
    if (battle_->outcome() != BattleOutcome::Fighting) return finish();
    if (running_ && input.cancel) {  // the buttons are let go
        running_ = false;
        battle_->setRunning(false);
    }
    if (++frames_ % FF6_BATTLE_TICK_FRAMES == 0) {
        battle_->tick();
        collectEvents();
    }
    if (!messages_.empty() || battle_->outcome() != BattleOutcome::Fighting || running_) return;
    const int ready = battle_->readyCharacter();
    if (ready >= 0) {
        active_ = ready;
        openCommands();
    }
}

// With the active mode the gauges go on while a command is chosen; the menu closes if its character cannot act any more.
bool BattleScreen::keepsMenu() {
    if (!wait_ && ++frames_ % FF6_BATTLE_TICK_FRAMES == 0) {
        battle_->tick();
        collectEvents();
    }
    const Combatant& character = battle_->combatants()[static_cast<size_t>(active_)];
    if (battle_->outcome() == BattleOutcome::Fighting && character.alive() && character.ready) return true;
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
        const int id = actor[FF6_CHAR_PROP_COMMANDS + slot] == FF6_CMD_MORPH && (c.status & FF6_ST_MORPH) ? FF6_CMD_REVERT
                                                                                                    : actor[FF6_CHAR_PROP_COMMANDS + slot];
        if (id >= FF6_CMD_COUNT || !kCommandNames[id][0] || id == FF6_CMD_DEFEND || id == FF6_CMD_ROW) continue;
        Entry entry;
        entry.name = text(kCommandNames[id]);
        entry.value = id;
        entry.enabled = id == FF6_CMD_FIGHT || id == FF6_CMD_ITEM || id == FF6_CMD_DEFEND || id == FF6_CMD_STEAL ||
                        id == FF6_CMD_BUSHIDO || id == FF6_CMD_BLITZ || (id == FF6_CMD_MAGIC && battle_->canCast(active_)) ||
                        (id == FF6_CMD_X_MAGIC && battle_->canCast(active_)) || id == FF6_CMD_SHOCK || id == FF6_CMD_HEALTH ||
                        (id == FF6_CMD_TOOLS && hasTools()) || (id == FF6_CMD_LORE && party_->lores != 0) || id == FF6_CMD_MORPH ||
                        id == FF6_CMD_REVERT || id == FF6_CMD_RUNIC || id == FF6_CMD_JUMP || id == FF6_CMD_MAGITEK || id == FF6_CMD_SKETCH || id == FF6_CMD_CONTROL || id == FF6_CMD_CAPTURE || id == FF6_CMD_POSSESS || id == FF6_CMD_GP_RAIN ||
                        (id == FF6_CMD_THROW && hasWeapons()) || (id == FF6_CMD_MIMIC && battle_->canMimic());
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
    if (direction == Up || direction == Down) {
        command_ = (command_ + (direction == Down ? 1 : count - 1)) % count;
        sound_.command(FF6_SFX_MENU_MOVE, 0, 0);
    }
    if (!input.confirm) return;
    const Entry& entry = shown[static_cast<size_t>(command_)];
    if (!entry.enabled) return;
    sound_.command(FF6_SFX_MENU_SELECT, 0, 0);
    controlList_ = false;
    switch (entry.value) {
        case FF6_CMD_FIGHT: {
            Attack fight;
            fight.target = FF6_TARGET_MANUAL | FF6_TARGET_ONE_SIDE | FF6_TARGET_ENEMY;
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
        case FF6_CMD_MIMIC:
            battle_->mimic(active_);
            collectEvents();
            mode_ = BattleMode::Fighting;
            break;
        case FF6_CMD_MORPH:
        case FF6_CMD_REVERT:
        case FF6_CMD_RUNIC:
            if (entry.value == FF6_CMD_MORPH) battle_->morph(active_);
            else if (entry.value == FF6_CMD_REVERT) battle_->revert(active_);
            else battle_->runic(active_);
            collectEvents();
            mode_ = BattleMode::Fighting;
            break;
        case FF6_CMD_STEAL: {
            Attack steal;
            steal.target = FF6_TARGET_MANUAL | FF6_TARGET_ONE_SIDE | FF6_TARGET_ENEMY;
            beginTarget(Pending::Steal, 0, steal);
            break;
        }
        case FF6_CMD_ROW:
            battle_->changeRow(active_);
            collectEvents();
            mode_ = BattleMode::Fighting;
            break;
        case FF6_CMD_ITEM: openList(BattleMode::Item); break;
        case FF6_CMD_TOOLS: openList(BattleMode::Item, true); break;
        case FF6_CMD_DEFEND:
            battle_->defend(active_);
            collectEvents();
            mode_ = BattleMode::Fighting;
            break;
        default:
            running_ = true;
            battle_->setRunning(true);
            mode_ = BattleMode::Fighting;
            break;
    }
}

void BattleScreen::updateList(const InputState& input, int direction) {
    if (!keepsMenu()) return;
    const int count = static_cast<int>(list_.size());
    if (count > 0 && (direction == Up || direction == Down)) {
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

void BattleScreen::openMagitek() {
    list_.clear();
    const Combatant& c = battle_->combatants()[static_cast<size_t>(active_)];
    for (int i = 0; i < FF6_MAGITEK_ATTACKS; ++i) addAttack(c, FF6_ATTACK_FIRST_MAGITEK + i);
    listMode_ = BattleMode::Tech;
    listCursor_ = listTop_ = 0;
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
    switch (pending_) {
        case Pending::Fight: battle_->fight(active_, targets.front()); break;
        case Pending::Spell: battle_->cast(active_, pendingValue_, targets, xMagic_ == 1); break;
        case Pending::Item: battle_->useItem(active_, pendingValue_, targets); break;
        case Pending::Steal: battle_->steal(active_, targets.front()); break;
        case Pending::Throw: battle_->throwWeapon(active_, pendingValue_, targets.front()); break;
        case Pending::Jump: battle_->jump(active_, targets.front()); break;
        case Pending::Sketch: battle_->sketch(active_, targets.front()); break;
        case Pending::Capture: battle_->capture(active_, targets.front()); break;
        case Pending::Possess: battle_->possess(active_, targets.front()); break;
        case Pending::GpRain: battle_->gpRain(active_, targets); break;
        case Pending::Control:
            if (battle_->control(active_, targets.front())) {  // it obeys: the attack that it is given is next
                collectEvents();
                openControl(targets.front());
                return;
            }
            break;
        case Pending::ControlAttack: {
            uint16_t mask = 0;
            for (const int target : targets) mask = static_cast<uint16_t>(mask | (1u << target));
            battle_->controlAttack(active_, pendingValue_, mask);
            break;
        }
    }
    collectEvents();
    if (xMagic_ == 1) {  // the second spell of X Magic
        xMagic_ = 2;
        openList(BattleMode::Magic);
        return;
    }
    xMagic_ = 0;
    mode_ = BattleMode::Fighting;
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

// A back attack turns the whole battle round; in a pincer attack every other monster is on the other side.
bool BattleScreen::mirrored(size_t monster) const {
    const int type = battle_->battleType();
    return type == FF6_BATTLE_TYPE_BACK || (type == FF6_BATTLE_TYPE_PINCER && monster % 2 == 1);
}

int BattleScreen::monsterX(size_t monster, int width) const {
    const int x = (formation_.positions[monster] >> 4) * FF6_BATTLE_MONSTER_X_UNIT;
    return mirrored(monster) ? FF6_SCREEN_WIDTH - x - width : x;
}

int BattleScreen::characterX(int slot) const {
    static constexpr int kCharY[FF6_BATTLE_PARTY_SLOTS] = FF6_BATTLE_CHAR_Y;
    const int angle = FF6_BATTLE_CHAR_ANGLE + (battle_->combatants()[static_cast<size_t>(slot)].backRow ? FF6_BATTLE_CHAR_ROW_ANGLE : 0);
    const int x = (angle * (kCharY[slot] + FF6_BATTLE_CHAR_APEX) >> 8) + FF6_BATTLE_CHAR_CENTER_X;
    return battle_->battleType() == FF6_BATTLE_TYPE_BACK ? FF6_SCREEN_WIDTH - x - FF6_CHAR_WIDTH : x;
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

// A monster that falls melts away: the pixels go one by one, in an order of their own.
BattleImage dissolved(const BattleImage& image, int percent) {
    BattleImage result = image;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const unsigned hash = (static_cast<unsigned>(x) * 73856093u) ^ (static_cast<unsigned>(y) * 19349663u);
            if (static_cast<int>(hash % 100) < percent) result.rgba[(static_cast<size_t>(y) * image.width + x) * FF6_BYTES_PER_PIXEL + FF6_ALPHA_OFFSET] = 0;
        }
    }
    return result;
}

}  // namespace

// The monsters at their places, as the formation gives them: the nibbles of the position are in units of 8 pixels.
void BattleScreen::drawField() {
    if (!gfx_) return;
    const bool scripted = player_ && !player_->done();  // an animation that moves, hides and colors what it reaches
    const auto shiftOf = [&](bool monster, int slot) { return scripted ? player_->shift(monster, slot) : AnimShift(); };
    const auto tintOf = [&](int group) { return scripted ? player_->tint(group) : AnimTint(); };
    const BattleImage& field = gfx_->background(background_);
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
    } else if (showField) {
        screen_.image(0, 0, field.width, field.height, shownField.rgba.data());
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
    const int behindLimit = scripted ? player_->behindBg1Limit() : 0;
    const auto place = [&](int x, int y, int w, int h, const uint8_t* rgba, bool behind) {
    std::vector<uint8_t>& target = behind ? behindPixels_ : objectPixels_;
        for (int row = 0; row < h; ++row) {
            if (y + row < 0 || y + row >= FF6_BATTLE_FIELD_HEIGHT) continue;
            for (int column = 0; column < w; ++column) {
                if (x + column < 0 || x + column >= FF6_SCREEN_WIDTH) continue;
                const uint8_t* from = rgba + (static_cast<size_t>(row) * w + column) * FF6_BYTES_PER_PIXEL;
                if (from[FF6_ALPHA_OFFSET] == 0) continue;
                std::copy(from, from + FF6_BYTES_PER_PIXEL, &target[(static_cast<size_t>(y + row) * FF6_SCREEN_WIDTH + x + column) * FF6_BYTES_PER_PIXEL]);
            }
        }
    };
    const AnimTint monsterTint = tintOf(kTintMonsters), characterTint = tintOf(kTintCharacters);
    for (size_t i = 0; i < formation_.monsters.size(); ++i) {
        const size_t slot = FF6_BATTLE_PARTY_SLOTS + i;
        const Combatant& monster = battle_->combatants()[slot];
        const AnimShift shift = shiftOf(true, static_cast<int>(i));
        const bool blinkOff = blinkTimer_[slot] > 0 && (blinkTimer_[slot] / FF6_BATTLE_BLINK_PERIOD) % 2 == 0;
        bool shown = monster.alive() && !(monster.status & FF6_ST_HIDE) && !blinkOff;
        if (dying_[slot]) shown = true;
        if (!shown || shift.hidden) continue;
        const BattleImage& image = gfx_->monster(formation_.monsters[i]);
        if (image.rgba.empty()) continue;
        const BattleImage flipped = mirrored(i) ? turned(image) : BattleImage();
        const BattleImage& whole = mirrored(i) ? flipped : image;
        const BattleImage melting = dying_[slot] && fadeTimer_[slot] > 0
                                        ? dissolved(whole, 100 - fadeTimer_[slot] * 100 / FF6_BATTLE_BLINK_FRAMES)
                                        : BattleImage();
        const BattleImage& drawn = melting.rgba.empty() ? whole : melting;
        const std::vector<uint8_t> lit = monsterTint.active ? colored(drawn.rgba, monsterTint) : std::vector<uint8_t>();
        const int monsterTop = (formation_.positions[i] & 0x0F) * FF6_BATTLE_MONSTER_Y_UNIT;
        place(monsterX(i, image.width) + shift.x, monsterTop + shift.y, image.width, image.height,
              monsterTint.active ? lit.data() : drawn.rgba.data(), behindLimit > 0 && monsterTop + image.height - 8 < behindLimit);
    }
    // The characters stand along a line that leans to the right, one below the other.
    static constexpr int kCharY[FF6_BATTLE_PARTY_SLOTS] = FF6_BATTLE_CHAR_Y;
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        const Combatant& c = battle_->combatants()[static_cast<size_t>(i)];
        if (!c.present || (c.escaped && escapeTimer_[static_cast<size_t>(i)] == 0)) continue;
        int pose = FF6_CHAR_POSE_FIGHT;
        const AnimShift shift = shiftOf(false, i);
        if (c.escaped) {
            pose = FF6_CHAR_POSE_RUN;
        } else if (c.hp == 0) {
            pose = FF6_CHAR_POSE_DEAD;
        } else if (battle_->outcome() == BattleOutcome::Won) {
            pose = FF6_CHAR_POSE_VICTORY;
        } else if (poseTimer_[static_cast<size_t>(i)] > 0) {
            pose = poseValue_[static_cast<size_t>(i)];
        } else if (c.ready && !running_) {
            pose = FF6_CHAR_POSE_READY;
        } else if (c.hp * FF6_NEAR_FATAL_DIVISOR < c.maxHp) {
            pose = FF6_CHAR_POSE_NEAR_FATAL;
        }
        const BattleImage& image = gfx_->character(c.id, c.hp > 0 && !c.escaped && shift.pose >= 0 ? shift.pose : pose);
        if (image.rgba.empty()) continue;
        if (shift.hidden) continue;
        const int step = stepTimer_[static_cast<size_t>(i)] > 0 ? FF6_BATTLE_STEP_PIXELS : 0;
        const bool back = battle_->battleType() == FF6_BATTLE_TYPE_BACK;
        const BattleImage flipped = back ? turned(image) : BattleImage();
        const int run = c.escaped ? (FF6_BATTLE_ESCAPE_FRAMES - escapeTimer_[static_cast<size_t>(i)]) * FF6_BATTLE_ESCAPE_SPEED : 0;
        const std::vector<uint8_t> lit = characterTint.active ? colored((back ? flipped : image).rgba, characterTint) : std::vector<uint8_t>();
        place(characterX(i) + (back ? step - run : run - step) + shift.x, kCharY[i] + shift.y, image.width, image.height,
              characterTint.active ? lit.data() : (back ? flipped : image).rgba.data(), behindLimit > 0 && kCharY[i] + FF6_CHAR_HEIGHT < behindLimit);
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

// The damage of the message that is shown, above the combatant that took it.
void BattleScreen::drawPopup() {
    if (popupTarget_ < 0 || popupDamage_ < 0) return;
    static constexpr int kCharY[FF6_BATTLE_PARTY_SLOTS] = FF6_BATTLE_CHAR_Y;
    int x = 0, y = 0;
    if (popupTarget_ < FF6_BATTLE_PARTY_SLOTS) {
        y = kCharY[popupTarget_];
        x = characterX(popupTarget_);
    } else {
        const size_t slot = static_cast<size_t>(popupTarget_ - FF6_BATTLE_PARTY_SLOTS);
        const BattleImage& image = gfx_->monster(formation_.monsters[slot]);
        x = monsterX(slot, image.width) + image.width / 2;
        y = (formation_.positions[slot] & 0x0F) * FF6_BATTLE_MONSTER_Y_UNIT;
    }
    screen_.text(std::max(0, x / FF6_MENU_TILE_SIZE - 1), std::max(0, y / FF6_MENU_TILE_SIZE - 1), std::to_string(popupDamage_),
                 Layer::Bg3, FF6_MENU_BG3_YELLOW);
}

void BattleScreen::drawMonsters() {
    screen_.window(0, FF6_BATTLE_MONSTERS_Y, FF6_BATTLE_MONSTERS_W, FF6_BATTLE_WINDOW_ROWS);
    int row = 0;
    for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {
        if (!battle_->combatants()[static_cast<size_t>(i)].alive()) continue;
        putPx(FF6_BATTLE_NAME_X, monsterRowPx(row), nameOf(i), FF6_MENU_BG3_DEFAULT);
        ++row;
    }
}

void BattleScreen::putPx(int x, int yPx, const Codes& codes, int color) {
    screen_.setTextOffset(yPx % FF6_MENU_TILE_SIZE);
    put(x, yPx / FF6_MENU_TILE_SIZE, codes, color);
    screen_.setTextOffset(0);
}

int BattleScreen::monsterRowPx(int row) const {
    int alive = 0;
    for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) alive += battle_->combatants()[static_cast<size_t>(i)].alive();
    const int pitch = alive > FF6_BATTLE_MONSTER_ROWS ? FF6_BATTLE_MONSTER_ROW_PX : FF6_BATTLE_ROW_PX;
    return FF6_BATTLE_ROWS_TOP_PX + row * pitch;
}

void BattleScreen::drawParty() {
    screen_.window(FF6_BATTLE_PARTY_X, FF6_BATTLE_MONSTERS_Y, FF6_BATTLE_PARTY_W, FF6_BATTLE_WINDOW_ROWS);
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        const Combatant& c = battle_->combatants()[static_cast<size_t>(i)];
        if (!c.present) continue;
        const int y = FF6_BATTLE_ROWS_TOP_PX + i * FF6_BATTLE_ROW_PX;
        // The name tells the state: gray when down, yellow when ready, teal with a status.
        int color = FF6_MENU_BG3_DEFAULT;
        if (c.hp == 0 || c.escaped) {
            color = FF6_MENU_BG3_GRAY;
        } else if (c.ready) {
            color = FF6_MENU_BG3_YELLOW;
        } else if (c.status & ~(FF6_ST_DEAD | FF6_ST_NEAR_FATAL)) {
            color = FF6_MENU_BG3_TEAL;
        }
        putPx(FF6_BATTLE_CHAR_NAME_X, y, text(characterName(c.id)), color);
        putPx(FF6_BATTLE_HP_X, y, text(number(c.hp, 4)), FF6_MENU_BG3_DEFAULT);
        // The gauge: a frame with cells that fill four steps each.
        const int total = FF6_BATTLE_GAUGE_CELLS * FF6_BATTLE_GAUGE_STEPS;
        const int steps = std::min(c.gauge * total / FF6_ATB_FULL, total);
        Codes bar = {FF6_MENU_CODE_GAUGE_LEFT};
        for (int cell = 0; cell < FF6_BATTLE_GAUGE_CELLS; ++cell) {
            const int filled = steps - cell * FF6_BATTLE_GAUGE_STEPS;
            bar.push_back(filled >= FF6_BATTLE_GAUGE_STEPS ? FF6_MENU_CODE_GAUGE_FULL
                          : filled > 0 ? static_cast<uint8_t>(FF6_MENU_CODE_GAUGE_EMPTY + 2 * filled)
                                       : FF6_MENU_CODE_GAUGE_EMPTY);
        }
        bar.push_back(FF6_MENU_CODE_GAUGE_RIGHT);
        putPx(FF6_BATTLE_GAUGE_X, y, bar, FF6_MENU_BG3_DEFAULT);
    }
}

// The commands, or the spells or the items, in a window that takes the place of the ones at the bottom.
void BattleScreen::drawLists() {
    const bool listShown = mode_ == BattleMode::Magic || mode_ == BattleMode::Item || mode_ == BattleMode::Tech;
    if (mode_ == BattleMode::Command) {
        const std::vector<Entry>& entries = shownCommands();
        const int count = static_cast<int>(entries.size());
        const int shown = std::min(count, FF6_BATTLE_LIST_ROWS);
        const int top = std::clamp(command_ - shown + 1, 0, count - shown);
        screen_.window(FF6_BATTLE_COMMAND_X, FF6_BATTLE_COMMAND_Y, FF6_BATTLE_COMMAND_W, FF6_BATTLE_WINDOW_ROWS);
        for (int row = 0; row < shown; ++row) {
            const Entry& entry = entries[static_cast<size_t>(top + row)];
            putPx(FF6_BATTLE_COMMAND_X + FF6_BATTLE_LIST_TEXT_X, FF6_BATTLE_ROWS_TOP_PX + row * FF6_BATTLE_ROW_PX, entry.name,
                  entry.enabled ? FF6_MENU_BG3_DEFAULT : FF6_MENU_BG3_GRAY);
        }
        screen_.hand(FF6_BATTLE_COMMAND_HAND_X, FF6_BATTLE_ROWS_TOP_PX + (command_ - top) * FF6_BATTLE_ROW_PX + FF6_ITEM_CURSOR_DY);
    } else if (listShown) {
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
        screen_.hand(FF6_BATTLE_HAND_X, FF6_BATTLE_ROWS_TOP_PX + (listCursor_ - listTop_) * FF6_BATTLE_ROW_PX + FF6_ITEM_CURSOR_DY);
    }
}

// The hands over the combatants that the cursor of the target covers.
void BattleScreen::drawHands() {
    if (mode_ != BattleMode::Target) return;
    for (const int target : selection()) {
        if (target < FF6_BATTLE_PARTY_SLOTS) {
            screen_.hand(FF6_BATTLE_PARTY_HAND_X, FF6_BATTLE_ROWS_TOP_PX + target * FF6_BATTLE_ROW_PX + FF6_ITEM_CURSOR_DY);
            continue;
        }
        int row = 0;
        for (int i = FF6_BATTLE_PARTY_SLOTS; i < target; ++i) row += battle_->combatants()[static_cast<size_t>(i)].alive();
        screen_.hand(FF6_BATTLE_HAND_X, monsterRowPx(row) + FF6_ITEM_CURSOR_DY);
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
    if (!current_.empty() && !mode7Shown()) {
        screen_.window(FF6_BATTLE_MESSAGE_X, FF6_BATTLE_MESSAGE_Y, FF6_BATTLE_MESSAGE_W, 2);
        put(FF6_BATTLE_MESSAGE_X + 1, FF6_BATTLE_MESSAGE_Y + 1, current_, FF6_MENU_BG3_DEFAULT);
    }
    drawMonsters();  // the window of the commands is drawn over it
    drawParty();
    drawLists();
    drawHands();
    if (mode_ == BattleMode::Result) drawResult();
}

}  // namespace ff6
