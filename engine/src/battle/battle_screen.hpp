#pragma once

#include <deque>
#include <array>
#include <initializer_list>
#include <optional>
#include <vector>

#include "battle/attack_anim_player.hpp"
#include "battle/battle.hpp"
#include "data/attack_anim.hpp"
#include "data/battle_gfx.hpp"
#include "input.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

enum class BattleMode { Fighting, Command, Magic, Item, Tech, Target, Result, Done };

// The battle as text in windows: the monsters by name, the party with its hp and gauges, the command of the character
// that is ready, its spells and items, and the messages of what happens.
class BattleScreen {
public:
    BattleScreen(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    // `battleSpeed` is the raw value of the config; `wait` stops the gauges while a command is chosen; `background` is
    // the number of the battlefield behind the monsters, -1 for none.
    void start(const BattleData& data, const GameRules& rules, PartyState& party, const std::vector<int>& characters,
        int battle, uint32_t seed, int battleSpeed, bool wait, int background = -1);
    // One frame of the game.
    void update(const InputState& input);
    void draw();
    // The graphics to draw the battle with; without them it is text only.
    void setGfx(const BattleGfx* gfx) { gfx_ = gfx; }
    // The animations of the attacks: spells and techniques play as the game scripts them; without them, by hand.
    void setAttackAnim(const AttackAnim* anim) {
        anim_ = anim;
        player_.reset();
        if (anim) player_.emplace(*anim);
    }
    // Whether an animation of an attack is playing: the messages wait for it.
    bool animating() const { return (player_ && !player_->done()) || esperGfx_ >= 0; }
    // Plays the animation of an attack from the first monster to the characters, for looking at it.
    void playAnimation(int attack);

    bool active() const { return battle_.has_value(); }
    bool finished() const { return mode_ == BattleMode::Done; }
    BattleMode mode() const { return mode_; }
    BattleOutcome outcome() const { return battle_ ? battle_->outcome() : BattleOutcome::Fighting; }
    const Battle* battle() const { return battle_ ? &*battle_ : nullptr; }
    int command() const { return command_; }
    int target() const { return target_; }
    // The combatants that the cursor of the target covers.
    std::vector<int> selection() const;
    // The line of the message window, as codes of the font; empty when there is none.
    const std::vector<uint8_t>& message() const { return current_; }
    // The combatant over which the damage of the message is shown, and the damage; -1 when there is none.
    int popupTarget() const { return popupTarget_; }
    int popupDamage() const { return popupDamage_; }
    // Ends the battle and lets go of it: the party keeps what it won.
    void close();

private:
    using Codes = std::vector<uint8_t>;

    struct Entry {
        Codes name;
        int value = 0;  // the command, the spell or the item
        int amount = -1;  // the cost of a spell, the quantity of an item
        bool enabled = true;
    };
    enum class Pending { Fight, Spell, Item, Steal, Throw, Jump, Sketch, Control, ControlAttack, Capture, Possess, GpRain };

    Codes nameOf(int combatant) const;
    Codes join(std::initializer_list<Codes> parts) const;
    void queue(const Codes& message, int target = -1, int damage = -1);
    void queueWith(const Codes& message, int target, int damage, int effect, int actor, int subject, int element, bool flash);
    void collectEvents();
    void advanceMessage(bool skip);
    void finish();
    bool keepsMenu();
    void updateFighting(const InputState& input);
    void updateCommand(const InputState& input, int direction);
    void updateList(const InputState& input, int direction);
    void updateTarget(const InputState& input, int direction);
    void openCommands();
    void openList(BattleMode mode, bool tools = false);
    bool hasTools() const;
    void openTechs(bool blitz);
    void openLore();
    void openThrow();
    void openSummon();
    void openMagitek();
    void openControl(int monster);
    void addAttack(const Combatant& c, int attack);
    bool hasWeapons() const;
    void beginTarget(Pending pending, int value, const Attack& attack);
    void confirmTarget();
    bool targetable(int combatant) const;
    void moveTarget(int step);
    void switchSide();
    void drawMonsters();
    void drawField();
    void drawPopup();
    void drawParty();
    void drawResult();
    void drawLists();
    void drawHands();
    void put(int x, int y, const Codes& codes, int color);
    // A line of text at a pixel row, which need not be a multiple of a tile.
    void putPx(int x, int yPx, const Codes& codes, int color);
    // The line of a monster in the window at the bottom: the first rows of the names are taller than the last.
    int monsterRowPx(int row) const;
    // Where a monster and a character stand: the battle types that surprise the party turn the sides round.
    bool mirrored(size_t monster) const;
    int monsterX(size_t monster, int width) const;
    int characterX(int slot) const;
    Codes statusName(uint32_t status) const;

    MenuScreen& screen_;
    Sound& sound_;
    const BattleData* data_ = nullptr;
    const BattleGfx* gfx_ = nullptr;
    const AttackAnim* anim_ = nullptr;
    std::optional<AttackAnimPlayer> player_;
    bool animated_ = false;  // the spell that is shown has its animation, which takes the place of the drawn effects
    bool throwList_ = false;  // the list of items is the one of Throw
    int xMagic_ = 0;  // 1 while the first spell of X Magic is chosen, 2 for the second
    bool controlList_ = false;  // the list is the attacks of the monster that is controlled
    int esperGfx_ = -1, esperAge_ = 0, esperShown_ = 0;  // the picture of the esper that is summoned, and how much of it shows
    int esperX_ = 0, esperY_ = 0;
    bool esperPlaced_ = false;  // the esper stands at (esperX_, esperY_) instead of the middle
    bool esperLeaving_ = false;  // the esper fades away instead of coming out of white
    bool esperAfterBolts_ = false;  // the esper waits for the bg3 thread of its animation
    bool esperStands_ = true;  // its picture is shown whole before the animation
    const GameRules* rules_ = nullptr;
    PartyState* party_ = nullptr;
    BattleFormation formation_;
    int background_ = -1;
    std::optional<Battle> battle_;
    BattleMode mode_ = BattleMode::Done;
    struct Message {
        Codes text;
        int target = -1, damage = -1;  // a number shown over the combatant that the message is about
        // What the picture does when the message comes: a character takes a pose, a combatant is hit, the field flashes.
        enum class Effect { None, Cast, Strike, Defend, Hurt, Fade, Heal, Escape } effect = Effect::None;
        int actor = -1, subject = -1, element = 0;
        bool flash = false;
        int attack = -1;  // the attack whose animation plays when the message comes, on these combatants
        std::vector<int> animTargets{};
        bool leftHand = false;  // a blow: the weapon is the one in the left hand; `attack` is its animation
    };
    void trigger(const Message& message);
    void startAnimation(const Message& message);
    void startBlow(const Message& message);
    bool body(int combatant, AnimBody& body) const;
    void drawAnimation();
    // Mode 7: the animation turns a plane (a picture of 256 x 256) into the whole screen, which has no windows and no field then.
    bool mode7Shown() const { return player_ && !player_->done() && player_->mode7().on; }
    void updatePlane();
    std::vector<uint8_t> mode7Pixels() const;
    BattleImage plane_;
    int planeCycle_ = -1;
    std::vector<uint8_t> bgPixels_, objectPixels_, behindPixels_;  // the field, what stands on it, and what bg1 covers; RGBA, field size
    std::array<std::vector<uint8_t>, 3> layerPixels_;  // the layers of background of the animation, apart (1 and 2)
    void tickEffects();
    std::array<int, FF6_BATTLE_COMBATANTS> poseTimer_{}, poseValue_{}, stepTimer_{}, blinkTimer_{}, fadeTimer_{}, escapeTimer_{};
    std::array<bool, FF6_BATTLE_COMBATANTS> dying_{};  // a monster that fell stays until its message fades it
    int flashTimer_ = 0, flashColor_[3] = {255, 255, 255};
    int animation_ = 0;  // frames, for what moves by itself
    // The drawn effects over the targets: a slash, a ring of the element, sparkles that rise.
    struct Fx {
        enum class Kind { Slash, Ring, Sparkle } kind = Kind::Slash;
        int x = 0, y = 0, frame = 0, length = 0;
        int color[3] = {255, 255, 255};
    };
    std::vector<Fx> fx_;
    int lastElement_ = 0;
    bool center(int combatant, int& x, int& y) const;
    void addFx(Fx::Kind kind, int combatant, int length, const int* color);
    void drawFx();
    std::deque<Message> messages_;
    Codes current_;
    int popupTarget_ = -1, popupDamage_ = -1;
    int messageTimer_ = 0;
    int frames_ = 0;
    int active_ = -1, command_ = 0, target_ = FF6_BATTLE_PARTY_SLOTS;
    std::vector<Entry> commands_, list_;
    std::vector<Entry> rowDefend_, run_;  // the windows that left and right of the commands bring
    int page_ = 0, mainCommand_ = 0;  // -1 Row and Defend, 0 the commands, 1 Run
    const std::vector<Entry>& shownCommands() const { return page_ < 0 ? rowDefend_ : page_ > 0 ? run_ : commands_; }
    int listCursor_ = 0, listTop_ = 0;
    BattleMode listMode_ = BattleMode::Magic;
    Pending pending_ = Pending::Fight;
    int pendingValue_ = 0;
    Attack pendingAttack_;
    bool group_ = false, startsOnMonsters_ = true;  // the cursor covers a whole side
    bool running_ = false, wait_ = true, finishing_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
