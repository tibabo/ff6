#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "constants.hpp"
#include "data/battle_data.hpp"
#include "data/game_rules.hpp"
#include "state/party_state.hpp"

namespace ff6 {

// The random numbers of a battle. With the table of the game (RNGTbl, Rand and RandA), each number is the next byte of the table
// from an index that the seed starts; without it, xorshift, so that a test can repeat a battle.
class BattleRandom {
public:
    explicit BattleRandom(uint32_t seed, const std::vector<uint8_t>* table = nullptr)
        : state_(seed ? seed : 1), table_(table && table->size() >= FF6_RNG_BYTES ? table : nullptr), index_(static_cast<uint8_t>(seed)) {}
    int below(int limit);  // 0 to limit - 1
    int byte() { return table_ ? (*table_)[++index_] : below(256); }
    // BattleLoop: the index goes one further at each turn of the main loop of the battle.
    void step() { ++index_; }

private:
    uint32_t state_;
    const std::vector<uint8_t>* table_;
    uint8_t index_;
};

// The formulas, apart from the battle so that they can be tested.
// InvertEvade: what an evade or magic block of `percent` leaves of a hit.
int invertEvade(int percent);
// CalcSpeed: how much the gauge of a combatant grows at each tick; the monsters are slowed by the battle speed (0 to 5).
int atbConstant(int speed, bool monster, int battleSpeed, bool slow = false, bool haste = false);
// CalcDmg, ApplyDmgMult and CalcDmgMod of a physical attack: `power` is the battle power of the weapon or the attack of
// the monster, `strength` the vigor times two, `random` a byte for the variance of the damage.
int physicalDamage(int power, int strength, int level, bool monster, int defense, int random, bool twoHand = false,
    bool critical = false, bool halve = false, bool genji = false, int extra = 0);  // the offering halves the damage, the genji glove takes a quarter
// ChooseBattleType: the type (FF6_BATTLE_TYPE_*) from the flags of the battle, the allies alive and a random number. The back
// guard puts the pincer attacks and the back attacks aside when another type is possible.
int chooseBattleType(int flags, int alliesAlive, BattleRandom& random, bool backGuard = false);

// An attack as the battle sees it: a row of the table of attacks (a spell, a special attack of a monster), or what a
// consumable item does (CalcItemEffect).
struct Attack {
    int target = 0, element = 0;
    unsigned flags = 0;  // FF6_AF_*
    int mp = 0, power = 0, misc = 0, hit = 0, special = 0;
    uint32_t status = 0;  // FF6_ST_*
    int bonus = 0;  // half-steps of extra damage: 2 doubles it
    bool fixed = false;  // the damage is the one that was worked out, without the variance, the defense and the elements
    bool item = false;  // a consumable: it has no level
    bool has(unsigned flag) const { return (flags & flag) != 0; }
};
Attack attackFromProp(const uint8_t* prop);
Attack attackFromItem(const uint8_t* itemProp);

// A hand attacks with its own weapon.
struct Hand {
    int power = 0, hitRate = 0, element = 0;
    bool twoHand = false, longRange = false;
    int special = 0;  // the special effect of the weapon (FF6 ATTACK_SPECIAL_EFFECT)
    int spell = -1;  // the spell that it sometimes casts after a blow
};

// The sprite sheet of a character of the party: the first fourteen have their own, Leo and Banon come after the imp.
inline int characterSprite(int character) { return character < 14 ? character : FF6_CHAR_GFX_LEO + character - 14; }

struct Combatant {
    bool present = false;
    bool monster = false;
    bool escaped = false;  // a character that ran away
    int id = 0;  // the character (the number in the party) or the monster
    int level = 1;
    int hp = 0, maxHp = 0, mp = 0, maxMp = 0;
    int speed = 0, strength = 0, magicPower = 0, stamina = 0;
    std::array<Hand, 2> hands{};  // a monster attacks with the first
    int evade = 0, defense = 0, magicDefense = 0, magicBlock = 0;  // the percents of the evade and the block
    int atbConstant = 0;
    int gauge = 0;  // FF6_ATB_FULL when it is full
    bool ready = false;  // a character that waits for its command
    int pace = 0, paceConstant = FF6_ATB_NORMAL;  // the counter that decides when a character that runs may leave
    int exp = 0, gil = 0;  // what a monster gives
    uint32_t status = 0;  // FF6_ST_*
    uint32_t blocked = 0;  // the status that cannot be set
    int elemAbsorb = 0, elemNull = 0, elemWeak = 0, elemHalf = 0, monsterFlags = 0;
    bool defending = false;
    bool runic = false;  // absorbs the magic that is aimed at it
    int morph = 0;  // ticks of the morph that are left
    bool backRow = false;  // takes and deals half of the blows, but for the weapons that reach
    int relic1 = 0, relic2 = 0, relic3 = 0, relic4 = 0, relic5 = 0;  // the effects of the relics that count in a battle
    std::array<int, 2> stealItems{FF6_ITEM_EMPTY, FF6_ITEM_EMPTY};  // what a monster can be robbed of: rare, then common
    bool invincible = false;  // a script made it immune to damage
    bool noTarget = false;  // a script made it impossible to aim at
    // A character of a scene acts by the script of a monster (`aiId`), may fight for the monsters (it then stands among them, a
    // monster whose stats are the character's), has the sprite `gfx` and a place on the screen, and waits out of the battle
    // (`offstage`) until a scene brings it.
    int aiId = -1, charIndex = -1, gfx = -1, placeX = -1, placeY = -1;
    bool offstage = false;
    bool retort = false;  // Cyan's stance: he answers the blows of the monsters
    bool turnPending = false;  // its gauge is full and its turn waits in the queue of the actions
    bool acting = false;  // its action is being shown
    bool desperationUsed = false, lowHpDone = false;  // once in a battle: a desperation attack, the spells of the relics at low hp
    int rageMonster = -1, danceIndex = -1;  // the monster that Gau became, the dance that Mog dances
    int seizeTarget = -1, seizeAttacker = -1, charmTarget = -1, charmAttacker = -1;  // who it holds, who holds it; who it charmed, who charmed it
    bool phantasm = false;  // it loses hp as if it were in sap
    bool overcast = false;  // when it falls it turns into a zombie instead
    int loveTarget = -1, loveAttacker = -1, targettingTarget = -1;  // who covers it (a love token), whom it covers; whom it marked
    int sleepCounter = 0, stopCounter = 0, reflectCounter = 0, freezeCounter = 0, condemned = 0, dot = 0;  // paces left
    // The script of a monster (AI): where it starts, the retaliation script, where a wait left it, how many conditions had
    // failed then, its timer and variable, and what hit it last.
    int aiStart = -1, aiCounter = -1, aiLoop = 0, aiCount = FF6_AI_NO_LOOP, timer = 0, variable = 0;
    int retalAttacker = -1, retalCommand = -1, retalAttack = -1, retalItem = -1, retalElement = 0;
    bool alive() const { return present && !escaped && !offstage && hp > 0 && !(status & FF6_ST_GONE) && !(monster && (status & FF6_ST_HIDE)); }
    bool undead() const { return (monsterFlags & FF6_MONSTER_FLAG_UNDEAD) != 0 || (status & FF6_ST_ZOMBIE) != 0; }
};

enum class BattleOutcome { Fighting, Won, Lost, Fled };

struct BattleEvent {
    enum class Kind {
        Hit, Miss, Critical, Defeated, Escaped,
        Cast, UseItem, Defend,  // the attacker starts an attack: `attack` is the spell, `item` the item
        Damage, Heal, MpDamage, MpHeal,  // what a spell or item does to the target
        StatusSet, StatusCleared,  // `status` has the bits that changed
        NoEffect, NotEnoughMp, Reflected,
        Stole, StealFailed, ChangedRow,  // `item` is what was taken; `damage` is 1 for the back row
        Poison, Regen, Sap,  // damage or healing from status, `damage` and the target
        Runic,  // the attacker takes the Runic stance
        GilToss,  // the attacker throws gil
        Possess,  // the attacker and the target leave the battle
        Learned,  // the target (Strago) learned the lore `attack`
        Dialog,  // the monster `attacker` says the dialog `damage`
        ChangeBattle,  // the monsters are those of the battle `damage`
        Entry,  // the monster `attacker` comes (`damage` is 0, 2 or an esper's...) or goes (1, 3, 4, 5): the mode of FF6 MONSTER_ENTRY_EXIT
        Rage,  // the attacker (Gau) becomes the monster `attack`
        Leap,  // the attacker leaps and learns the rages of the monsters that were there
        Dance,  // the attacker starts the dance `damage`
        DanceFailed,  // the attacker stumbles: the dance does not match the field
        LearnedDance,  // the party learned the dance `damage`
        Background,  // the field changes to the background `damage`
        Slot,  // the reels stopped on `damage` (the result of FF6 GetSlotAttack)
        Umaro,  // the attacker charges (`damage` is FF6_UMARO_MOVE_CHARGE) or throws a character (FF6_UMARO_MOVE_THROW, `target` is the one thrown)
        Scan,  // the target was scanned: the screen shows what it is
        GainLevel,  // the attacker is now at the level `damage`
        LearnedSpell,  // the attacker learned the spell `attack`
        MagicPoints,  // the party earned `damage` magic points
        Scene,  // a battle event (F7) whose dialogs the screen shows: `damage` is its number
        GotItem,  // the attacker got the item `item` (Metamorph)
        Seized,  // the target is held (`damage` 0), charmed (1), let go (2), covered by a love token (3) or clung to (4)
        Forcefield,  // the attacker nullified the element of the bit `damage` for the whole battle
        Interceptor,  // the dog of the target takes the blow of the attacker
        MonsterAnim  // a script moves or lights the monster `target` (the animation `attack`), or plays the sound effect `attack` when `damage` is FF6_MONSTER_ANIM_SFX
    };
    Kind kind = Kind::Miss;
    int attacker = -1, target = -1;  // indices of the combatants
    int damage = 0;
    int attack = -1, item = -1;
    int hand = 0;  // the hand that struck, for Hit, Critical and Miss
    uint32_t status = 0;
};

struct BattleReward {
    long exp = 0;  // for each character that is alive
    long gil = 0;
    std::vector<int> items;
    int magicPoints = 0;  // what the espers and items teach with
};

// A bout of the colosseum: a character alone, who acts by itself, against the monster of the item that was wagered. The prize
// is won with the victory, and the wager is lost either way.
struct ColosseumBout {
    int monster = -1;  // -1 for a normal battle
    int wager = FF6_ITEM_EMPTY, prize = FF6_ITEM_EMPTY;
};

class Battle {
public:
    using Colosseum = ColosseumBout;
    // `party` has the characters (numbers in the party) that fight, at most FF6_BATTLE_PARTY_SLOTS. The state of the
    // party is changed by the battle: hp, and at the end experience, gil and items.
    // `gameRandom`: the random numbers come from the table of the game, the seed being the index of the first one.
    Battle(const BattleData& data, const GameRules& rules, PartyState& party, const std::vector<int>& characters,
        int battle, uint32_t seed, int battleSpeed = FF6_BATTLE_SPEED_DEFAULT, const Colosseum& colosseum = Colosseum(), bool gameRandom = false);
    void stepRandom() { random_.step(); }

