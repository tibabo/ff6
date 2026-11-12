#pragma once

#include <cstdint>
#include <array>
#include <vector>

#include "data/attack_anim.hpp"

namespace ff6 {

// Where a combatant stands on the screen, in pixels.
struct AnimBody {
    bool monster = false;
    int slot = 0;  // the character (0-3) or the monster (0-5)
    int centerX = 0, centerY = 0, topY = 0, bottomY = 0;
    int width = 16;
    bool backRow = false;  // a character in the back row
};

// A sprite of the animation on the screen: the frame of the sheet, its corner and whether it is mirrored.
struct AnimSprite {
    int script = 0, frame = 0, x = 0, y = 0, palette = 0;
    bool flipped = false;
    int layer = 0;  // 0 sprites, 1 bg1, 2 bg3: the graphics are those of the script of that layer
    int esper = -1;  // the order of the esper whose picture is the sheet of this sprite, when the thread has one of its own
    bool veil = false;  // a frame of bg1 or bg3 of Justice: the game shows it a little wider than its cells
    bool back = false;  // the sprite goes behind the combatants (the game's IsBackSprite)
};

// What the palette commands do to a group of colors: a value for each component, added to the colors or taken from them.
struct AnimTint {
    int r = 0, g = 0, b = 0;  // 0 to 31
    bool subtract = false, active = false;
};

// How far a script moved a combatant (the target or the attacker) and whether it hid it.
struct AnimShift {
    int x = 0, y = 0;
    bool hidden = false;
    int pose = -1;  // the pose of a character that the script chose
    bool flip = false;  // the pose is turned the other way round
    int armor = 0;  // what its magitek armor does ($F9): 0 stands, 1 walks, 2 fires
    int action = 0;  // the animated action of a character (CHAR_ACTION), which makes its pose go on changing
    int vx = 0, vy = 0;  // where a jump along a vector put the combatant (set, not added to x and y)
    int priority = -1;  // the sprite tile priority a script gave it (0 to 3), -1 for none
    bool animPalette = false;  // it is drawn with the palette of the animation (the game's palette 3), not its own
    bool imp = false, vanish = false;  // a script turned it into an imp (the status toggled) or made it vanish
    int angle = 0;  // how far a character ran along the line of the party, in 256ths of a turn (it runs by 4 at a time)
    int rowAngle = 0;  // how far a character has moved toward the other row (angle units, by the speeds of the game's table)
    int rowSteps = 0;  // the steps it took
    bool rowFlip = false;  // a script toggled the row of the character: the next steps go back the other way
    bool turnAround = false;  // the character faces the other way
    uint32_t statusXor = 0;  // statuses (the bits of the game's four status bytes) that a script toggled for the picture
    bool flipX = false, flipY = false;  // a monster turned over by a script
};

// A character that leaves echoes of its last positions (the trail of the quadra slam): which, and how many frames apart.
struct AnimEcho {
    bool on = false;
    int slot = -1, mask = 0;  // the echo takes a new position when the frame counter and the mask are 0
};

// The scroll of a layer of background, line by line (the HDMA tables of the game): added to the position of the layer on each of the
// 151 lines of the field, horizontally and vertically.
struct AnimLineScroll {
    bool active = false;
    int h[FF6_ANIM_SCROLL_LINES] = {}, v[FF6_ANIM_SCROLL_LINES] = {};
};

// A thread of a scene of a battle event: the script (an address of bank D0) of a character, and who it moves.
struct SceneThread {
    int script = 0;
    AnimBody body;
};
// An attack animation of a scene: the attack of the table, who casts it and who it reaches.
struct SceneAttack {
    int attack = 0;
    AnimBody attacker;
    std::vector<AnimBody> targets;
};

// The waves that scroll a background layer line by line (the HDMA of the game): an amplitude, a phase and the speed
// of the phase, for the horizontal scroll and for the vertical one.
struct AnimWave {
    bool active = false, horizontal = false, vertical = false;
    int hAmplitude = 0, vAmplitude = 0, hFrequency = 0, vFrequency = 0;
    int hPhase = 0, vPhase = 0, hShown = 0, vShown = 0;
};

// The colors of an animation that a script turns: the first one, how many, and by how many places.
struct AnimCycle {
    int first = 0, count = 0, steps = 0;
};

// How a layer is put over what is under it: added, taken away, or normally; by half or not.
struct AnimBlend {
    bool math = false, subtract = false, half = false;
};

// The groups of colors that the palette commands change.
enum AnimTintGroup { kTintBg3, kTintField, kTintBg1, kTintSprites, kTintMonsters, kTintCharacters, kTintGroups };

// The fixed color that the HDMA of the gradients gives to a scanline: each plane from 0 to 31.
struct AnimFixedColor {
    int r = 0, g = 0, b = 0;
};

// The layer (0 sprites, 1 bg1) whose sheet of graphics is the picture of an esper, which the init function of its animation loads,
// and the order of the esper; -1 for none.
struct AnimEsperSheet {
    int layer = -1, esper = 0;
};

// The picture that mode 7 shows for an attack, a plane of 256 x 256 pixels: from a set of tiles and a map of the ROM, or from the
// picture of an esper that stands from a row of tiles (the one of the crusader has a part of it moved).
struct AnimPlane {
    enum Kind { kNone, kTiles, kEsper } kind = kNone;
    int tileSet = 0, mapSet = 0, palette = 0;
    int esper = 0, row = 0;
    bool crusader = false;
};

// What the scripts do to mode 7: the matrix that turns the screen into the plane (1.0 is 0x100) and its center, the scroll of
// bg1 and the flips of the screen.
struct AnimMode7 {
    bool on = false;
    int a = 0x100, b = 0, c = 0, d = 0x100, x = 0, y = 0, scrollX = 0, scrollY = 0;
    bool flipX = false, flipY = false;
};

// The sprite scripts of an attack animation, run as the game does: a thread for every target (and its copies), each with
// its own script pointer, position and loops, one step per frame. The commands that move the background layers or cast
// a spell of their own (vectors, triangles) are skipped, with their length.
class AttackAnimPlayer {
public:
    explicit AttackAnimPlayer(const AttackAnim& anim) : anim_(&anim) {}

