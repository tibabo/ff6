#include <algorithm>
#include <array>
#include <functional>

#include "battle/battle.hpp"

namespace ff6 {
namespace {

constexpr int kSizes[16] = FF6_AI_COMMAND_SIZES;

int commandSize(int command) { return command < FF6_AI_FIRST_COMMAND ? 1 : kSizes[command - FF6_AI_FIRST_COMMAND]; }

int lowestBit(uint16_t mask) {
    for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
        if (mask & (1u << i)) return i;
    }
    return -1;
}

}  // namespace

// The bit of a monster in the masks of its script: its own place, nothing for a character.
int Battle::monsterBit(int index) const { return index >= FF6_BATTLE_PARTY_SLOTS ? 1 << (index - FF6_BATTLE_PARTY_SLOTS) : 0; }

int& Battle::aiVariable(int index, int number) {
    if (number < FF6_AI_VARIABLES) return variables_[static_cast<size_t>(number)];
    return combatants_[static_cast<size_t>(index)].variable;
}

// FindAIScriptEnd for a monster: the main script ends with 0xFF and the retaliation script follows it.
void Battle::initAi(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    if (scriptId(c) < 0) return;
    const size_t start = data_.aiStart(scriptId(c));
    if (start >= data_.aiScript.size()) return;
    c.aiStart = static_cast<int>(start);
    size_t pos = start;
    while (pos < data_.aiScript.size()) {
        const int command = data_.aiScript[pos++];
        if (command == 0xFF) break;
        if (command != 0xFE) pos += static_cast<size_t>(commandSize(command) - 1);
    }
    if (pos < data_.aiScript.size() && data_.aiScript[pos] != 0xFF) c.aiCounter = static_cast<int>(pos);
}

// CheckAITarget: the combatants that a code of a script means, a bit for each; `onlyAlive` leaves out the ones that fell.
uint16_t Battle::aiTargets(int index, int code, bool onlyAlive) {
    uint16_t party = 0, monsters = 0;
    for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
        const Combatant& c = combatants_[static_cast<size_t>(i)];
        if (!c.present || c.escaped) continue;
        if (onlyAlive && !c.alive()) continue;
        (i < FF6_BATTLE_PARTY_SLOTS ? party : monsters) |= static_cast<uint16_t>(1u << i);
    }
    const uint16_t self = static_cast<uint16_t>(1u << index);
    const auto some = [&](uint16_t mask) {
        std::vector<int> bits;
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            if (mask & (1u << i)) bits.push_back(i);
        }
        return bits.empty() ? uint16_t(0) : static_cast<uint16_t>(1u << bits[static_cast<size_t>(random_.below(static_cast<int>(bits.size())))]);
    };
    const auto withStatus = [&](uint32_t status, bool monsterSide) {
        uint16_t mask = 0;
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            const Combatant& c = combatants_[static_cast<size_t>(i)];
            if (c.present && (i >= FF6_BATTLE_PARTY_SLOTS) == monsterSide && (c.status & status)) mask |= static_cast<uint16_t>(1u << i);
        }
        return mask;
    };
    if (code < FF6_AI_TARGET_MONSTER_1) {  // a character by its actor
        for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
            const Combatant& c = combatants_[static_cast<size_t>(i)];
            if ((party & (1u << i)) && party_.characters[static_cast<size_t>(c.id)].actor == code) return static_cast<uint16_t>(1u << i);
        }
        return 0;
    }
    if (code < FF6_AI_TARGET_SELF) {
        const uint16_t bit = static_cast<uint16_t>(1u << (FF6_BATTLE_PARTY_SLOTS + code - FF6_AI_TARGET_MONSTER_1));
        return monsters & bit;
    }
    switch (code) {
        case 0x36: return self;
        case 0x37: return monsters & ~self;
        case 0x38: return monsters;
        case 0x39: return some(monsters & ~self);
        case 0x3A: return some(monsters);
        case 0x3B: return withStatus(FF6_ST_DEAD, false);
        case 0x3C: return some(withStatus(FF6_ST_DEAD, false));
        case 0x3D: return withStatus(FF6_ST_DEAD, true);
        case 0x3E: return some(withStatus(FF6_ST_DEAD, true));
        case 0x3F: return withStatus(FF6_ST_REFLECT, false);
        case 0x40: return some(withStatus(FF6_ST_REFLECT, false));
        case 0x41: return withStatus(FF6_ST_REFLECT, true);
        case 0x42: return some(withStatus(FF6_ST_REFLECT, true));
        case 0x43: return party;
        case 0x44: return some(party);
        case 0x45: {
            const int attacker = combatants_[static_cast<size_t>(index)].retalAttacker;
            return attacker >= 0 ? static_cast<uint16_t>(1u << attacker) & (party | monsters) : uint16_t(0);
        }
        case 0x46: return party | monsters;
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B: return party & static_cast<uint16_t>(1u << (code - 0x48));
        case 0x4C: {
            const uint16_t others = (party | monsters) & ~self;
            return random_.below(2) ? some(others) : others;
        }
        case FF6_AI_TARGET_TARGETTING: {  // the one that was marked, once
            Combatant& me = combatants_[static_cast<size_t>(index)];
            const int marked = me.targettingTarget;
            me.targettingTarget = -1;
            return marked >= 0 ? static_cast<uint16_t>(1u << marked) & (party | monsters) : uint16_t(0);
        }
        default: return 0;
    }
}