    // One tick of the gauges (every FF6_BATTLE_TICK_FRAMES frames); the monsters act when theirs is full.
    void tick();
    // The first character that waits for a command, or -1.
    int readyCharacter() const;
    // With queued turns, a combatant that acts by itself waits with a full gauge until the screen runs its turn (the action
    // queue of the game), instead of acting in the tick.
    void setQueuedTurns(bool queued) { queuedTurns_ = queued; }
    int takePendingTurn();  // the next one whose gauge filled, -1 for none
    void runTurn(int index);
    // While the action of a combatant is shown its gauge does not move; it empties once the action is over (ExecAction).
    void beginAction(int index) { combatants_[static_cast<size_t>(index)].acting = true; }
    void endAction(int index) {
        Combatant& c = combatants_[static_cast<size_t>(index)];
        c.acting = false;
        if (!c.ready && !c.turnPending) c.gauge = 0;
    }
    // The command Fight of a character at a monster (a combatant index): each hand that has a weapon strikes.
    void fight(int attacker, int target);
    // The party runs while the buttons are held: a character that waits for a command leaves at its next turn of the
    // counter, and the battle ends when no character is left.
    void setRunning(bool running) { running_ = running && canRun(); }
    // The command Magic: the character casts a spell of the table of attacks at the combatants. A silenced character, a
    // spell that costs more than its mp or an empty list of targets does nothing (the mp of a spell is lost only when cast).
    void cast(int caster, int spell, const std::vector<int>& targets, bool keepTurn = false);
    // The command Item: one item of the inventory is used on the targets.
    void useItem(int user, int item, const std::vector<int>& targets);
    // The command Defend: the character takes half the damage of physical attacks until its next turn.
    void defend(int user);
    // The command Row: the character goes to the other row.
    void changeRow(int user);
    // The command Steal, at a monster.
    void steal(int thief, int target);
    // Morph doubles the damage that Terra deals for a while; Revert ends it; Runic makes Celes absorb the next magic that
    // is aimed at her, until her next turn.
    void morph(int user);
    void revert(int user);
    void runic(int user);
    // The command Sketch: the character uses one of the two attacks that it draws from the monster, at it.
    void sketch(int user, int target);
    // The command Control: whether the monster obeys. If it does, the character keeps its turn to give it an attack, once.
    bool control(int user, int target);
    // The monster that is controlled uses one of its attacks, at the combatants of `mask`; the turn of the character ends.
    void controlAttack(int user, int attack, uint16_t mask);
    int controlled() const { return controlled_; }
    // The command GP Rain: gil thrown at the monsters. Possess: the monster and the character both leave. Capture: a blow
    // that steals when it hurts.
    void gpRain(int user, const std::vector<int>& targets);
    void possess(int user, int target);
    void capture(int user, int target);
    // The command Throw: a weapon of the inventory is lost to hit a monster for twice its power.
    void throwWeapon(int user, int item, int target);
    // The command Jump: a blow of double damage.
    void jump(int user, int target);
    // The command Mimic: the last Fight, Magic or Item of a character, again.
    void mimic(int user);
    bool canMimic() const { return last_.kind >= 0; }
    // Gau, Mog and Setzer: Rage becomes a monster that the party knows, Leap (on the veldt) learns the rages of the monsters
    // that are there and ends the battle, Dance starts a dance that the field has to allow, Slot is stopped on three icons.
    bool canLeap() const;
    void leap(int user);
    void rage(int user, int monster);
    void dance(int user, int dance);
    void slot(int user, const std::array<int, 3>& reels);
    void setVeldt(bool veldt) { veldt_ = veldt; }
    void setBackground(int background) { background_ = background; }
    int background() const { return background_; }
    // Morph needs a supply of more than FF6_MORPH_MIN_SUPPLY.
    bool canMorph() const { return party_.morphCounter > FF6_MORPH_MIN_SUPPLY; }
    // An esper is summoned once in a battle.
    bool summoned() const { return summoned_; }
    bool canCast(int caster) const;
    // The attack of a spell, and what the character pays for it.
    Attack spellAttack(int spell) const;
    // The combatants that an attack can be aimed at: the ones that stand, or the ones that fell when it brings back.
    bool canTarget(const Attack& attack, int target) const;