    void setMagitek(bool on) { magitek_ = on; }
    // `flipped` is whether the attacker faces right (forward is then to the right); `battleType` picks the jumps of
    // scripts that differ by the type of battle.
    void start(int attack, const AnimBody& attacker, const std::vector<AnimBody>& targets, bool flipped, int battleType = 0);
    // The blow of a weapon: its script in the hand of the attacker, and the effect of the hit on the target unless it
    // missed. A monster attacks with a script of its own.
    void startWeapon(const WeaponAnimProp& weapon, const AnimBody& attacker, const AnimBody& target, bool flipped, bool leftHand,
                     bool miss);
    // The animations of the scene of a battle event that go on together: the characters walk and pose by their scripts, and the
    // attacks of the scene play over them.
    void startScene(const std::vector<SceneThread>& threads, const std::vector<SceneAttack>& attacks, bool flipped, int battleType = 0);
    // What the scripts of a scene asked of the screen: sound commands (the three bytes of the SPC command), a new picture for a
    // character (its actor and the number of its graphics), and how the field shakes (-1 when nothing was asked).
    std::vector<std::array<int, 3>> takeSoundCommands();
    std::vector<std::pair<int, int>> takeGfxChanges();
    // The characters that a scene hid stay hidden for the animations that follow, until another scene shows them.
    void presetHidden(const bool hidden[4]) {
        for (int slot = 0; slot < 4; ++slot) shifts_[slot].hidden = hidden[slot];
    }
    int takeShake() {
        const int value = shake_;
        shake_ = -1;
        return value;
    }
    // The palette of sparkles that a script of a scene gave to the frozen esper, or -1 when it did not ask since the last time.
    int takeEventPalette() {
        const int value = eventPalette_;
        eventPalette_ = -1;
        return value;
    }
    // One frame; false when every thread is over.
    bool tick();
    bool done() const { return done_; }
    // Whether a thread of a layer (0 sprites, 1 bg1, 2 bg3) is still running.
    bool layerActive(int layer) const {
        for (const Thread& thread : threads_) {
            if (thread.active && thread.layer == layer) return true;
        }
        return false;
    }
    void stop() {
        done_ = true;
        sprites_.clear();
    }
    int frames() const { return frames_; }
    int attack() const { return attack_; }
    int sprite() const { return sprite_; }
    int palette() const { return palette_; }
    const std::vector<AnimSprite>& sprites() const { return sprites_; }
    const AnimTint& tint(int group) const { return tints_[group]; }
    // The waves of a layer of background (1 or 2).
    const AnimWave& wave(int layer) const { return waves_[layer]; }
    const AnimBlend& blend(int layer) const { return blends_[layer]; }
    // The scroll line by line of a layer of background (1 or 2), which the random scrolls, the quasar, the moon song and the
    // seizing tentacles set.
    const AnimLineScroll& lineScroll(int layer) const { return lines_[layer]; }
    // The palettes of the field that Chadarnook's lights loaded (3 of 16 colors, BGR555 words), or null when no script loaded them.
    const uint8_t* lightsPalette() const {
        return lights_ < 0 || anim_->lights.size() < FF6_ATTACK_LIGHTS_BYTES ? nullptr : &anim_->lights[static_cast<size_t>(lights_) * (FF6_ATTACK_LIGHTS_BYTES / 2)];
    }
    int lights() const { return lights_; }
    const AnimEcho& echo() const { return echo_; }
    // The faces that the three dice show, which the animations of the dice read ($80/$73).
    void setDice(int first, int second, int third) { dice_[0] = first, dice_[1] = second, dice_[2] = third; }
    // The layers of the subscreen that the color math adds to a layer: 0x10 sprites (monsters and characters), 0x02 the field.
    int subscreen() const { return subscreen_; }
    // Whether the pixel of a layer of background shows through the windows of the animation (the circle).
    bool layerWindowed(int layer) const { return windowNibble(layer) & 0x0A; }
    bool layerVisible(int layer, int x, int y) const;
    // The colors of the palette of a layer, turned: the argument of AttackAnim::frame, or 0.
    int cycle(int layer) const {
        const AnimCycle& turn = cycles_[layer];
        if (turn.count < 2) return 0;
        const int steps = ((turn.steps % turn.count) + turn.count) % turn.count;
        return steps == 0 ? 0 : steps * 256 + turn.first * 16 + turn.count;
    }
    // Where a combatant stands for the screen while the animation plays.
    const AnimShift& shift(bool monster, int slot) const { return shifts_[(monster ? 4 : 0) + slot]; }
    // Mode 7: whether the screen is in it, what it shows, how, and the layers and the priority of the sprites it goes with.
    const AnimMode7& mode7() const { return mode7_; }
    const AnimPlane& plane() const { return plane_; }
    const AnimEsperSheet& esperSheet() const { return esperSheet_; }
    bool bg1Shown() const { return (mainLayers_ & 0x01) != 0; }
    // The OAM priority of the combatants: from 2 they are in front of bg1 in mode 7, below that behind it.
    int spritePriority() const { return spritePriority_; }
    // Whether the tiles of bg3 are over everything (the usual), or behind the field, where bg3 only shows through the color math.
    bool bg3High() const { return bg3High_; }
    // The line under which a combatant is in front of bg1; the ones above it are behind it. 0 when the order is not set.
    int behindBg1Limit() const { return behindLimit_; }
    // The screen mosaic register that a script set ($2106: the size in the high nibble, the layers in the low one), and how many
    // steps of brightness the scripts took away.
    int mosaic() const { return mosaic_; }
    int dimmed() const { return brightness_; }
    // How far a script scrolled the field (bg2), in pixels.
    int fieldScrollX() const { return fieldScrollX_; }
    int fieldScrollY() const { return fieldScrollY_; }
    // The color math with the fixed color of each scanline (the gradients): the CGADSUB bits of the layers it reaches (0x20 the
    // backdrop, 0x02 bg2, 0x80 takes it away, 0x40 halves), or 0.
    int fixedMath() const { return fixedMath_; }
    const AnimFixedColor& fixedColor(int line) const { return fixed_[line & 0xFF]; }
    // The sound effects the scripts asked for since the last call.
    std::vector<int> takeSounds();
    // How many commands were skipped, by their code (a second byte makes it 0x100 | code for the commands of $80).
    const std::vector<std::pair<int, int>>& skipped() const { return skipped_; }

private:
    struct Thread {
        bool active = false, spriteOn = false;
        int layer = 0;  // 0 sprites, 1 bg1, 2 bg3
        int sheet = -1, palette = 0;  // the script whose graphics the frames come from, and their palette
        int index = 0;  // among the copies of a target
        int esper = -1;  // the picture of an esper that is the sheet of the thread
        AnimBody target;
        size_t ptr = 0, loopPtr = 0, returnPtr = 0;
        int counter = 1, rate = 1, loopCount = 0, frameOffset = 0, autoCounter = 1;
        int posX = 0, posY = 0, offsetX = 0, offsetY = 0, targetX = 0, targetY = 0, attackerX = 0, attackerY = 0;
        bool flipped = false;
        double vecX = 0, vecY = 0;  // the direction from the attacker to the target, and how far the thread has gone along it
        int vecLength = 0, vecPos = 0;
        int polarRadius = 0, polarAngle = 0;
        int cycleWait = 0;  // the runs of the loop that go by before the colors turn again
        int spriteFrame = 0, spriteX = 0, spriteY = 0, spriteLayer = 0, spriteSheet = -1, spritePalette = 0;
        int priority = 3;  // of the sprites of the thread, from $90
        bool spriteFlipped = false;
        bool back = false;  // the sprites of the thread go behind the combatants
        int waveStep = 0, waveHigh = 0;  // the counters of the sine waves: $80/$5D, and $80/$31 and $63
        int steerAngle = 0;  // the heading of the commands that steer toward the target ($ED), of 256 to the turn
        AnimBody attacker;  // who the commands that name the attacker move
    };

