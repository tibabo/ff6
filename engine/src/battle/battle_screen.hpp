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

enum class BattleMode { Fighting, Command, Magic, Item, Tech, Target, Result, Done, Slot };

// The battle as text in windows: the monsters by name, the party with its hp and gauges, the command of the character
// that is ready, its spells and items, and the messages of what happens.
class BattleScreen {
public:
    BattleScreen(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound) {}

    // `battleSpeed` is the raw value of the config; `wait` stops the gauges while a command is chosen; `background` is
    // the number of the battlefield behind the monsters, -1 for none.
    void start(const BattleData& data, const GameRules& rules, PartyState& party, const std::vector<int>& characters,
        int battle, uint32_t seed, int battleSpeed, bool wait, int background = -1, bool veldt = false,
        const Battle::Colosseum& colosseum = Battle::Colosseum());
    // One frame of the game.
    void update(const InputState& input);
    void draw();
    // The graphics that each character of the story wears on the field (the battle draws the same ones), -1 for its own.
    void setCharacterGfx(const std::array<int, FF6_CHAR_COUNT>& gfx) { characterGfx_ = gfx; }
    // The graphics to draw the battle with; without them it is text only.
    void setGfx(const BattleGfx* gfx) { gfx_ = gfx; }
    // The animations of the attacks: spells and techniques play as the game scripts them; without them, by hand.
    void setAttackAnim(const AttackAnim* anim) {
        anim_ = anim;
        player_.reset();
        if (anim) player_.emplace(*anim);
    }
    // Whether an animation of an attack is playing: the messages wait for it.
    bool animating() const { return castLead_ > 0 || (player_ && !player_->done()) || esperGfx_ >= 0; }
    // Plays the animation of an attack from the first monster to the characters, for looking at it.
    void playAnimation(int attack);
    // Plays a battle event (F7) of the scenes of the game, for captures.
    void playScene(int event) {
        if (battle_) battle_->triggerScene(event);
    }