    // A script recruited Gau (misc effect 3): the game adds him to the party.
    bool recruitedGau() const { return recruitedGau_; }
    void setGauJoined(bool joined) { gauJoined_ = joined; }
    // What the story says at the end of a battle: whether Gau and Shadow are characters that can be in the party, whether Gau
    // has been won over once (he then comes back by himself), and whether Shadow must stay (the story needs him).
    struct Story {
        bool gauAvailable = true, gauObtained = false, shadowAvailable = false, shadowStays = true, magicOnly = false;
    };
    void setStory(const Story& story) {
        story_ = story;
        variables_[FF6_AI_STORY_VARIABLE] = (story.gauObtained ? FF6_AI_STORY_GAU_OBTAINED : 0) | (story.shadowStays ? FF6_AI_STORY_SHADOW_STAYS : 0);
    }
    bool magicOnly() const { return story_.magicOnly; }
    // A battle event (F7) that the screen plays now, as if a script had asked.
    void triggerScene(int event);
    // What the scene of the battle (the character AI of the game) asks of the screen: no names, another background or song.
    bool hideNames() const { return hideNames_; }
    int sceneBackground() const { return sceneBackground_; }
    int sceneSong() const { return sceneSong_; }
    // Gau leapt away, and Shadow left: the game takes them out of the party.
    bool gauLeaped() const { return gauLeaped_; }
    bool shadowLeft() const { return shadowLeft_; }
    // The battle was won with a victory (WinBattle): the fanfare plays, the rewards were given.
    bool victory() const { return victory_; }
    bool colosseum() const { return colosseum_.monster >= 0; }
    // The party can run when no monster that stands holds it back.
    bool canRun() const;
    BattleOutcome outcome() const { return outcome_; }
    int battleType() const { return type_; }
    bool preemptive() const { return preemptive_; }  // the party strikes first
    const BattleReward& reward() const { return reward_; }
    const std::array<Combatant, FF6_BATTLE_COMBATANTS>& combatants() const { return combatants_; }
    // The events since the last call, for the screen.
    std::vector<BattleEvent> takeEvents();

private:
    struct LastAction {
        int kind = -1;  // 0 Fight, 1 Magic, 2 Item
        int value = 0;
        std::vector<int> targets;
    };
    LastAction last_;
    bool summoned_ = false;
    int blowMultiplier_ = 1;  // a jump hits twice as hard, three times with a spear
    int controlled_ = -1;
    bool sketchHits(const Combatant& user, const Combatant& target, bool boosted);
    void tryStealing(int thief, int target);
    void initGauges();
    void attack(int attacker, int target, int hand);
    bool hits(const Hand& hand, const Combatant& attacker, const Combatant& target);
    int hitChance(int hitRate, const Combatant& attacker, const Combatant& target, bool physical) const;
    int coverFor(int attacker, int target) const;
    void counterBlow(int attacker, int target);
    void lowHpSpells(int index);
    std::vector<int> targetsFor(int user, const Attack& attack, int chosen);
    bool desperation(int attacker, int target);
    void umaroFight(int attacker, int target);
    void runCounters();
    void checkEnd();
    bool gauAppears();
    void shadowLeaves();
    void win();
    void strike(int attacker, int target);
    void blows(int attacker, int target);
    void autoAct(int index);
    void setStatus(int index, uint32_t status, int level = 0);
    void clearStatus(int index, uint32_t status);
    void updateNearFatal(Combatant& c);
    void refreshPace(Combatant& c);
    void syncParty(const Combatant& c);
    void damageStatus(int index, bool heal, BattleEvent::Kind kind);
    void expireCounters(int index);
    void perform(int user, const Attack& attack, std::vector<int> targets, int spell, int item);
    void resolve(int user, const Attack& attack, const std::vector<int>& targets, int base, bool reflected);
    void applyTo(int user, const Attack& attack, int target, int base);
    bool spellHits(const Attack& attack, const Combatant& user, const Combatant& target);
    // The monsters' scripts (ExecAI): a turn, a run of a script, a condition, a target code and an attack of a monster.
    void monsterTurn(int index);
    void initAi(int index);
    void initMonsters(const BattleFormation& formation);
    void loadMonster(size_t at, int monster);
    void fillCharacter(Combatant& c, int index);
    // The scene of a battle (a row of the table of the character AI): who stands in the party, who fights for the monsters, who
    // waits out of the battle, with what script, sprite and place.
    struct SceneSlot {
        int character = -1, script = -1, gfx = -1, x = -1, y = -1;
        bool enemy = false, offstage = false;
    };
    void applyCharAi(const BattleFormation& formation, std::vector<int>& fighters);
    void applyScenePlace(Combatant& c, size_t slot);
    bool readSceneSlot(const uint8_t* slot, SceneSlot& out) const;
    Combatant* placeEnemyChar(const SceneSlot& enemy);
    void placeEnemyChars();
    void sceneEffects(int event);
    void showCharacter(int actor, bool shown);
    int monsterBit(int index) const;
    int scriptId(const Combatant& c) const { return c.aiId >= 0 ? c.aiId : c.monster ? c.id : -1; }
    std::vector<SceneSlot> sceneSlots_, sceneEnemies_;
    bool hideNames_ = false;
    int sceneBackground_ = -1, sceneSong_ = -1;
    int targetMask_ = 0xFF;  // the monsters that can be aimed at (w7e2f46), a bit for each place; the scripts change it
    void aiChangeBattle(int battle, bool restore);
    void aiEntryExit(int index, int mode, uint16_t mask);
    void aiItem(int index, bool throwing, int item, uint16_t mask);
    void runScript(int index, size_t start, bool counter);
    bool aiCondition(int index, int condition, int first, int second, uint16_t& mask);
    uint16_t aiTargets(int index, int code, bool onlyAlive);
    void aiAttack(int index, int attack, uint16_t mask);
    void aiMisc(int index, int effect, int value);
    void noteHit(int user, int target, int command, int attack, int item, int element);
    // The special effects of the attacks (attacker_effect.asm and target_effect.asm): what changes before the targets are hit, and
    // what happens to each of them. TargetEffect returns 0 for a miss, 1 to go on and 2 when everything was done.
    bool specialBefore(int user, Attack& attack, std::vector<int>& targets, int& base, int& rounds);
    int specialTarget(int user, Attack& attack, int target);
    void specialAfter(int user, const Attack& attack);
    void leaves(int index);
    void defeat(int index);
    void releaseHolds(int index);
    void seizeDrain(int index);
    int& aiVariable(int index, int number);