// The condition of a script (FC): it may also change the targets of the script.
bool Battle::aiCondition(int index, int condition, int first, int second, uint16_t& mask) {
    Combatant& self = combatants_[static_cast<size_t>(index)];
    int target = -1;
    const auto aimed = [&]() {  // IF_TARGET_VALID
        const uint16_t found = aiTargets(index, first, false);
        if (!found) return false;
        mask = first == FF6_AI_TARGET_SELF ? uint16_t(0) : found;
        target = lowestBit(found);
        return true;
    };
    const auto standing = [&](bool monsters) {
        uint16_t bits = 0;
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            if (combatants_[static_cast<size_t>(i)].alive() && (i >= FF6_BATTLE_PARTY_SLOTS) == monsters) bits |= static_cast<uint16_t>(1u << i);
        }
        return bits;
    };
    const auto hit = [&](bool matches) {
        if (!matches || self.retalAttacker < 0) return false;
        mask = static_cast<uint16_t>(1u << self.retalAttacker);
        return true;
    };
    const auto hasStatus = [&]() {  // IF_STATUS_SET
        if (!aimed()) return false;
        const int bit = (second & 0x0F) + ((second & 0x10) ? 16 : 0);
        uint16_t kept = 0;
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            if ((aiTargets(index, first, second >= 0x10) & (1u << i)) && (combatants_[static_cast<size_t>(i)].status & (1u << bit))) kept |= static_cast<uint16_t>(1u << i);
        }
        if (!kept) return false;
        std::vector<int> bits;
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            if (kept & (1u << i)) bits.push_back(i);
        }
        mask = static_cast<uint16_t>(1u << bits[static_cast<size_t>(random_.below(static_cast<int>(bits.size())))]);
        return true;
    };
    const auto levelAtLeast = [&]() { return aimed() ? combatants_[static_cast<size_t>(target)].level >= second : false; };
    const auto variableAtLeast = [&]() { return aiVariable(index, first) >= second; };
    const auto switchSet = [&]() { return (aiVariable(index, first) >> (second & 7)) & 1; };
    const int word = first | (second << 8);
    switch (condition) {
        case 1: return hit(self.retalCommand == first || self.retalCommand == second);
        case 2: return hit(self.retalAttack == first || self.retalAttack == second);
        case 3: return hit(self.retalItem == first || self.retalItem == second);
        case 4: return hit((self.retalElement & first) != 0);
        case 5: return hit(true);
        case 6: return aimed() && combatants_[static_cast<size_t>(target)].hp <= second * 128;
        case 7: return aimed() && combatants_[static_cast<size_t>(target)].mp <= second;
        case 8: return hasStatus();
        case 9: return !hasStatus();
        case 11: return (self.timer >> 1) >= word;
        case 12: return !variableAtLeast();
        case 13: return variableAtLeast();
        case 14: return !levelAtLeast();
        case 15: return levelAtLeast();
        case 16: {
            std::vector<int> kinds;
            for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {
                const Combatant& c = combatants_[static_cast<size_t>(i)];
                if (c.alive() && std::find(kinds.begin(), kinds.end(), c.id) == kinds.end()) kinds.push_back(c.id);
            }
            return kinds.size() <= 1;
        }
        case 17:
        case 18: {
            const int slots = first ? first : monsterBit(index);
            uint16_t wanted = static_cast<uint16_t>(slots << FF6_BATTLE_PARTY_SLOTS), present = 0;
            for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {
                if (combatants_[static_cast<size_t>(i)].present) present |= static_cast<uint16_t>(1u << i);
            }
            const uint16_t alive = standing(true);
            return condition == 17 ? (alive & wanted) == wanted : ((present ^ alive) & wanted) == wanted;
        }
        case 19: {
            const int count = static_cast<int>(__builtin_popcount(standing(first != 0)));
            return first == 0 ? count >= second : second >= count;
        }
        case 20: return switchSet() != 0;
        case 21: return switchSet() == 0;
        case 22: return (battleTimer_ >> 1) >= word;
        case 23: return aimed();
        case 24: return !gauJoined_;  // the veldt scripts that wait for Gau to be won over
        case 25: return monsterBit(index) & first;
        case 26: return aimed() && (combatants_[static_cast<size_t>(target)].elemWeak & second);
        case 27: return battleNumber_ == word;
        case 28: return true;
        default: return false;  // never, and Gau
    }
}