    void run(Thread& thread);
    void begin(const AnimBody& attacker, bool flipped);
    void addThread(int layer, int script, int palette, const AnimBody& target, int counter, int index);
    void addAttack(int attack, const AnimBody& attacker, const std::vector<AnimBody>& targets);
    void addSceneThread(int address, const AnimBody& body);
    void commandScene(Thread& thread, const uint8_t* code, size_t at);
    void syncChar(int slot);
    void updatePose(int slot);
    void pickAction(const Thread& thread, bool arms);
    void command(Thread& thread, const uint8_t* code, size_t at);
    void special(Thread& thread, const uint8_t* code, size_t at);
    void move(Thread& thread, int argument);
    void showFrame(Thread& thread, int frame);
    void place(const AnimBody& body, bool isTarget, int align, int halfHeight, bool flipped, int& x, int& y) const;
    void skip(int code);
    void setBlends(int cgadsub);
    void updateCircle();
    void rotateMode7();
    void scrollBg1();
    void gradient(int sub, int argument);
    int windowNibble(int layer) const { return layer == 1 && w12_ >= 0 ? w12_ & 0x0F : layer == 2 && w34_ >= 0 ? w34_ & 0x0F : 0; }
    bool palette(Thread& thread, int op, int argument);
    void shiftBody(const AnimBody& body, int dx, int dy);
    void hideBody(const AnimBody& body, bool hidden);
    void stepRow(const AnimBody& body);
    void loadSpritePalette(int palette);
    void setVector(Thread& thread, double fromX, double fromY, double toX, double toY);
    bool jumpAlongVector(Thread& thread, bool forward, int speed);
    void updateTriangle(bool threeD);
    void fillTriangle(const int xs[3], const int ys[3]);
    // Rand: the next number of the table of random numbers of the game (0 to 255), and the numbers at a place of the table.
    uint32_t nextRandom() { return anim_->rng.empty() ? 0 : anim_->rng[randomAt_++ & 0xFF]; }
    int rngAt(int index) const { return anim_->rng.empty() ? 0 : anim_->rng[static_cast<size_t>(index) & 0xFF]; }
    int rngWord(int index) const { return rngAt(index) | (rngAt(index + 1) << 8); }

