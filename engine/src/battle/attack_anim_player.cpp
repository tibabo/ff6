#include "battle/attack_anim_player.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "battle/attack_anim_inits.hpp"

namespace ff6 {

namespace {

constexpr int kEnd = 0xFF;
constexpr int kBlankFrame = 0x1F;
constexpr int kFrameMask = 0x7F;
constexpr int kMaxFrames = 900;  // the longest animation is far shorter; a script that never ends is cut here
constexpr int kScreenX = 128, kScreenY = 80;  // the center of the battle field, for the animations aligned with the screen
constexpr int kFrontNear = 24, kFrontFar = 32, kFrontBody = 16;
constexpr int kMaxX = 252;
constexpr int kHeaderBytes = 2;
constexpr int kMoveDirections = 8;

// The copies of the thread of each target that an init function makes, and the frames that go by between them.
struct Copies {
    int type, delay, copies;
};
constexpr Copies kCopies[] = {
    {0x7D, 5, 2}, {0x7A, 4, 7}, {0x78, 3, 3}, {0x75, 3, 2}, {0x56, 7, 2}, {0x53, 2, 5}, {0x4D, 16, 2}, {0x4C, 6, 7},
    {0x4A, 1, 7}, {0x47, 3, 3}, {0x1E, 8, 5}, {0x20, 8, 5}, {0x31, 0, 2}, {0x1A, 8, 5}, {0x15, 8, 5}, {0x30, 0, 7},
    {0x22, 0, 5}, {0x2E, 8, 5}, {0x21, 8, 5}, {0x14, 6, 5}, {0x1C, 0, 3}, {0x18, 3, 5}, {0x25, 8, 2}, {0x17, 40, 1}, {0x5E, 4, 1},
};

int word(const uint8_t* code, size_t at) { return code[at] | (code[at + 1] << 8); }

// The init functions of the attacks that show mode 7 (the esper ones put its picture from a row of tiles of the plane).
struct PlaneInit {
    int type;
    AnimPlane::Kind kind;
    int tileSet, mapSet, esper, row;
    bool crusader;
};
constexpr PlaneInit kPlanes[] = {
    {0x63, AnimPlane::kTiles, 1, 0, 0, 0, false},  // S. Cross
    {0x64, AnimPlane::kTiles, 0, 1, 0, 0, false},  // Overcast
    {0x75, AnimPlane::kTiles, 2, 2, 0, 0, false},  // H-Bomb
    {0x45, AnimPlane::kEsper, 0, 0, FF6_ESPER_ODIN, 1, false},  // Atom Edge
    {0x6B, AnimPlane::kEsper, 0, 0, FF6_ESPER_RAIDEN, 1, false},  // True Edge
    {0x74, AnimPlane::kEsper, 0, 0, FF6_ESPER_CRUSADER, 6, true},  // Purifier
};
// The init functions that load the picture of an esper as the sheet of a layer (0 sprites, 1 bg1): its order among the espers.
struct EsperSheetInit {
    int type, layer, esper;
};
constexpr EsperSheetInit kEsperSheets[] = {
    {0x33, 0, 1}, {0x34, 0, 5}, {0x35, 0, 4}, {0x36, 0, 7}, {0x37, 1, 18}, {0x38, 0, 0}, {0x39, 0, 2}, {0x3A, 1, 3}, {0x3B, 1, 14},
    {0x3C, 0, 19}, {0x3D, 0, 17}, {0x3E, 0, 23}, {0x3F, 0, 26}, {0x40, 1, 21}, {0x41, 0, 25}, {0x42, 1, 13}, {0x43, 0, 6}, {0x44, 1, 9},
    {0x46, 1, 24}, {0x68, 1, 8}, {0x69, 1, 22}, {0x6A, 1, 16}, {0x6C, 0, 10}, {0x6D, 1, 20}, {0x6E, 0, 27},
};
constexpr int kMainLayersAll = 0x17;  // bg1, bg2, bg3 and the sprites
constexpr int kMode7Unit = 0x100;  // 1.0 in the matrix

int signedByte(int value) { return value >= 0x80 ? value - 0x100 : value; }
int wrap16(int value) { return static_cast<int16_t>(static_cast<uint16_t>(value)); }

// The commands of palette: the group of colors, whether they subtract, and whether the value is a change of the last one.
struct PaletteOp {
    int op, group;
    bool subtract, relative;
};
constexpr PaletteOp kPaletteOps[] = {
    {0xAA, kTintBg3, true, false},        {0xAB, kTintBg3, false, false},        {0xB3, kTintBg3, false, true},
    {0xB4, kTintBg3, true, true},         {0xAF, kTintField, true, false},       {0xB0, kTintField, false, false},
    {0xB5, kTintField, false, true},      {0xB6, kTintField, true, true},        {0xCC, kTintBg1, true, false},
    {0xCD, kTintBg1, false, false},       {0xCE, kTintBg1, false, true},         {0xCF, kTintBg1, true, true},
    {0xB1, kTintSprites, true, false},    {0xB2, kTintSprites, false, false},    {0xB7, kTintSprites, false, true},
    {0xB8, kTintSprites, true, true},     {0xB9, kTintMonsters, true, false},    {0xBA, kTintMonsters, false, false},
    {0xBB, kTintMonsters, false, true},   {0xBC, kTintMonsters, true, true},     {0xFB, kTintCharacters, true, false},
    {0xFC, kTintCharacters, false, false}, {0xFD, kTintCharacters, false, true}, {0xFE, kTintCharacters, true, true},
};
constexpr int kTintMax = 31;

// The step of a move command, dddxxxxx: forward is to the left unless the thread faces the other way.
void stepOf(int argument, bool flipped, int& dx, int& dy) {
    static constexpr int kDx[kMoveDirections] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static constexpr int kDy[kMoveDirections] = {1, 1, 1, 0, 0, -1, -1, -1};
    const int distance = (argument & 0x1F) + 1, direction = (argument >> 5) & (kMoveDirections - 1);
    dx = (flipped ? -kDx[direction] : kDx[direction]) * distance;
    dy = kDy[direction] * distance;
}

// The bytes of a command, its code included; the jumps and the end are handled by the commands themselves.
int commandLength(int op) {
    switch (op) {
        case 0x85: case 0x8A: case 0x8C: case 0x91: case 0x94: case 0x95: case 0x9A: case 0xA7: case 0xA8: case 0xC0: case 0xC3:
        case 0xD3: case 0xDC: case 0xDE: case 0xDF: case 0xE4: case 0xE7: case 0xF2: case 0xF5: case 0xF6:
            return 1;
        case 0x81: case 0x82: case 0x92: case 0x96: case 0x98: case 0x9D: case 0x9E: case 0xA0: case 0xA1: case 0xA3: case 0xA4:
        case 0xA9: case 0xAC: case 0xBF: case 0xC1: case 0xC6: case 0xD2: case 0xD6: case 0xDA: case 0xE2: case 0xE3: case 0xE8:
        case 0xE9: case 0xED: case 0xEF: case 0xFA:
            return 3;
        case 0xA6: case 0xD4: case 0xD8: case 0xE5: case 0xE6: case 0xF9: return 4;
        case 0xDD: case 0xE0: case 0xF8: return 5;
        case 0xA5: return 8;
        case 0xC5: return 9;
        case 0xF0: case 0xF3: return 11;
        default: return 2;
    }
}

// The bytes of a command of the family $80, from its code.
int specialLength(int sub) {
    switch (sub) {
        case 0x1A: case 0x26: case 0x27: case 0x28: case 0x29: case 0x2B: case 0x2C: case 0x31: case 0x33: case 0x39: case 0x3A:
        case 0x3E: case 0x40: case 0x42: case 0x43: case 0x45: case 0x46: case 0x47: case 0x4B: case 0x50: case 0x51: case 0x55:
        case 0x57: case 0x58: case 0x59: case 0x5F: case 0x63: case 0x72: case 0x73: case 0x76: case 0x7D: case 0x82: case 0x85:
        case 0x86: case 0x87: case 0x89: case 0x8C:
            return 3;
        case 0x2E: return 4;
        case 0x61: case 0x62: case 0x75: case 0x80: return 5;
        case 0x32: case 0x41: case 0x60: return 6;
        case 0x2D: return 8;
        default: return 2;
    }
}

}  // namespace

std::vector<int> AttackAnimPlayer::takeSounds() {
    std::vector<int> sounds;
    sounds.swap(sounds_);
    return sounds;
}

void AttackAnimPlayer::skip(int code) {
    for (auto& entry : skipped_) {
        if (entry.first == code) {
            ++entry.second;
            return;
        }
    }
    skipped_.push_back({code, 1});
}

// Where a thread starts for a combatant, by the alignment in the header of its script.
void AttackAnimPlayer::place(const AnimBody& body, bool isTarget, int align, int halfHeight, bool flipped, int& x, int& y) const {
    const int side = flipped ? -1 : 1;
    if (!body.monster) {
        switch (align) {
            case 0x00: x = body.centerX; y = body.bottomY + 8 - halfHeight; return;
            case 0x40: x = body.centerX; y = body.bottomY - 8; return;
            case 0x80: case 0xC0:
                x = body.centerX;
                y = body.bottomY;
                if (isTarget) x = std::clamp(body.centerX + side * kFrontNear, 0, kMaxX);
                return;
            case 0xA0:
                if (isTarget) {
                    x = kScreenX;
                    y = kScreenY;
                } else {
                    x = body.centerX;
                    y = body.bottomY;
                }
                return;
            case 0xE0: x = body.centerX - 8; y = body.centerY - 8; return;
            default: x = body.centerX; y = body.centerY; return;
        }
    }
    switch (align) {
        case 0x00: x = body.centerX; y = body.bottomY + 8 - halfHeight; return;
        case 0x40: x = body.centerX; y = body.topY; return;
        case 0x80: x = body.centerX + side * (body.width / 2 + kFrontBody); y = body.bottomY; return;
        case 0xC0: x = body.centerX + side * (body.width / 2 + kFrontFar); y = body.bottomY; return;
        case 0xA0: x = kScreenX; y = kScreenY; return;
        default: x = body.centerX; y = body.centerY; return;
    }
}

void AttackAnimPlayer::begin(const AnimBody& attacker, bool flipped) {
    attacker_ = attacker;
    flipped_ = flipped;
    frames_ = 0;
    threads_.clear();
    sprites_.clear();
    sounds_.clear();
    skipped_.clear();
    hidden_[0] = hidden_[1] = hidden_[2] = false;
    for (AnimTint& tint : tints_) tint = AnimTint();
    for (AnimWave& wave : waves_) wave = AnimWave();
    for (AnimBlend& blend : blends_) blend = AnimBlend();
    for (AnimCycle& turn : cycles_) turn = AnimCycle();
    circle_ = Circle();
    w12_ = w34_ = -1;
    subscreen_ = 0;
    shape_ = kCircle;
    circleAngle_ = 0;
    for (int line = 0; line < 256; ++line) {
        spanLeft_[line] = 255;
        spanRight_[line] = 0;
    }
    for (AnimShift& shift : shifts_) shift = AnimShift();
    mode7_ = AnimMode7();
    plane_ = AnimPlane();
    esperSheet_ = AnimEsperSheet();
    mainLayers_ = kMainLayersAll;
    spritePriority_ = 3;
    bg3High_ = true;
    rotateScale_ = 0xFF;
    bg1Base_[0] = bg1Base_[1] = bg1Cur_[0] = bg1Cur_[1] = 0;
    bg1Shift_[0] = bg1Shift_[1] = bg3Shift_[0] = bg3Shift_[1] = 0;
    behindLimit_ = 0;
    veilBg3_ = false;
    for (AnimFixedColor& line : fixed_) line = AnimFixedColor();
    fixedMath_ = gradientLevel_ = gradientCounter_ = 0;
    miss_ = false;
    done_ = true;
    for (int layer = 0; layer < 3; ++layer) {
        sheet_[layer] = -1;
        palettes_[layer] = 0;
    }
    sprite_ = -1;
    palette_ = 0;
}

// A thread of a layer: it follows the script, aligned with a combatant as the header of the script says.
void AttackAnimPlayer::addThread(int layer, int script, int palette, const AnimBody& target, int counter, int index) {
    if (script < 0) return;
    size_t length = 0;
    const uint8_t* code = anim_->script(script, &length);
    if (!code || length < kHeaderBytes) return;
    Thread thread;
    thread.active = true;
    thread.layer = layer;
    thread.sheet = script;
    thread.palette = palette;
    thread.index = index;
    thread.target = target;
    thread.ptr = static_cast<size_t>(code - anim_->scripts.data()) + kHeaderBytes;
    thread.rate = (code[0] >> 4) + 1;
    thread.counter = counter;
    thread.flipped = flipped_;
    const int align = code[1] & 0xE0;
    const int halfHeight = anim_->gfx(script).height * FF6_ATTACK_CELL / 2;
    place(target, true, align, halfHeight, flipped_, thread.posX, thread.posY);
    thread.targetX = thread.posX;
    thread.targetY = thread.posY;
    place(attacker_, false, align, halfHeight, flipped_, thread.attackerX, thread.attackerY);
    if (layer == 1) {
        bg1Base_[0] = bg1Cur_[0] = thread.posX;
        bg1Base_[1] = bg1Cur_[1] = thread.posY;
    }
    threads_.push_back(thread);
}

void AttackAnimPlayer::start(int attack, const AnimBody& attacker, const std::vector<AnimBody>& targets, bool flipped, int battleType) {
    begin(attacker, flipped);
    attack_ = attack;
    battleType_ = battleType;
    const AttackAnimProp prop = anim_->prop(attack);
    sfx_ = prop.sfx;
    for (const AnimInit& entry : kAnimInits) {  // what the init function of the animation sets
        if (entry.type != (prop.init & 0x7F)) continue;
        w12_ = entry.w12;
        w34_ = entry.w34;
        shape_ = std::max(entry.shape, 0);
        if (entry.cgwsel >= 0 && (entry.cgwsel & 0x02)) {
            setBlends(entry.cgadsub);
            subscreen_ = entry.subscreen;
        }
        break;
    }
    for (const PlaneInit& entry : kPlanes) {  // mode 7 starts with the identity, and bg1 off the screen until a script shows it
        if (entry.type != (prop.init & 0x7F)) continue;
        plane_.kind = entry.kind;
        plane_.tileSet = entry.tileSet;
        plane_.mapSet = entry.mapSet;
        plane_.palette = prop.bg1Pal;
        plane_.esper = entry.esper;
        plane_.row = entry.row;
        plane_.crusader = entry.crusader;
        mode7_.a = mode7_.d = kMode7Unit;
        mainLayers_ &= entry.kind == AnimPlane::kTiles ? 0x12 : ~0x01;
        break;
    }
    const int initType = prop.init & 0x7F;
    if (initType == FF6_ATTACK_INIT_JUSTICE || initType == FF6_ATTACK_INIT_POSSESS) bg3High_ = false;
    if (initType == FF6_ATTACK_INIT_JUSTICE) {
        behindLimit_ = FF6_JUSTICE_BEHIND_Y;
        veilBg3_ = true;
    }
    // Some inits give the window 2 its first lines at once, as wide as the screen but for the margins of 8 pixels.
    const int windowLines = initType == FF6_ATTACK_INIT_JUSTICE ? FF6_JUSTICE_WINDOW_LINES : initType == 0x5C ? 72 : 0;
    const bool wholeWidth = initType == FF6_ATTACK_INIT_JUSTICE;  // the video shows Alexander up to both edges
    for (int line = 0; line < windowLines; ++line) {
        spanLeft_[line] = wholeWidth ? 0 : 8;
        spanRight_[line] = wholeWidth ? 0xFF : 0xF7;
    }
    for (const EsperSheetInit& entry : kEsperSheets) {
        if (entry.type == (prop.init & 0x7F)) esperSheet_ = {entry.layer, entry.esper};
    }
    const int scripts[3] = {prop.sprite, prop.bg1, prop.bg3};
    const int pals[3] = {prop.spritePal, prop.bg1Pal, prop.bg3Pal};
    for (int layer = 0; layer < 3; ++layer) {
        sheet_[layer] = scripts[layer] == FF6_ATTACK_NO_SCRIPT ? -1 : scripts[layer] & ~FF6_ATTACK_GFX_ONLY_FLAG;
        palettes_[layer] = pals[layer];
    }
    sprite_ = sheet_[0];
    palette_ = palettes_[0];
    const auto make = [&](int layer, const AnimBody& target, int counter, int index) {
        if (scripts[layer] == FF6_ATTACK_NO_SCRIPT || (scripts[layer] & FF6_ATTACK_GFX_ONLY_FLAG)) return;
        addThread(layer, sheet_[layer], palettes_[layer], target, counter, index);
    };

    int copies = 0, gap = 0;
    for (const Copies& entry : kCopies) {
        if (entry.type == (prop.init & 0x7F)) {
            copies = entry.copies;
            gap = entry.delay;
        }
    }
    int counter = 1;
    for (const AnimBody& target : targets) {
        for (int index = 0; index <= copies; ++index) make(0, target, counter + index * gap, index);
        counter += prop.delay;
    }
    // The esper has a thread of its own, at the attacker; the purifier makes three of it, and these three have the picture of
    // the crusader as their sheet, as the esper of mode 7 has its own.
    if (prop.extra != FF6_ATTACK_NO_SCRIPT && (prop.extra & FF6_ATTACK_GFX_ONLY_FLAG)) {
        const int type = prop.init & 0x7F;
        const int figures = type == FF6_ATTACK_INIT_PURIFIER || type == FF6_ATTACK_INIT_JUSTICE ? 3 : 1;
        const int purifier[3] = {FF6_ESPER_CRUSADER, FF6_ESPER_CRUSADER_DARK, FF6_ESPER_CRUSADER_SHEET};
        for (int index = 0; index < figures; ++index) {
            const int sheet = type == FF6_ATTACK_INIT_PURIFIER ? purifier[index] : type == 0x45 ? FF6_ESPER_ODIN : type == 0x6B ? FF6_ESPER_RAIDEN : -1;
            addThread(0, prop.extra & ~FF6_ATTACK_GFX_ONLY_FLAG, prop.spritePal, attacker, 1, index);
            if (!threads_.empty()) threads_.back().esper = sheet;
            if (!threads_.empty() && sheet_[0] >= 0) threads_.back().sheet = sheet_[0];  // its frames are those of the sprite graphics
            // The side figures touch the edges of the screen, which the 8 pixels that their scripts step inward do not allow.
            if (type == FF6_ATTACK_INIT_PURIFIER && index > 0 && !threads_.empty()) {
                threads_.back().offsetX = (index == 1 ? -FF6_PURIFIER_REACH : FF6_PURIFIER_REACH) * (flipped_ ? -1 : 1);
            }
        }
    }
    // The graphics of the backgrounds are shown from the first target.
    if (!targets.empty()) {
        // The picture of Justice starts a frame after its sprites, so that it jitters in step with them (their loops start on a move).
        make(1, targets.front(), initType == FF6_ATTACK_INIT_JUSTICE ? 2 : 1, 0);
        make(2, targets.front(), 1, 0);
    }
    // The lines of the purifier start from the head of the left figure, which a frame that covers the screen puts at its center.
    if ((prop.init & 0x7F) == FF6_ATTACK_INIT_PURIFIER) {
        for (Thread& thread : threads_) {
            if (thread.layer == 2) thread.posX = kScreenX, thread.posY = kScreenY;
        }
    }
    done_ = threads_.empty();
}

void AttackAnimPlayer::startWeapon(const WeaponAnimProp& weapon, const AnimBody& attacker, const AnimBody& target, bool flipped,
                                   bool leftHand, bool miss) {
    begin(attacker, flipped);
    attack_ = -1;
    sfx_ = weapon.sfx;
    miss_ = miss;
    // The hand that swings is the left one, unless the character faces the other way.
    int script = (leftHand != flipped) ? weapon.leftScript : weapon.rightScript;
    if (attacker.monster) script = FF6_ATTACK_MONSTER_WEAPON_SCRIPT;
    addThread(0, script, weapon.weaponPal, target, 1, 0);
    if (!threads_.empty()) {  // the weapon swings around the attacker
        threads_.back().posX = threads_.back().attackerX;
        threads_.back().posY = threads_.back().attackerY;
    }
    if (!miss) {
        int hit = weapon.hitScript;
        if (attacker.monster && hit >= FF6_ATTACK_MONSTER_HIT_FIRST) hit += 0x200;
        addThread(weapon.hitIsSprite ? 0 : 1, hit, weapon.hitPal, target, 1, 0);
    }
    sprite_ = script;
    palette_ = weapon.weaponPal;
    done_ = threads_.empty();
}

void AttackAnimPlayer::showFrame(Thread& thread, int frame) {
    thread.spriteOn = true;
    // A frozen layer keeps the picture that it has; only where it is goes on.
    const bool frozen = thread.layer > 0 && hidden_[thread.layer] && thread.spriteSheet >= 0 && thread.spriteFrame >= 0;
    if (!frozen) {
        thread.spriteFrame = frame == kBlankFrame ? -1 : (frame + thread.frameOffset) & kFrameMask;
        thread.spriteSheet = thread.sheet;
        thread.spritePalette = thread.palette;
    }
    thread.spriteX = thread.posX + thread.offsetX;
    thread.spriteY = thread.posY + thread.offsetY;
    if (thread.layer == 1) {
        bg1Cur_[0] = thread.spriteX;
        bg1Cur_[1] = thread.spriteY;
        scrollBg1();
        thread.spriteX -= bg1Shift_[0];
        thread.spriteY -= bg1Shift_[1];
    }
    if (thread.layer == 2) {
        thread.spriteX -= bg3Shift_[0];
        thread.spriteY -= bg3Shift_[1];
    }
    thread.spriteFlipped = thread.flipped;
    thread.spriteLayer = thread.layer;
}

void AttackAnimPlayer::move(Thread& thread, int argument) {
    int dx = 0, dy = 0;
    stepOf(argument, thread.flipped, dx, dy);
    thread.posX += dx;
    thread.posY += dy;
}

// The palette commands set the value of a component of the colors, or change the one that was set.
bool AttackAnimPlayer::palette(Thread& thread, int op, int argument) {
    (void)thread;
    for (const PaletteOp& entry : kPaletteOps) {
        if (entry.op != op) continue;
        AnimTint& tint = tints_[entry.group];
        int* parts[3] = {&tint.r, &tint.g, &tint.b};
        const bool affected[3] = {(argument & 0x80) != 0, (argument & 0x40) != 0, (argument & 0x20) != 0};
        if (entry.relative) {
            int first = 0;
            while (first < 2 && !affected[first]) ++first;
            const int amount = argument & 0x0F;
            const int value = std::clamp(*parts[first] + ((argument & 0x10) ? -amount : amount), 0, kTintMax);
            for (int part = 0; part < 3; ++part) {
                if (affected[part]) *parts[part] = value;
            }
        } else {
            for (int part = 0; part < 3; ++part) *parts[part] = affected[part] ? argument & 0x1F : 0;
        }
        tint.subtract = entry.subtract;
        tint.active = tint.r || tint.g || tint.b;
        return true;
    }
    return false;
}

void AttackAnimPlayer::shiftBody(const AnimBody& body, int dx, int dy) {
    AnimShift& shift = shifts_[(body.monster ? 4 : 0) + body.slot];
    shift.x += dx;
    shift.y += dy;
}

void AttackAnimPlayer::hideBody(const AnimBody& body, bool hidden) { shifts_[(body.monster ? 4 : 0) + body.slot].hidden = hidden; }

// The color math of the init functions and of $D4: CGADSUB names the layers (the sprites, bg1 and bg3 here) that are added to
// what is under them, or taken from it, by half or not.
void AttackAnimPlayer::setBlends(int cgadsub) {
    const int masks[3] = {0x10, 0x01, 0x04};
    for (int layer = 0; layer < 3; ++layer) {
        blends_[layer] = (cgadsub & masks[layer]) ? AnimBlend{true, (cgadsub & 0x80) != 0, (cgadsub & 0x40) != 0} : AnimBlend();
    }
}

// Window 2 is the circle; window 1 is empty. A layer with a window enabled is hidden where the window mask is set, and the
// inversion turns the inside of a window into the outside.
bool AttackAnimPlayer::layerVisible(int layer, int x, int y) const {
    const int nibble = windowNibble(layer);
    if (!(nibble & 0x0A)) return true;
    bool masked = false;
    if (nibble & 0x02) masked = masked || (nibble & 0x01) != 0;  // window 1 holds nothing
    if (nibble & 0x08) {
        const bool inside = y >= 0 && y < 256 && x >= spanLeft_[y] && x <= spanRight_[y];
        masked = masked || inside != ((nibble & 0x04) != 0);
    }
    return !masked;
}

// The sine of the game (a table of -127 to 127) times a multiplier, over 256, rounded towards zero.
static int sine8(int angle, int multiplier) {
    const int value = static_cast<int>(std::lround(127 * std::sin(2 * 3.14159265358979 * (angle & 0xFF) / 256)));
    return value < 0 ? -((-value * multiplier) >> 8) : (value * multiplier) >> 8;
}

// The span of each line of the circle: the half widths of the radius, bounded by the margins of 8 pixels, round a center. The
// blobs pulse their radius and every line of them (and of the Bio Blast) sways on a sine whose angle moves at each update.
void AttackAnimPlayer::updateCircle() {
    int radius = circle_.size & 0x7F;
    if (radius == 0) return;
    const bool blob = shape_ == kSlimerBlob || shape_ == kSmallBlob || shape_ == kBigBlob;
    if (blob) radius = std::clamp(radius + sine8(circleAngle_, radius), 1, 127);
    for (int line = 0; line < 256; ++line) {
        spanLeft_[line] = 255;
        spanRight_[line] = 0;
    }
    const auto half = [&](int offset) {
        return static_cast<int>(std::lround(std::sqrt(static_cast<double>(std::max(radius * radius - offset * offset, 0)))));
    };
    const auto put = [&](int line, int width, int center) {
        if (line < 0 || line >= 256) return;
        const int left = center - width, right = center + width;
        spanLeft_[line] = left < 8 ? 8 : left;
        spanRight_[line] = right > 0xF7 ? 0xF7 : right;
    };
    const int x = circle_.x, y = circle_.y;
    // The sway of a line: the Bio Blast by the line of the circle, the blobs by the line of the table of the game, where the
    // lines above the center count down from 127 and the ones below up from 128.
    int blobStep = 4, blobAmplitude = 0x10, blobMove = 8;
    if (shape_ == kSmallBlob) blobStep = 8, blobAmplitude = 0x0A, blobMove = 4;
    if (shape_ == kBigBlob) blobAmplitude = 0x20;
    const int bioFirst = circleAngle_ + 2, blobFirst = circleAngle_ + blobMove;
    const int octant = static_cast<int>(radius * 0.7071) + 1;  // the Bio Blast stops swaying with the lines of the first octant
    const auto sway = [&](int line, bool upper) {
        if (shape_ == kBioBlast) return sine8(bioFirst + 4 * (std::min(line, octant - 1) + 1), circle_.speed);
        if (blob) return sine8(blobFirst + blobStep * ((upper ? 127 - line : 128 + line) + 1), blobAmplitude);
        return 0;
    };
    switch (shape_) {
        case kHorzOval:  // half as many lines, as wide
            for (int i = 0; i < radius / 2; ++i) {
                put(y - 1 - i, half(2 * i), x);
                put(y + i, half(2 * i), x);
            }
            break;
        case kVertOval:  // twice as many lines
            for (int i = 0; i < radius; ++i) {
                put(y - 1 - 2 * i, half(i), x);
                put(y - 2 - 2 * i, half(i), x);
                put(y + 2 * i, half(i), x);
                put(y + 2 * i + 1, half(i), x);
            }
            break;
        case kUltima:  // a dome: the upper half is the whole circle, the lower half has half as many lines
            for (int i = 0; i < radius; ++i) {
                put(y - 1 - i, half(i), x);
                if (i % 2 == 0) put(y + i / 2, half(i), x);
            }
            break;
        case kTopBeam:  // the lower half of the circle, and above it the widest line all the way up
            for (int i = 0; i < radius; ++i) put(y + i, half(i), x);
            for (int line = 0; line < y; ++line) put(line, radius, x);
            break;
        default:  // the circle, the Bio Blast and the blobs
            for (int i = 0; i < radius; ++i) {
                put(y - 1 - i, half(i), x + sway(i, true));
                put(y + i, half(i), x + sway(i, false));
            }
            break;
    }
    if (shape_ == kBioBlast) circleAngle_ += 2;
    if (blob) circleAngle_ += blobMove;
    circleAngle_ &= 0xFF;
}

// The scroll of bg1 is how far its thread has gone since a script set where it starts.
void AttackAnimPlayer::scrollBg1() {
    mode7_.scrollX = bg1Base_[0] - bg1Cur_[0];
    mode7_.scrollY = bg1Base_[1] - bg1Cur_[1];
}

// Zoom and turn: the matrix is the sine and the cosine of an angle (-2 of 256 turns per frame) times a scale that falls by 2 at
// each use, as the game works it out with its table of sines.
void AttackAnimPlayer::rotateMode7() {
    const auto times = [&](int angle) {
        const int sine = static_cast<int>(std::lround(32767 * std::sin(2 * 3.14159265358979 * (angle & 0xFF) / 256)));
        const int magnitude = (std::abs(sine) * rotateScale_ * 2) >> 16;
        return sine < 0 ? -magnitude : magnitude;
    };
    const int angle = -(2 * frames_);
    mode7_.a = mode7_.d = times(angle);
    mode7_.b = times(angle + 0x40);
    mode7_.c = -mode7_.b;
    rotateScale_ = (rotateScale_ - 2) & 0xFF;
}

// The gradients are fixed colors that change from one scanline to the next, as the HDMA of the game feeds them: a triangle of
// 32 lines (0 up to 15 and back) that scrolls, less the level; or a wide one that grows to the middle of the screen.
void AttackAnimPlayer::gradient(int sub, int argument) {
    static constexpr int kColors[2][8] = {{0x80, 0x40, 0x60, 0x20, 0xA0, 0xC0, 0x80, 0x20}, {0x20, 0x60, 0x20, 0x60, 0x20, 0x60, 0x20, 0x60}};
    constexpr int kLines = 151, kRamp = 32, kRampTop = 16;
    const auto ramp = [](int at) { return (at & (kRamp - 1)) < kRampTop ? (at & (kRamp - 1)) : kRamp - 1 - (at & (kRamp - 1)); };
    const auto put = [&](int line, int color, int level) {
        if (line < 0 || line >= 256) return;
        level = std::clamp(level, 0, 31);
        fixed_[line] = {(color & 0x20) ? level : 0, (color & 0x40) ? level : 0, (color & 0x80) ? level : 0};
    };
    switch (sub) {
        case 0x37:
            for (AnimFixedColor& line : fixed_) line = AnimFixedColor();
            break;
        case 0x3A: gradientLevel_ = argument; break;
        case 0x39: {  // about four triangles of blue
            gradientLevel_ = (gradientLevel_ + argument) & 0xFF;
            const int now = signedByte(gradientLevel_);
            put(0, 0x80, 0);
            for (int line = 1; line <= kRamp; ++line) {
                const int value = ramp(gradientCounter_ + line - 1) - now;
                for (int copy = 0; copy < 4; ++copy) put(line + kRamp * copy, 0x80, value);
                if (line <= 22) put(line + 4 * kRamp, 0x80, value);
            }
            ++gradientCounter_;
            break;
        }
        case 0x5F: {  // wide: rises from both ends to the middle, and the level takes it away
            gradientLevel_ = (gradientLevel_ + argument) & 0xFF;
            const int now = signedByte(gradientLevel_);
            put(0, 0x80, 0);
            for (int step = 0; step < kLines / 2; ++step) {
                put(1 + step, 0x80, (step >> 2) - now);
                put(kLines - step, 0x80, (step >> 2) - now);
            }
            ++gradientCounter_;
            break;
        }
        default: {  // 0x33 the rainbow and 0x4B red and yellow: the color changes every 32 lines
            const int base = sub == 0x4B ? 8 : 0;
            int index = ((gradientLevel_ & 0xE0) >> 5) + base;
            int at = gradientCounter_ & (kRamp - 1);
            put(0, 0x80, 0);
            for (int line = 1; line < kLines; ++line) {
                put(line, kColors[base ? 1 : 0][index & 7], ramp(at) - argument);
                at = (at + 1) & (kRamp - 1);
                if (at == 0) index = ((index + 1) & 7) + base;
            }
            ++gradientCounter_;
            gradientLevel_ = (gradientLevel_ + 1) & 0xFF;
            break;
        }
    }
}

void AttackAnimPlayer::special(Thread& thread, const uint8_t* code, size_t at) {
    const int sub = code[at + 1];
    size_t next = at + static_cast<size_t>(specialLength(sub));
    switch (sub) {
        case 0x01: case 0x66: thread.offsetX = thread.offsetY = 0; break;  // clear the offset of the position
        case 0x10: thread.posX = thread.targetX; thread.posY = thread.targetY; break;  // move to the target
        case 0x4F: thread.posX = thread.attackerX; thread.posY = thread.attackerY; break;  // move to the attacker
        case 0x2E: thread.posX = code[at + 2], thread.posY = code[at + 3]; break;  // move to a position of the screen
        case 0x86: case 0x87: case 0x8C: sounds_.push_back(code[at + 2] ? code[at + 2] : sfx_); break;  // sound with a pan
        case 0x2D: {  // jump by the type of the battle
            const int type = std::min(battleType_, 2);
            next = static_cast<size_t>(word(code, at + 2 + 2 * static_cast<size_t>(type)));
            break;
        }
        case 0x32: next = static_cast<size_t>(word(code, at + 2 + (thread.flipped ? 2 : 0))); break;  // jump by the direction
        case 0x72:  // jumps ahead if the blow did not miss
            if (!miss_) next = at + 3 + code[at + 2];
            break;
        case 0x3E: mainLayers_ = code[at + 2]; break;  // the layers of the main screen
        case 0x27:  // hides (1) or shows (0) every character
            for (int slot = 0; slot < 4; ++slot) shifts_[slot].hidden = code[at + 2] == 1;
            break;
        case 0x40: mode7_.on = (code[at + 2] & 7) == 7; break;  // the screen mode
        case 0x41:  // the zoom and the center of mode 7 change, by signed bytes
            mode7_.a = wrap16(mode7_.a + signedByte(code[at + 2]));
            mode7_.d = wrap16(mode7_.d + signedByte(code[at + 3]));
            mode7_.x = wrap16(mode7_.x + signedByte(code[at + 4]));
            mode7_.y = wrap16(mode7_.y + signedByte(code[at + 5]));
            break;
        case 0x42: mode7_.flipX = (code[at + 2] & 1) != 0, mode7_.flipY = (code[at + 2] & 2) != 0; break;
        case 0x5E: rotateMode7(); break;
        case 0x38: bg3High_ = true; break;  // the high priority of bg3
        case 0x2A: case 0x2B: case 0x2C: {  // a new palette for the sprites, bg1 or bg3
            const int layer = sub - 0x2A;
            palettes_[layer] = code[at + 2];
            if (layer == 0) palette_ = palettes_[0];
            for (Thread& other : threads_) {
                if (other.layer == layer) other.palette = palettes_[layer];
            }
            break;
        }
        case 0x58: shape_ = code[at + 2]; break;  // the shape of the circle
        case 0x57: w34_ = code[at + 2]; break;  // the windows of bg3 and bg4
        case 0x37: case 0x39: case 0x3A: case 0x5F: case 0x33: case 0x4B: gradient(sub, sub == 0x37 ? 0 : (sub == 0x3A || sub == 0x33 || sub == 0x4B ? code[at + 2] : signedByte(code[at + 2]))); break;
        default: skip(0x100 | sub); break;
    }
    thread.ptr = next;
}

void AttackAnimPlayer::command(Thread& thread, const uint8_t* code, size_t at) {
    const int op = code[at];
    size_t next = at + static_cast<size_t>(commandLength(op));
    switch (op) {
        case 0xA5:  // the circle: where it starts, its size, and where and how big it ends
            circle_.x = code[at + 1];
            circle_.y = code[at + 2];
            circle_.size = code[at + 3];
            circle_.finalX = code[at + 4];
            circle_.finalY = code[at + 5];
            circle_.finalSize = code[at + 6];
            circle_.speed = code[at + 7];
            break;
        case 0xA6: case 0xA9: {  // it moves, and grows with $A6, up to where it ends
            int dx = code[at + 1];
            if (op == 0xA9 && thread.flipped) dx = (-dx) & 0xFF;
            if (!((dx & 0x80) && circle_.x < 0x21)) {
                const int x = (circle_.x + dx) & 0xFF;
                circle_.x = x < circle_.finalX ? x : circle_.finalX;
            }
            const int y = (circle_.y + code[at + 2]) & 0xFF;
            circle_.y = y < circle_.finalY ? y : circle_.finalY;
            if (op == 0xA6) {
                const int size = (circle_.size + code[at + 3]) & 0xFF;
                circle_.size = size < circle_.finalSize ? size : circle_.finalSize;
            }
            break;
        }
        case 0xA7: updateCircle(); break;
        case 0xA8: case 0xD3: circle_.x = thread.attackerX; circle_.y = thread.attackerY; break;
        case 0xC3: circle_.x = thread.targetX; circle_.y = thread.targetY; break;
        case 0xA3: case 0xA4: {  // some colors of a palette turn one place every few runs (the palettes of the sprites, bg1 and bg3)
            if (thread.cycleWait == 0) {
                const int row = code[at + 2] >> 4;
                const int layer = row == 11 ? 0 : row == 3 ? 1 : row == 1 ? 2 : -1;
                if (layer >= 0) {
                    cycles_[layer].first = code[at + 1] >> 4;
                    cycles_[layer].count = code[at + 1] & 0x0F;
                    cycles_[layer].steps += op == 0xA3 ? 1 : -1;
                }
                thread.cycleWait = code[at + 2] & 0x0F;
            }
            thread.cycleWait = (thread.cycleWait + 255) & 0xFF;
            break;
        }
        case 0xD4: {  // color math: with the subscreen, the layers it names are added to (or taken from) what is under them
            for (AnimBlend& blend : blends_) blend = AnimBlend();
            fixedMath_ = (code[at + 1] & 0x02) ? 0 : code[at + 2];
            if (code[at + 1] & 0x02) {
                setBlends(code[at + 2]);
                subscreen_ = code[at + 3];
            }
            break;
        }
        case 0xAC: {  // the amplitude and the speed of the waves of the layers that the command names (bg1 and bg3)
            const int what = code[at + 1], how = code[at + 2];
            for (const int layer : {1, 2}) {
                if (!(what & (layer == 1 ? 0x80 : 0x20))) continue;
                AnimWave& wave = waves_[layer];
                if (how & 0x80) wave.vAmplitude = how & 0x3F, wave.vFrequency = what & 0x1F;
                if (how & 0x40) wave.hAmplitude = how & 0x3F, wave.hFrequency = what & 0x1F;
            }
            break;
        }
        case 0xAD: break;  // the type of the scroll table; the waves here are always sines
        case 0xAE: {  // the waves take their next step
            const int what = code[at + 1];
            for (const int layer : {1, 2}) {
                if (!(what & (layer == 1 ? 0x04 : 0x01))) continue;
                AnimWave& wave = waves_[layer];
                wave.active = true;
                if (what & 0x80) {
                    wave.vertical = true;
                    wave.vShown = wave.vPhase;
                    wave.vPhase = (wave.vPhase + wave.vFrequency) & 0xFF;
                }
                if (what & 0x40) {
                    wave.horizontal = true;
                    wave.hShown = wave.hPhase;
                    wave.hPhase = (wave.hPhase + wave.hFrequency) & 0xFF;
                }
            }
            break;
        }
        case 0x83: move(thread, code[at + 1]); break;
        case 0x81: case 0x82: {  // the pose of the attacker or of the target, by the way it faces
            const AnimBody& body = op == 0x81 ? attacker_ : thread.target;
            if (!body.monster) shifts_[body.slot].pose = code[at + 1 + (thread.flipped ? 1 : 0)];
            break;
        }
        case 0x86: case 0x87: {  // the attacker or the target moves
            int dx = 0, dy = 0;
            stepOf(code[at + 1], thread.flipped, dx, dy);
            shiftBody(op == 0x86 ? attacker_ : thread.target, dx, dy);
            break;
        }
        case 0xE1: case 0xF1: {  // the attacker or the target is shown or hidden, if it is of the kind that the command names
            const AnimBody& body = op == 0xE1 ? attacker_ : thread.target;
            if (body.monster == ((code[at + 1] & 0x80) != 0)) hideBody(body, (code[at + 1] & 1) == 0);
            break;
        }
        // These change nothing that this screen draws: the order of the sprites, the tiles in front, the wait for a scanline.
        case 0xD1: case 0xF7: case 0xC2: break;
        case 0x90: thread.priority = code[at + 1] >> 4; break;
        case 0xD0: spritePriority_ = code[at + 1] >> 4; break;  // the priority of the combatants against bg1 in mode 7
        case 0x84: thread.rate = code[at + 1]; break;
        case 0x85: case 0x91: thread.posX = thread.attackerX; thread.posY = thread.attackerY; break;
        case 0x9F:  // a loop that goes as many times as there are threads
            thread.loopPtr = at + 2;
            thread.loopCount = static_cast<int>(std::count_if(threads_.begin(), threads_.end(), [](const Thread& t) { return t.active; }));
            thread.frameOffset = 0;
            break;
        case 0x89: case 0x8B:  // a loop (the second one numbers its frames)
            thread.loopPtr = at + 2;
            thread.loopCount = code[at + 1];
            if (op == 0x8B) thread.frameOffset = 0;
            break;
        case 0x8A: case 0x8C:
            if (thread.loopCount > 0) {
                --thread.loopCount;
                next = thread.loopPtr;
                if (op == 0x8C) ++thread.frameOffset;
            } else if (op == 0x8C) {
                thread.frameOffset = 0;
            }
            break;
        case 0x98:  // the frame offset counts by itself
            if (--thread.autoCounter == 0) {
                thread.autoCounter = code[at + 1];
                const int last = code[at + 2] & 0x0F;
                if (++thread.frameOffset == last) thread.frameOffset = code[at + 2] >> 4;
            }
            break;
        case 0x9A: thread.flipped = thread.target.monster != (flipped_ != attacker_.monster); break;  // faces as the target does
        case 0xA2: if (code[at + 1] & 0x40) thread.flipped = !thread.flipped; break;
        case 0xBF: thread.returnPtr = next; next = static_cast<size_t>(word(code, at + 1)); break;
        case 0x94: case 0x95: case 0xE7: {  // the vector from the attacker to the target, or to a random place on it
            double toX = thread.targetX, toY = thread.targetY;
            if (op == 0x94) {
                const int height = std::max(thread.target.bottomY - thread.target.topY, 8);
                toX += static_cast<int>(nextRandom() % static_cast<uint32_t>(thread.target.width)) - thread.target.width / 2;
                toY += static_cast<int>(nextRandom() % static_cast<uint32_t>(height)) - height / 2;
            }
            const double dx = toX - thread.attackerX, dy = toY - thread.attackerY;
            const double length = std::sqrt(dx * dx + dy * dy);
            thread.vecLength = static_cast<int>(length);
            thread.vecX = length > 0 ? dx / length : 0;
            thread.vecY = length > 0 ? dy / length : 0;
            thread.vecPos = 0;
            break;
        }
        case 0x93:  // a place on the vector
            thread.vecPos = code[at + 1];
            thread.offsetX = static_cast<int>(std::lround(thread.vecX * thread.vecPos));
            thread.offsetY = static_cast<int>(std::lround(thread.vecY * thread.vecPos));
            break;
        case 0x92:  // one step along the vector, and again until the end of it
            thread.offsetX = static_cast<int>(std::lround(thread.vecX * thread.vecPos));
            thread.offsetY = static_cast<int>(std::lround(thread.vecY * thread.vecPos));
            thread.vecPos += code[at + 1];
            if (thread.vecPos < thread.vecLength && code[at + 2] != 0) next = at + 2 - code[at + 2];
            break;
        case 0xBD: {  // the graphics of bg1 or bg3 stop being copied (frozen) or go on, for the layer that the command names
            const int what = code[at + 1];
            if (what & 0x80) hidden_[1] = (what & 0x20) != 0;
            if (what & 0x40) hidden_[2] = (what & 0x10) != 0;
            break;
        }
        case 0xC4:  // the thread of a background goes where this one is
            if (code[at + 1] & 0x80) {
                bg1Base_[0] = bg1Cur_[0] = thread.posX + thread.offsetX;
                bg1Base_[1] = bg1Cur_[1] = thread.posY + thread.offsetY;
                scrollBg1();
                // The picture is centered at its thread only while the scroll starts from half its size; this moves it.
                if (sheet_[1] >= 0) {
                    const AttackGfxProp size = anim_->gfx(sheet_[1]);
                    bg1Shift_[0] = bg1Base_[0] - size.width * 8;
                    bg1Shift_[1] = bg1Base_[1] - size.height * 8;
                }
            }
            if ((code[at + 1] & 0x40) && sheet_[2] >= 0) {  // the same for bg3, from where its thread is
                const AttackGfxProp size = anim_->gfx(sheet_[2]);
                bg3Shift_[0] = thread.posX + thread.offsetX - size.width * 8;
                bg3Shift_[1] = thread.posY + thread.offsetY - size.height * 8;
            }
            for (Thread& other : threads_) {
                if (other.layer == (code[at + 1] == 0x80 ? 1 : 2) && &other != &thread) {
                    other.posX = thread.posX;
                    other.posY = thread.posY;
                }
            }
            break;
        case 0xC0: next = thread.returnPtr; break;
        case 0xC9: sounds_.push_back(code[at + 1] ? code[at + 1] : sfx_); break;
        case 0xD2: thread.targetX = thread.flipped ? ~code[at + 1] & 0xFF : code[at + 1]; thread.targetY = code[at + 2]; break;
        case 0xEC: {  // the thread goes on in another layer, with the graphics of that layer
            const int layer = std::min(code[at + 1] & 3, 2);
            thread.layer = layer;
            if (sheet_[layer] >= 0) {
                thread.sheet = sheet_[layer];
                thread.palette = palettes_[layer];
            }
            thread.frameOffset = 0;
            break;
        }
        case 0xE8: case 0xEF: {  // around the position of the thread: a radius and an angle (of 256 to the turn) that grow by steps
            const double angle = thread.polarAngle * 2 * 3.14159265358979 / 256;
            const int x = static_cast<int>(std::lround(thread.polarRadius * std::sin(angle)));
            const int radiusY = op == 0xEF ? thread.polarRadius / 2 : thread.polarRadius;  // the second one is flattened
            thread.offsetX = thread.flipped ? ~x : x;
            thread.offsetY = static_cast<int>(std::lround(radiusY * std::cos(angle)));
            thread.polarRadius = (thread.polarRadius + code[at + 1]) & 0xFF;
            thread.polarAngle = (thread.polarAngle + code[at + 2]) & 0xFF;
            break;
        }
        case 0xE9: break;  // moves at random; the offset stays
        case 0xEB: next = static_cast<size_t>(word(code, at + 1 + 2 * static_cast<size_t>(std::min(thread.index, 7)))); break;
        case 0xF0: next = static_cast<size_t>(word(code, at + 1 + 2 * static_cast<size_t>(thread.target.monster ? 4 : thread.target.slot & 3))); break;
        case 0xF3: next = static_cast<size_t>(word(code, at + 1 + 2 * static_cast<size_t>(attacker_.monster ? 4 : attacker_.slot & 3))); break;
        case 0xF8: next = static_cast<size_t>(word(code, at + 1)); break;  // the normal one
        case 0xFA: next = static_cast<size_t>(word(code, at + 1)); break;
        case 0x80: special(thread, code, at); return;
        case 0xC7: thread.active = false; thread.spriteOn = false; return;  // the commands of events are not part of an attack
        default:
            if (!palette(thread, op, code[at + 1])) skip(op);
            break;
    }
    thread.ptr = next;
}

// Runs the script of a thread up to the next frame.
void AttackAnimPlayer::run(Thread& thread) {
    const uint8_t* code = anim_->scripts.data();
    const size_t size = anim_->scripts.size();
    for (int guard = 0; guard < 4096 && thread.active; ++guard) {
        if (thread.ptr >= size) {
            thread.active = thread.spriteOn = false;
            return;
        }
        const int op = code[thread.ptr];
        if (op == kEnd) {
            thread.active = thread.spriteOn = false;
            return;
        }
        if (op < 0x80) {
            showFrame(thread, op);
            ++thread.ptr;
            return;
        }
        // A command may need bytes past the end of the data; keep to what there is.
        if (thread.ptr + 12 >= size) {
            thread.active = thread.spriteOn = false;
            return;
        }
        command(thread, code, thread.ptr);
    }
}

bool AttackAnimPlayer::tick() {
    if (done_) return false;
    ++frames_;
    for (Thread& thread : threads_) {
        if (!thread.active) continue;
        thread.counter = (thread.counter - 1) & 0xFF;  // a byte, as in the game: zero rolls over to 255
        if (thread.counter != 0) continue;
        thread.counter = thread.rate;
        run(thread);
    }
    sprites_.clear();
    bool any = false;
    for (const Thread& thread : threads_) {
        if (!thread.active) continue;
        any = true;
        // The sprites of priority 0 are under the field (bg2), which hides them.
        if (behindLimit_ > 0 && thread.spriteLayer == 0 && thread.priority == 0) continue;
        if (thread.spriteOn && thread.spriteFrame >= 0 && thread.spriteSheet >= 0) {
            sprites_.push_back({thread.spriteSheet, thread.spriteFrame, thread.spriteX, thread.spriteY, thread.spritePalette,
                                thread.spriteFlipped, thread.spriteLayer, thread.esper, veilBg3_ && thread.spriteLayer > 0});
        }
    }
    done_ = !any || frames_ >= kMaxFrames;
    if (done_) sprites_.clear();
    return !done_;
}

}  // namespace ff6