// The effects of the command FB.
void Battle::aiMisc(int index, int effect, int value) {
    Combatant& self = combatants_[static_cast<size_t>(index)];
    const auto each = [&](uint16_t mask, const std::function<void(int)>& action) {
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            if (mask & (1u << i)) action(i);
        }
    };
    switch (effect) {
        case 0: self.timer = 0; break;
        case 1: each(aiTargets(index, value, false), [&](int i) { combatants_[static_cast<size_t>(i)].invincible = true; }); break;
        case 5: each(aiTargets(index, value, false), [&](int i) { combatants_[static_cast<size_t>(i)].invincible = false; }); break;
        case 2:
        case 9:  // the veldt ends the battle, and Gau learns the rages that he saw
            if (effect == 9) learnRages();
            if (outcome_ == BattleOutcome::Fighting) outcome_ = BattleOutcome::Won;
            break;
        case 4: battleTimer_ = 0; break;
        case 3: recruitedGau_ = true; break;
        case 6: targetMask_ |= aiTargets(index, value, false) >> FF6_BATTLE_PARTY_SLOTS; break;  // only the monsters have a say
        case 7: targetMask_ &= ~(aiTargets(index, value, false) >> FF6_BATTLE_PARTY_SLOTS); break;
        case 8: each(aiTargets(index, value, true), [&](int i) { combatants_[static_cast<size_t>(i)].gauge = FF6_ATB_FULL; }); break;
        case 11: setStatus(index, 1u << (value & 0x1F)); break;
        case 12: clearStatus(index, 1u << (value & 0x1F)); break;
        case 13: setStatus(index, FF6_ST_HIDE); break;
        default: break;  // the ones that need the story: Gau, what can be aimed at
    }
}