    uint32_t randomAt_ = 0;

    const AttackAnim* anim_;
    int attack_ = -1, sprite_ = -1, palette_ = 0, sfx_ = 0, battleType_ = 0, frames_ = 0;
    int sheet_[3] = {-1, -1, -1}, palettes_[3] = {0, 0, 0};  // the script of the graphics and the palette of each layer
    AnimBody attacker_;
    bool flipped_ = false, done_ = true, miss_ = false;
    bool paused_[3] = {false, false, false};  // the layers whose threads wait for $C2 (the beams: sprites and bg3)
    bool magitek_ = false;  // the party rides magitek armors (wMagitekModeEnabled)
    bool hidden_[3] = {false, false, false};  // the layers of the backgrounds that the script hid
    AnimTint tints_[kTintGroups];
    AnimWave waves_[3];
    AnimBlend blends_[3];
    AnimCycle cycles_[3];
    // The circle of the scripts, and the window 2 that it makes: for each line, the first and the last column inside it.
    struct Circle {
        int x = 0, y = 0, size = 0, finalX = 0, finalY = 0, finalSize = 0, speed = 0;
    } circle_;
    int w12_ = -1, w34_ = -1, shape_ = 0, subscreen_ = 0, circleAngle_ = 0;
    int spanLeft_[256], spanRight_[256];
    int triX_ = 0, triY_ = 0, triRadius_ = 0, triAngle_ = 0;  // the triangle that window 2 takes the shape of
    int steerMargin_ = 8, steerCount_ = 0;  // how far the heading of $ED may turn at once, and the counter that makes it grow
    int mosaic_ = 0, brightness_ = 0;  // the screen mosaic register and the brightness that scripts lowered
    int fieldScrollX_ = 0, fieldScrollY_ = 0;  // where $D6 scrolled the field (bg2)
    AnimLineScroll lines_[3];
    int quasarStep_ = 1, moonStep_ = 0x80, chadarnook_ = -1;  // the counters of the quasar and the moon song, the monster that came in
    int mindFirst_[32] = {}, mindSecond_[32] = {};  // the two bands of 32 lines of bg1 that the mind blast ripples
    AnimEcho echo_;
    int dice_[3] = {0, 0, 0};
    bool charOnly_ = false;  // a character that moves along a vector does not take the thread with it ($80/$1B)
    int tornadoAngleX_ = 0, tornadoAngleY_ = 0, tornadoAmp_ = 0, tornadoAmpY_ = 0, tornadoX_ = 0, tornadoY_ = 0;
    int tornadoTable_[FF6_ANIM_TORNADO_LINES] = {};  // the sine that bg1 scrolls by, round and round
    int tornadoShift_ = 0;  // where the lines at the top of the field are in it
    bool tornadoTableOn_ = false;
    int lights_ = -1;  // the palette of the field of Chadarnook that a script loaded: 0 lights on, 1 lights off, -1 none
    AnimShift shifts_[4 + 6];  // the characters, then the monsters
    // Where the characters of a scene stand: the position, the offset that the vectors add, the one that Save keeps, and where
    // the scene found them (what shifts_ is counted from).
    struct CharMove {
        bool known = false;
        int posX = 0, posY = 0, offX = 0, offY = 0, savedX = 0, savedY = 0, startX = 0, startY = 0;
    } charMove_[4];
    std::vector<std::array<int, 3>> soundCommands_;
    std::vector<std::pair<int, int>> gfxChanges_;
    int shake_ = -1, maxFrames_ = 0;
    int eventPalette_ = -1;
    AnimMode7 mode7_;
    AnimPlane plane_;
    AnimEsperSheet esperSheet_;
    int mainLayers_ = 0x17, spritePriority_ = 3, rotateScale_ = 0xFF;
    bool bg3High_ = true;
    int behindLimit_ = 0;
    bool veilBg3_ = false;  // the frames of bg1 and bg3 are those of Justice
    int bg1Base_[2] = {0, 0}, bg1Cur_[2] = {0, 0};  // where bg1 was set to start, and where its thread is: their gap is the scroll
    int bg3Shift_[2] = {0, 0};  // the same for bg3
    int bg1Shift_[2] = {0, 0};  // how far the start of bg1 is from half its picture, which moves the picture from its thread
    AnimFixedColor fixed_[256];
    int fixedMath_ = 0, gradientLevel_ = 0, gradientCounter_ = 0;  // the intensity of the gradient and the turn that scrolls it
    std::vector<Thread> threads_;
    std::vector<AnimSprite> sprites_;
    std::vector<int> sounds_;
    std::vector<std::pair<int, int>> skipped_;
};

}  // namespace ff6