    const BattleData& data_;
    const GameRules& rules_;
    PartyState& party_;
    std::array<Combatant, FF6_BATTLE_COMBATANTS> combatants_;
    BattleRandom random_;
    BattleOutcome outcome_ = BattleOutcome::Fighting;
    BattleReward reward_;
    std::vector<BattleEvent> events_;
    int type_ = FF6_BATTLE_TYPE_NORMAL;
    bool preemptive_ = false;
    bool running_ = false;
    uint8_t counter_ = 0;
    int battleSpeed_ = FF6_BATTLE_SPEED_DEFAULT;
    int battleNumber_ = 0, battleTimer_ = 0;
    std::array<int, FF6_AI_VARIABLES> variables_{};
    int background_ = -1;
    bool veldt_ = false;
    Story story_;
    Colosseum colosseum_;
    void settleWager();
    bool gauLeaped_ = false, shadowLeft_ = false, gauShown_ = false, victory_ = false;
    int forcefield_ = 0;  // the elements that the monsters nullified, for everybody
    int zingerAttacker_ = -1, zingerTarget_ = -1;  // a monster that stays out of the fight until its target falls
    int mindBlastMask_ = 0;  // the targets of the last mind blast that get a status
    void rageTurn(int index);
    void danceAttack(int index);
    void learnRages();
    int quickTarget_ = -1, quickTurns_ = 0;  // the combatant that Quick gives turns to, while the others wait
    bool quickWaiting_ = false;
    int golemHp_ = 0;  // what the golem can still take for the party
    int turns_ = 0;  // the turns that combatants have taken (w7e3a3e)
    bool started_ = false, countering_ = false, holdEnd_ = false, recruitedGau_ = false, gauJoined_ = true;
    bool aiRunning_ = false;  // a script is running: the hits it causes do not start another one
    bool queuedTurns_ = false;
    std::vector<int> pendingTurns_;
};

}  // namespace ff6