// One attack of a script. A monster's blow, its special attack, or a row of the table of attacks.
void Battle::aiAttack(int index, int attackId, uint16_t mask) {
    Combatant& user = combatants_[static_cast<size_t>(index)];
    if (!user.alive() || attackId >= FF6_AI_ATTACK_NOTHING) return;
    if (user.status & FF6_ST_CONFUSE) mask = 0;
    const auto listed = [&](uint16_t bits) {
        std::vector<int> found;
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            if ((bits & (1u << i)) && combatants_[static_cast<size_t>(i)].alive()) found.push_back(i);
        }
        return found;
    };
    const auto opponent = [&](int i) { return combatants_[static_cast<size_t>(i)].alive() && combatants_[static_cast<size_t>(i)].monster != user.monster; };
    const auto pickOne = [&](const std::vector<int>& from) {
        return std::vector<int>{from[static_cast<size_t>(random_.below(static_cast<int>(from.size())))]};
    };
    if (attackId == FF6_AI_ATTACK_BATTLE) {
        std::vector<int> targets = listed(mask);
        if (targets.empty()) {
            for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
                if (opponent(i)) targets.push_back(i);
            }
            if (targets.empty()) return;
            targets = pickOne(targets);
        }
        for (const int target : targets) {
            if (combatants_[static_cast<size_t>(target)].alive()) attack(index, target, 0);
        }
        return;
    }
    Attack a = attackFromProp(rules_.attack(attackId));
    if (attackId == FF6_AI_ATTACK_SPECIAL) {
        const int special = data_.monster(scriptId(user))[FF6_MONSTER_PROP_SPECIAL];
        const int kind = special & FF6_SPECIAL_KIND_MASK;
        a.power = user.hands[0].power;
        a.hit = user.hands[0].hitRate;
        if (special & FF6_SPECIAL_NO_DAMAGE) {
            a.power = 0;
            a.misc |= FF6_ATTACK_MISC_STATUS_BLOCK;
        }
        if (special & FF6_SPECIAL_NO_DODGE) a.flags |= FF6_AF_NO_DODGE;
        if (kind < FF6_SPECIAL_MULTIPLIER) {
            a.status |= 1u << kind;
        } else if (kind == FF6_SPECIAL_DRAIN_HP || kind == FF6_SPECIAL_DRAIN_MP) {
            a.flags |= FF6_AF_DRAIN | (kind == FF6_SPECIAL_DRAIN_MP ? FF6_AF_AFFECT_MP : 0u);
        } else if (kind >= FF6_SPECIAL_REMOVE_REFLECT) {
            a.flags |= FF6_AF_REMOVE_STATUS;
            a.status = FF6_ST_REFLECT;
        } else {
            a.bonus = kind - FF6_SPECIAL_MULTIPLIER;
        }
    }
    std::vector<int> targets = listed(mask);
    if (targets.empty()) {
        const int init = a.target & FF6_TARGET_INIT_MASK;
        const bool towardsOpponents = (a.target & FF6_TARGET_ENEMY) != 0;
        std::vector<int> side;
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            const Combatant& c = combatants_[static_cast<size_t>(i)];
            if (c.alive() && (init == FF6_TARGET_INIT_ALL || (c.monster != user.monster) == towardsOpponents)) side.push_back(i);
        }
        if (side.empty()) return;
        targets = init == 0 || a.has(FF6_AF_RANDOM_TARGET) ? (towardsOpponents || init != 0 ? pickOne(side) : std::vector<int>{index}) : side;
    }
    BattleEvent announce;
    announce.kind = BattleEvent::Kind::Cast;
    announce.attacker = index;
    announce.attack = attackId;
    events_.push_back(announce);
    if (user.mp < a.mp) {
        BattleEvent lacking = announce;
        lacking.kind = BattleEvent::Kind::NotEnoughMp;
        events_.push_back(lacking);
        return;
    }
    user.mp -= a.mp;
    perform(index, a, targets, attackId, -1);
}

// F2: the monsters of another battle take the place of these.
void Battle::aiChangeBattle(int battle, bool restore) {
    (void)restore;  // the new monsters always start with all their hp and mp
    if (battle < 0 || battle >= FF6_BATTLE_COUNT) return;
    battleNumber_ = battle;
    holdEnd_ = false;
    initMonsters(data_.formation(battle));
    BattleEvent changed;
    changed.kind = BattleEvent::Kind::ChangeBattle;
    changed.damage = battle;
    events_.push_back(changed);
}

// F5: monsters come into the battle or go out of it. Mode 0 brings them back with all their hp, 2 with the hp that they have, 1 and
// 3 hide them, 4 hides them and keeps the battle from ending, 5 hides them for dead.
void Battle::aiEntryExit(int index, int anim, int mode, uint16_t mask) {
    (void)index;
    for (int slot = 0; slot < FF6_BATTLE_SLOTS; ++slot) {
        if (!(mask & (1u << slot))) continue;
        const size_t at = static_cast<size_t>(FF6_BATTLE_PARTY_SLOTS + slot);
        Combatant& m = combatants_[at];
        if (!m.present) continue;
        BattleEvent entry;
        entry.kind = BattleEvent::Kind::Entry;
        entry.attacker = static_cast<int>(at);
        entry.damage = mode;
        entry.attack = anim;  // MONSTER_ENTRY_EXIT_ANIM: how the screen shows it
        if (mode == 0 || mode == 2) {
            if (mode == 0 || m.hp == 0) m.hp = mode == 0 ? m.maxHp : 1;
            m.status &= ~(FF6_ST_DEAD | FF6_ST_PETRIFY | FF6_ST_ZOMBIE | FF6_ST_IMP | FF6_ST_HIDE);
            m.escaped = false;
            m.rattled = false;
            holdEnd_ = false;
        } else if (mode >= 1 && mode <= 5) {
            m.status |= FF6_ST_HIDE;
            if (mode == 4) holdEnd_ = true;
            if (mode == 5) m.status |= FF6_ST_DEAD;
        }
        events_.push_back(entry);
    }
}

