#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "constants.hpp"
#include "data/battle_data.hpp"
#include "data/game_rules.hpp"
#include "state/party_state.hpp"

namespace ff6 {

// The random numbers of a battle: xorshift, so that a test can repeat a battle.
class BattleRandom {
public:
    explicit BattleRandom(uint32_t seed) : state_(seed ? seed : 1) {}
    int below(int limit);  // 0 to limit - 1
    int byte() { return below(256); }

private:
    uint32_t state_;
};

// The formulas, apart from the battle so that they can be tested.
// InvertEvade: what an evade or magic block of `percent` leaves of a hit.
int invertEvade(int percent);
// CalcSpeed: how much the gauge of a combatant grows at each tick; the monsters are slowed by the battle speed (0 to 5).
int atbConstant(int speed, bool monster, int battleSpeed, bool slow = false, bool haste = false);
// CalcDmg, ApplyDmgMult and CalcDmgMod of a physical attack: `power` is the battle power of the weapon or the attack of
// the monster, `strength` the vigor times two, `random` a byte for the variance of the damage.
int physicalDamage(int power, int strength, int level, bool monster, int defense, int random, bool twoHand = false,
    bool critical = false);
// ChooseBattleType: the type (FF6_BATTLE_TYPE_*) from the flags of the battle, the allies alive and a random number.
int chooseBattleType(int flags, int alliesAlive, BattleRandom& random);

// An attack as the battle sees it: a row of the table of attacks (a spell, a special attack of a monster), or what a
// consumable item does (CalcItemEffect).
struct Attack {
    int target = 0, element = 0;
    unsigned flags = 0;  // FF6_AF_*
    int mp = 0, power = 0, misc = 0, hit = 0, special = 0;
    uint32_t status = 0;  // FF6_ST_*
    int bonus = 0;  // half-steps of extra damage: 2 doubles it
    bool item = false;  // a consumable: it has no level
    bool has(unsigned flag) const { return (flags & flag) != 0; }
};
Attack attackFromProp(const uint8_t* prop);
Attack attackFromItem(const uint8_t* itemProp);

// A hand attacks with its own weapon.
struct Hand {
    int power = 0, hitRate = 0, element = 0;
    bool twoHand = false, longRange = false;
};

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
    int relic1 = 0, relic3 = 0;  // the effects of the relics that count in a battle
    std::array<int, 2> stealItems{FF6_ITEM_EMPTY, FF6_ITEM_EMPTY};  // what a monster can be robbed of: rare, then common
    bool invincible = false;  // a script made it immune to damage
    int sleepCounter = 0, stopCounter = 0, reflectCounter = 0, freezeCounter = 0, condemned = 0, dot = 0;  // paces left
    // The script of a monster (AI): where it starts, the retaliation script, where a wait left it, how many conditions had
    // failed then, its timer and variable, and what hit it last.
    int aiStart = -1, aiCounter = -1, aiLoop = 0, aiCount = FF6_AI_NO_LOOP, timer = 0, variable = 0;
    int retalAttacker = -1, retalCommand = -1, retalAttack = -1, retalItem = -1, retalElement = 0;
    bool alive() const { return present && !escaped && hp > 0 && !(status & FF6_ST_GONE); }
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
        Learned  // the target (Strago) learned the lore `attack`
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
};

class Battle {
public:
    // `party` has the characters (numbers in the party) that fight, at most FF6_BATTLE_PARTY_SLOTS. The state of the
    // party is changed by the battle: hp, and at the end experience, gil and items.
    Battle(const BattleData& data, const GameRules& rules, PartyState& party, const std::vector<int>& characters,
        int battle, uint32_t seed, int battleSpeed = FF6_BATTLE_SPEED_DEFAULT);

    // One tick of the gauges (every FF6_BATTLE_TICK_FRAMES frames); the monsters act when theirs is full.
    void tick();
    // The first character that waits for a command, or -1.
    int readyCharacter() const;
    // The command Fight of a character at a monster (a combatant index): each hand that has a weapon strikes.
    void fight(int attacker, int target);
    // The party runs while the buttons are held: a character that waits for a command leaves at its next turn of the
    // counter, and the battle ends when no character is left.
    void setRunning(bool running) { running_ = running; }
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
    // An esper is summoned once in a battle.
    bool summoned() const { return summoned_; }
    bool canCast(int caster) const;
    // The attack of a spell, and what the character pays for it.
    Attack spellAttack(int spell) const;
    // The combatants that an attack can be aimed at: the ones that stand, or the ones that fell when it brings back.
    bool canTarget(const Attack& attack, int target) const;

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
    bool summoned_ = false, doubleBlow_ = false;
    int controlled_ = -1;
    bool sketchHits(const Combatant& user, const Combatant& target, bool boosted);
    void tryStealing(int thief, int target);
    void initGauges();
    void attack(int attacker, int target, int hand);
    bool hits(const Hand& hand, const Combatant& target);
    void runCounters();
    void checkEnd();
    void win();
    void strike(int attacker, int target);
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
    void runScript(int index, size_t start, bool counter);
    bool aiCondition(int index, int condition, int first, int second, uint16_t& mask);
    uint16_t aiTargets(int index, int code, bool onlyAlive);
    void aiAttack(int index, int attack, uint16_t mask);
    void aiMisc(int index, int effect, int value);
    void noteHit(int user, int target, int command, int attack, int item, int element);
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
    bool aiRunning_ = false;  // a script is running: the hits it causes do not start another one
};

}  // namespace ff6