    bool active() const { return battle_.has_value(); }
    bool finished() const { return mode_ == BattleMode::Done; }
    BattleMode mode() const { return mode_; }
    BattleOutcome outcome() const { return battle_ ? battle_->outcome() : BattleOutcome::Fighting; }
    const Battle* battle() const { return battle_ ? &*battle_ : nullptr; }
    void setStory(const Battle::Story& story) {
        if (battle_) battle_->setStory(story);
    }
    int command() const { return command_; }
    int target() const { return target_; }
    // The combatants that the cursor of the target covers.
    std::vector<int> selection() const;
    // The line of the message window, as codes of the font; empty when there is none.
    const std::vector<uint8_t>& message() const { return current_; }
    // The combatant over which the damage of the message is shown, and the damage; -1 when there is none.
    int popupTarget() const { return popupTarget_; }
    int popupDamage() const { return popupDamage_; }
    // How many numbers of damage are jumping over the combatants (the game keeps four at most).
    size_t numeralCount() const { return numerals_.size(); }
    // Whether everything that happened has been shown: no message, animation or number is left to come.
    bool resolved() const { return messages_.empty() && messageTimer_ == 0 && pendingPopup_.target < 0 && !animating(); }
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
    bool knowsRage() const;
    void openRage();
    void openDance();
    void updateSlot(const InputState& input);
    void drawSlot();
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
    bool mirrored(size_t monster, int width) const;
    int monsterX(size_t monster, int width) const;
    // The picture of a monster of the formation; Gau, who jumps in after a battle of the veldt, is a character facing the party.
    const BattleImage& monsterImage(size_t slot) const;
    mutable BattleImage sceneImage_;
    int monsterTop(size_t monster) const;
    int characterTop(int slot) const;
    int characterX(int slot) const;
    bool facesRight(int slot) const;
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
    bool rageList_ = false, danceList_ = false;  // the list is the rages that Gau knows, or the dances that Mog knows
    std::array<int, 3> reels_{}, reelSpeed_{};  // the icons that the slot shows, and how fast each one turns
    int slotStopped_ = 0, slotFrame_ = 0;  // the reels that were stopped, and a counter of frames
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
        enum class Effect { None, Cast, Strike, Defend, Hurt, Fade, Heal, Escape, Animate, Scene, Visible } effect = Effect::None;
        int actor = -1, subject = -1, element = 0;
        bool flash = false;
        int hpAfter = -1;  // the hit points that the target has once the damage is shown
        int attack = -1;  // the attack whose animation plays when the message comes, on these combatants
        int sfx = -1;  // a sound effect that plays when the message comes
        std::vector<int> animTargets{};
        std::vector<SceneThread> sceneThreads{};  // the scripts of the characters of a scene of a battle event, and its attacks
        std::vector<SceneAttack> sceneAttacks{};
        bool leftHand = false;  // a blow: the weapon is the one in the left hand; `attack` is its animation
        bool instant = false;  // it has no text and takes no time: the next message comes at once
        bool moves = false;  // the characters stay where the animation leaves them, in the pose that it gives them (as after a scene)
        int wait = -1;  // how many frames it stays once its number shows, -1 for the usual time
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
    std::vector<uint8_t> frontPixels_;  // the combatants that a script put in front of bg1
    bool frontShown_ = false;
    std::array<std::vector<uint8_t>, 3> layerPixels_;  // the layers of background of the animation, apart (1 and 2)
    void tickEffects();
    int castLead_ = 0;  // frames left before the animation of a spell starts, while its caster gets ready
    Message castPending_;
    std::array<int, FF6_BATTLE_COMBATANTS> poseTimer_{}, poseValue_{}, stepTimer_{}, blinkTimer_{}, fadeTimer_{}, escapeTimer_{};
    std::array<bool, FF6_BATTLE_COMBATANTS> dying_{};  // a monster that fell stays until its message fades it
    int flashTimer_ = 0, flashColor_[3] = {255, 255, 255};
    int animation_ = 0;  // frames, for what moves by itself
    int scrollX_ = 0, scrollY_ = 0;  // where the field has scrolled to: the clouds, the waterfall and the trains move
    AnimShift keptShift_[FF6_BATTLE_COMBATANTS];  // how far the animations of the scripts moved a monster for good
    // The echoes of a character (the trail of the quadra slam): where and how it was at the last 8 times that a script asked to keep it.
    struct EchoFrame {
        int x = 0, y = 0, width = 0, height = 0;
        std::vector<uint8_t> rgba;
    };
    std::array<EchoFrame, 8> echoHistory_;
    int persistSlot_ = -1;  // the monster whose animation is playing and moves it for good
    AnimShift persistLast_;
    void flushPersist();
    // The scenes of the battle events (F7): the dialogs are messages, the animations of the characters play between them.
    void queueScene(int event);
    void queueDialog(int dialog, std::string& speaker);
    void startScene(const Message& message);
    void flushScene();
    bool sceneActive_ = false;  // an animation of a scene is playing: the characters it moves stay where it leaves them
    int keptPose_[FF6_BATTLE_PARTY_SLOTS] = {-1, -1, -1, -1};  // the pose that a scene left a character in
    bool keptFlip_[FF6_BATTLE_PARTY_SLOTS] = {false, false, false, false};
    int gfxOverride_[FF6_BATTLE_PARTY_SLOTS] = {-1, -1, -1, -1};  // the picture that a scene gave to a character
    int bgShake_ = 0;  // the field shakes (a scene asked, 0x80 for the horizontal shake only)
    int eventPalette_ = -1;  // the sparkles that a scene gave the first monster, -1 for its own colors
    bool keptHidden_[FF6_BATTLE_PARTY_SLOTS] = {false, false, false, false};  // the characters that a scene took off the field
    bool sceneControlled_[FF6_BATTLE_PARTY_SLOTS] = {false, false, false, false};  // a scene decides whether they are seen
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
    std::array<int, FF6_CHAR_COUNT> characterGfx_ = [] {
        std::array<int, FF6_CHAR_COUNT> none;
        none.fill(-1);
        return none;
    }();
    int popupTarget_ = -1, popupDamage_ = -1;
    // The damage and the hit points show when the animation of the blow is over, not when the battle has worked them out.
    struct PendingPopup {
        int target = -1, damage = -1, hp = -1, wait = -1;
        bool green = false;  // the number is a recovery
    } pendingPopup_;
    // The numbers of damage that show over the combatants, as sprites that jump and fall (the game keeps four at most).
    struct Numeral {
        int target = -1, value = 0, frame = 0;  // a negative value is the word Miss
        bool green = false;
    };
    std::vector<Numeral> numerals_;
    void addNumeral(int target, int damage, bool green);
    std::array<int, FF6_BATTLE_COMBATANTS> prevHp_{}, heldHp_{}, queuedHp_{};  // before the action, shown until its message, after the queued messages
    int shownHp(int combatant) const;
    bool monsterListed(int combatant) const;
    bool attackFlips(const AnimBody& attacker, const AnimBody& target) const;  // whether the attacker faces right
    bool magitekMode() const;  // anyone rides an armor: the whole party does (wMagitekModeEnabled)
    void revealPopup();
    int advanceHp(int target, int delta);
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