// F6: a monster uses an item on the combatants of its target, or throws it.
void Battle::aiItem(int index, bool throwing, int item, uint16_t mask) {
    (void)throwing;
    if (item < 0 || item >= FF6_ITEM_COUNT || item == FF6_ITEM_EMPTY || !combatants_[static_cast<size_t>(index)].alive()) return;
    const Attack a = attackFromItem(rules_.item(item));
    std::vector<int> targets;
    for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
        if ((mask & (1u << i)) && canTarget(a, i)) targets.push_back(i);
    }
    if (targets.empty()) targets = targetsFor(index, a, -1);
    if (targets.empty()) return;
    BattleEvent used;
    used.kind = BattleEvent::Kind::UseItem;
    used.attacker = index;
    used.item = item;
    events_.push_back(used);
    perform(index, a, targets, -1, item);
}

// ExecAICmd: the commands of a script, from `start`, until it waits or ends. The counter script runs when the monster is hit.
void Battle::runScript(int index, size_t start, bool counter) {
    Combatant& self = combatants_[static_cast<size_t>(index)];
    const std::vector<uint8_t>& script = data_.aiScript;
    const auto at = [&](size_t pos) { return pos < script.size() ? static_cast<int>(script[pos]) : 0xFF; };
    size_t pc = start;
    size_t loop = counter ? 0 : static_cast<size_t>(self.aiLoop);
    int saved = counter ? FF6_AI_NO_LOOP : self.aiCount;
    int failed = 0;
    uint16_t mask = 0;
    for (int guard = 0; guard < FF6_AI_LOOP_GUARD; ++guard) {
        int command = at(pc);
        if (command < 0xFC && failed == saved) {  // back to where the last wait left it
            saved = FF6_AI_NO_LOOP;
            pc = loop;
            command = at(pc);
        }
        const int size = commandSize(command);
        const int first = at(pc + 1), second = at(pc + 2), third = at(pc + 3);
        pc += static_cast<size_t>(size);
        if (command < FF6_AI_FIRST_COMMAND) {
            aiAttack(index, command, mask);
        } else {
            switch (command) {
                case 0xF0: {
                    const int pick = (std::array<int, 3>{first, second, third})[static_cast<size_t>(random_.below(3))];
                    if (pick != FF6_AI_ATTACK_NOTHING) aiAttack(index, pick, mask);
                    break;
                }
                case 0xF1: mask = aiTargets(index, first, true); break;
                case 0xF4: {
                    const int pick = (std::array<int, 3>{first, second, third})[static_cast<size_t>(random_.below(3))];
                    if (pick == FF6_CMD_FIGHT) aiAttack(index, FF6_AI_ATTACK_BATTLE, mask);
                    break;
                }
                case 0xF2: aiChangeBattle(first | ((second & 0x7F) << 8), (second & 0x80) == 0); break;
                case 0xF3: {
                    BattleEvent said;
                    said.kind = BattleEvent::Kind::Dialog;
                    said.attacker = index;
                    said.damage = first;
                    events_.push_back(said);
                    break;
                }
                case 0xF5: aiEntryExit(index, first, second, static_cast<uint16_t>(third ? third : monsterBit(index))); break;
                case 0xF6: {
                    const int pick = (std::array<int, 3>{second, third, third})[static_cast<size_t>(random_.below(3))];
                    aiItem(index, first != 0, pick, mask);
                    break;
                }
                case 0xF7: {  // a scene: its dialogs are shown, not its animations
                    BattleEvent scene;
                    scene.kind = BattleEvent::Kind::Scene;
                    scene.attacker = index;
                    scene.damage = first;
                    events_.push_back(scene);
                    sceneEffects(first);
                    break;
                }
                case 0xF8: {
                    int& variable = aiVariable(index, first);
                    const int amount = second & FF6_AI_VALUE_MASK;
                    if (!(second & FF6_AI_VALUE_HIGH)) variable = amount;
                    else variable = std::clamp(variable + ((second & FF6_AI_VALUE_SUBTRACT) ? -amount : amount), 0, 0xFF);
                    break;
                }
                case 0xF9: {
                    int& variable = aiVariable(index, second);
                    const int bit = 1 << (third & 7);
                    variable = first == 0 ? variable ^ bit : first == 1 ? variable | bit : variable & ~bit;
                    break;
                }
                case 0xFA: {  // a monster slides, flashes or glows, or a sound plays
                    static constexpr int kAnims[14] = FF6_MONSTER_ANIMS;
                    BattleEvent shown;
                    shown.kind = BattleEvent::Kind::MonsterAnim;
                    shown.attacker = index;
                    shown.damage = first;
                    if (first == FF6_MONSTER_ANIM_SFX) {
                        shown.attack = third;
                        events_.push_back(shown);
                    } else if (first == FF6_MONSTER_ANIM_RUN_RIGHT || first == FF6_MONSTER_ANIM_RUN_RIGHT + 1) {  // all the characters run across
                        shown.attack = kAnims[first];
                        shown.target = -1;
                        events_.push_back(shown);
                    } else if (first >= 0 && first < 14 && kAnims[first] >= 0) {
                        shown.attack = kAnims[first];
                        for (int slot = 0; slot < FF6_BATTLE_COMBATANTS - FF6_BATTLE_PARTY_SLOTS; ++slot) {
                            const Combatant& other = combatants_[static_cast<size_t>(FF6_BATTLE_PARTY_SLOTS + slot)];
                            if (!(second & (1 << slot)) || !other.present || !other.monster) continue;
                            shown.target = FF6_BATTLE_PARTY_SLOTS + slot;
                            events_.push_back(shown);
                        }
                    }
                    break;
                }
                case 0xFB: aiMisc(index, first, second); break;
                case 0xFC: {
                    if (aiCondition(index, first, second, third, mask)) break;
                    mask = 0;
                    while (pc < script.size()) {  // on to the end of the block
                        const int skipped = at(pc++);
                        if (skipped >= 0xFE) {
                            if (skipped == 0xFF) pc = script.size();
                            break;
                        }
                        pc += static_cast<size_t>(commandSize(skipped) - 1);
                    }
                    if (pc >= script.size()) {
                        if (!counter) self.aiCount = FF6_AI_NO_LOOP;
                        return;
                    }
                    ++failed;
                    break;
                }
                case 0xFD:
                    if (!counter) {
                        self.aiLoop = static_cast<int>(pc);
                        self.aiCount = failed;
                    }
                    return;
                case 0xFE:
                case 0xFF:
                    if (!counter) self.aiCount = FF6_AI_NO_LOOP;
                    return;
                default: break;  // dialogs, changes of battle, entries, events and animations
            }
        }
        if (outcome_ != BattleOutcome::Fighting || (!self.alive() && !(counter && self.rattled))) return;  // the last answer goes on
    }
}

// The turn of a monster: its script, or a blow when it has none.
void Battle::monsterTurn(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    c.gauge = 0;
    c.retalAttacker = -1;
    if (c.aiStart >= 0) {
        runScript(index, static_cast<size_t>(c.aiStart), false);
    } else {
        aiAttack(index, FF6_AI_ATTACK_BATTLE, 0);
    }
}

// What hit a monster last, for the conditions of its retaliation script, which runs at once.
void Battle::noteHit(int user, int target, int command, int attack, int item, int element) {
    Combatant& c = combatants_[static_cast<size_t>(target)];
    if (c.monster == combatants_[static_cast<size_t>(user)].monster) return;  // a script answers the other side
    c.retalAttacker = user;
    c.retalCommand = command;
    c.retalAttack = attack;
    c.retalItem = item;
    c.retalElement = element;
    if (aiRunning_ || c.aiCounter < 0) return;
    if (!c.alive()) {  // ExecAIRetal: a monster that the blow killed still answers it, once (if_self_dead, boss_death)
        if (!c.monster || !c.present || c.rattled || !(c.status & FF6_ST_DEAD)) return;
        c.rattled = true;
    }
    aiRunning_ = true;
    runScript(target, static_cast<size_t>(c.aiCounter), true);
    aiRunning_ = false;
}

}  // namespace ff6
