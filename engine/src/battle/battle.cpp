#include "battle/battle.hpp"

#include <algorithm>

#include "state/char_stats.hpp"
#include "state/party_ops.hpp"

namespace ff6 {

int BattleRandom::below(int limit) {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return limit > 0 ? static_cast<int>((state_ >> 8) % static_cast<uint32_t>(limit)) : 0;
}

int invertEvade(int percent) {
    const int doubled = percent * 2;
    const int inverted = doubled > 0xFF ? 1 : (0x100 - doubled) & 0xFF;
    return inverted == 0 ? 0xFF : inverted;
}

int atbConstant(int speed, bool monster, int battleSpeed, bool slow, bool haste) {
    const int pace = slow ? FF6_ATB_SLOW : haste ? FF6_ATB_HASTE : FF6_ATB_NORMAL;
    int factor = (speed + FF6_ATB_SPEED_BONUS) & 0xFF;
    if (monster) factor = (factor * (0xFF - battleSpeed * FF6_BATTLE_SPEED_STEP)) >> 8;
    return (factor * (pace + pace / 2)) >> FF6_ATB_SHIFT;
}

int physicalDamage(int power, int strength, int level, bool monster, int defense, int random, bool twoHand, bool critical) {
    int attack = monster ? power * FF6_MONSTER_POWER_FACTOR : power;
    if (twoHand) attack += ((attack >> 1) + attack) >> 1;
    int damage = ((((attack + strength) * level) & 0xFFFF) * level) >> 8;
    if (!monster) damage = ((power * 2 + damage) >> 1) + damage;
    if (critical) damage = std::min(damage + 2 * (damage >> 1), 0xFFFF);
    damage = ((damage * (random | FF6_DAMAGE_VARIANCE_MIN)) >> 8) + 1;
    damage = ((damage * (0xFF - defense)) >> 8) + 1;
    return std::min(damage, FF6_MAX_DAMAGE);
}

int chooseBattleType(int flags, int alliesAlive, BattleRandom& random) {
    static constexpr int kRates[FF6_BATTLE_TYPE_COUNT] = FF6_BATTLE_TYPE_RATES;
    bool allowed[FF6_BATTLE_TYPE_COUNT];
    for (int type = 0; type < FF6_BATTLE_TYPE_COUNT; ++type) allowed[type] = !(flags & (1 << (FF6_BATTLE_TYPE_FLAGS_SHIFT + type)));
    const bool others = allowed[FF6_BATTLE_TYPE_NORMAL] || allowed[FF6_BATTLE_TYPE_BACK] || allowed[FF6_BATTLE_TYPE_PINCER];
    if (alliesAlive < FF6_BATTLE_SIDE_MIN_ALLIES && others) allowed[FF6_BATTLE_TYPE_SIDE] = false;
    // RandBitWithRate: the sums of the rates of the allowed types, side first, each with one more.
    int sums[FF6_BATTLE_TYPE_COUNT];
    int total = 0;
    for (int type = FF6_BATTLE_TYPE_COUNT - 1; type >= 0; --type) {
        if (allowed[type]) total += kRates[type] + 1;
        sums[type] = total;
    }
    const int pick = random.below(total);
    for (int type = FF6_BATTLE_TYPE_COUNT - 1; type > 0; --type) {
        if (pick < sums[type]) return type;
    }
    return FF6_BATTLE_TYPE_NORMAL;
}

Battle::Battle(const BattleData& data, const GameRules& rules, PartyState& party, const std::vector<int>& characters,
    int battle, uint32_t seed, int battleSpeed)
    : data_(data), rules_(rules), party_(party), random_(seed), battleSpeed_(battleSpeed), battleNumber_(battle) {
    for (size_t i = 0; i < characters.size() && i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        const CharacterState& state = party.characters[static_cast<size_t>(characters[i])];
        const CharacterStats stats = calcStats(rules, state);
        Combatant& c = combatants_[i];
        c.present = true;
        c.id = characters[i];
        c.level = state.level;
        c.hp = state.hp;
        c.maxHp = stats.maxHp;
        c.mp = state.mp;
        c.maxMp = stats.maxMp;
        c.speed = stats.speed;
        c.strength = std::min(stats.vigor * 2, FF6_STAT_MAX);
        c.magicPower = stats.magicPower;
        c.stamina = stats.stamina;
        for (size_t hand = 0; hand < c.hands.size(); ++hand) {
            c.hands[hand].power = stats.handPower[hand];
            c.hands[hand].hitRate = stats.handHitRate[hand];
            c.hands[hand].twoHand = stats.handTwoHand[hand];
            c.hands[hand].longRange = stats.handLongRange[hand];
            if ((stats.relic3 & FF6_RELIC3_MAX_HIT) && c.hands[hand].power > 0) c.hands[hand].hitRate = FF6_HIT_ALWAYS;
        }
        c.relic1 = stats.relic1;
        c.relic3 = stats.relic3;
        c.evade = stats.evade;
        c.defense = stats.defense;
        c.magicDefense = stats.magicDefense;
        c.magicBlock = stats.magicBlock;
        for (int slot = 0; slot < FF6_MENU_EQUIP_SLOTS; ++slot) {
            const int id = state.equipment[static_cast<size_t>(slot)] & 0xFF;
            if (id == FF6_ITEM_EMPTY) continue;
            const uint8_t* item = rules.item(id);
            const bool weapon = (item[FF6_ITEM_PROP_TYPE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_WEAPON;
            c.blocked |= static_cast<uint32_t>(item[FF6_ITEM_PROP_IMMUNE] | (item[FF6_ITEM_PROP_IMMUNE + 1] << 8));
            c.status |= static_cast<uint32_t>((item[FF6_ITEM_PROP_EQUIP_STATUS_LOW] << 8) | (item[FF6_ITEM_PROP_EQUIP_STATUS_HIGH] << 16));
            c.elemAbsorb |= item[FF6_ITEM_PROP_ELEM_ABSORB];
            c.elemNull |= item[FF6_ITEM_PROP_ELEM_NULL];
            c.elemWeak |= item[FF6_ITEM_PROP_ELEM_WEAK];
            if (weapon) {
                if (slot < 2) c.hands[static_cast<size_t>(slot)].element = item[FF6_ITEM_PROP_ELEMENT];
            } else {
                c.elemHalf |= item[FF6_ITEM_PROP_ELEMENT];
            }
        }
        if (c.hp == 0) c.status = FF6_ST_DEAD;
        updateNearFatal(c);
    }
    const BattleFormation formation = data.formation(battle);
    for (size_t i = 0; i < formation.monsters.size(); ++i) {
        if (formation.monsters[i] == FF6_MONSTER_NONE) continue;
        const uint8_t* prop = data.monster(formation.monsters[i]);
        Combatant& c = combatants_[FF6_BATTLE_PARTY_SLOTS + i];
        c.present = true;
        c.monster = true;
        c.id = formation.monsters[i];
        c.level = prop[FF6_MONSTER_PROP_LEVEL];
        c.hp = c.maxHp = prop[FF6_MONSTER_PROP_HP] | (prop[FF6_MONSTER_PROP_HP + 1] << 8);
        c.mp = c.maxMp = prop[FF6_MONSTER_PROP_MP] | (prop[FF6_MONSTER_PROP_MP + 1] << 8);
        c.speed = prop[FF6_MONSTER_PROP_SPEED];
        c.hands[0].power = prop[FF6_MONSTER_PROP_ATTACK];
        c.hands[0].hitRate = prop[FF6_MONSTER_PROP_HIT_RATE];
        c.strength = FF6_MONSTER_STRENGTH_BASE + random_.below(FF6_MONSTER_STRENGTH_RANGE);
        c.evade = prop[FF6_MONSTER_PROP_EVADE];
        c.magicBlock = prop[FF6_MONSTER_PROP_MAGIC_BLOCK];
        c.defense = prop[FF6_MONSTER_PROP_DEFENSE];
        c.magicDefense = prop[FF6_MONSTER_PROP_MAGIC_DEFENSE];
        c.magicPower = std::min(prop[FF6_MONSTER_PROP_MAGIC_POWER] + prop[FF6_MONSTER_PROP_MAGIC_POWER] / 2, 0xFF);
        const int steps = (c.maxHp >> 8) >> 1;
        c.stamina = steps < FF6_STAMINA_MONSTER_STEPS ? steps + FF6_STAMINA_MONSTER_BASE : FF6_STAMINA_MONSTER_MAX;
        c.monsterFlags = prop[FF6_MONSTER_PROP_FLAGS];
        c.blocked = prop[FF6_MONSTER_PROP_IMMUNE] | (prop[FF6_MONSTER_PROP_IMMUNE + 1] << 8) | (prop[FF6_MONSTER_PROP_IMMUNE + 2] << 16);
        c.elemAbsorb = prop[FF6_MONSTER_PROP_ABSORB];
        c.elemNull = prop[FF6_MONSTER_PROP_NULL];
        c.elemWeak = prop[FF6_MONSTER_PROP_WEAK];
        c.status = prop[FF6_MONSTER_PROP_STATUS] | (prop[FF6_MONSTER_PROP_STATUS + 1] << 8) |
                   ((prop[FF6_MONSTER_PROP_STATUS + 2] & FF6_MONSTER_STATUS3_MASK) << 16);
        if (prop[FF6_MONSTER_PROP_STATUS + 2] & FF6_MONSTER_STATUS3_FLYING) c.status |= FF6_ST_FLOAT;
        c.exp = prop[FF6_MONSTER_PROP_EXP] | (prop[FF6_MONSTER_PROP_EXP + 1] << 8);
        const uint8_t* items = data.items(c.id);
        c.stealItems = {items[0], items[1]};
        c.gil = prop[FF6_MONSTER_PROP_GIL] | (prop[FF6_MONSTER_PROP_GIL + 1] << 8);
        initAi(static_cast<int>(FF6_BATTLE_PARTY_SLOTS + i));
    }
    int alliesAlive = 0;
    for (Combatant& c : combatants_) {
        if (!c.present) continue;
        refreshPace(c);
        alliesAlive += !c.monster && c.hp > 0;
    }
    // ChooseBattleType and InitBattleType; the relics that change the chances (back guard, gale hairpin) are not read.
    type_ = chooseBattleType(formation.flags, alliesAlive, random_);
    const bool strikes = type_ == FF6_BATTLE_TYPE_NORMAL || type_ == FF6_BATTLE_TYPE_SIDE;
    if (strikes && !(formation.aux & FF6_BATTLE_AUX_NO_PREEMPTIVE)) {
        preemptive_ = random_.byte() < (FF6_PREEMPTIVE_BASE | (type_ * FF6_PREEMPTIVE_STEP));
    }
    initGauges();
    checkEnd();
}

// InitGauge: the high byte of each gauge from the speed, a random rank among the places and the empty places.
void Battle::initGauges() {
    int emptyBonus = 0;
    for (const Combatant& c : combatants_) {
        if (!c.present) emptyBonus += FF6_GAUGE_EMPTY_SLOT;
    }
    std::vector<int> ranks;
    for (int rank = 0; rank < FF6_BATTLE_COMBATANTS; ++rank) ranks.push_back(rank);
    for (int slot = FF6_BATTLE_COMBATANTS - 1; slot >= 0; --slot) {
        const size_t pick = static_cast<size_t>(random_.below(static_cast<int>(ranks.size())));
        const int rank = ranks[pick] * FF6_GAUGE_RANK_STEP;
        ranks.erase(ranks.begin() + static_cast<std::ptrdiff_t>(pick));
        Combatant& c = combatants_[static_cast<size_t>(slot)];
        if (!c.present) continue;
        const auto fromSpeed = [&] {
            int value = c.speed + random_.below(c.speed);
            return std::min(std::min(value + rank, 0xFF) + emptyBonus, 0xFF);
        };
        int high = FF6_GAUGE_START;
        if (c.monster) {
            const int value = preemptive_ || type_ == FF6_BATTLE_TYPE_SIDE ? FF6_GAUGE_MONSTER_SURPRISE : fromSpeed();
            high = std::min(value + 1, 0xFF);
        } else if (!preemptive_ && type_ != FF6_BATTLE_TYPE_SIDE) {
            const int value = type_ == FF6_BATTLE_TYPE_NORMAL ? fromSpeed() : rank;
            high = std::min(value + 1, 0xFF);
        }
        c.gauge = high << 8;
    }
}

int Battle::readyCharacter() const {
    if (outcome_ != BattleOutcome::Fighting) return -1;
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        if (combatants_[static_cast<size_t>(i)].alive() && combatants_[static_cast<size_t>(i)].ready) return i;
    }
    return -1;
}

std::vector<BattleEvent> Battle::takeEvents() {
    std::vector<BattleEvent> events;
    events.swap(events_);
    return events;
}

bool Battle::hits(const Hand& hand, const Combatant& target) {
    if (hand.hitRate == FF6_HIT_ALWAYS || (target.status & FF6_ST_NO_COUNTER_HIT)) return true;
    const int chance = (hand.hitRate * invertEvade(target.evade)) >> 8;
    return random_.below(100) < chance;
}

void Battle::attack(int attackerIndex, int targetIndex, int handIndex) {
    Combatant& attacker = combatants_[static_cast<size_t>(attackerIndex)];
    Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    const Hand& hand = attacker.hands[static_cast<size_t>(handIndex)];
    BattleEvent event;
    event.attacker = attackerIndex;
    event.target = targetIndex;
    event.hand = handIndex;
    if (hand.power == 0 || !hits(hand, target)) {
        events_.push_back(event);  // a miss
        return;
    }
    const bool critical = random_.byte() < FF6_CRITICAL_LIMIT;
    event.kind = critical ? BattleEvent::Kind::Critical : BattleEvent::Kind::Hit;
    event.damage = physicalDamage(hand.power, attacker.strength, attacker.level, attacker.monster, target.defense,
        random_.byte(), hand.twoHand, critical);
    if (target.defending) event.damage = std::max(event.damage >> 1, 1);
    if (!attacker.monster && (attacker.relic1 & FF6_RELIC1_ATLAS)) event.damage += event.damage >> FF6_RELIC_DAMAGE_SHIFT;
    if (attacker.backRow && !hand.longRange) event.damage = std::max(event.damage >> 1, 1);
    if (attacker.status & FF6_ST_MORPH) event.damage <<= 1;
    if (doubleBlow_) event.damage <<= 1;
    if (target.backRow) event.damage = std::max(event.damage >> 1, 1);
    if (hand.element) {  // the element of the weapon against what the target absorbs, resists or fears
        if (target.elemAbsorb & hand.element) {
            event.kind = BattleEvent::Kind::Heal;
            event.damage = std::min(event.damage, FF6_MAX_DAMAGE);
            target.hp = std::min(target.hp + event.damage, target.maxHp);
            syncParty(target);
            events_.push_back(event);
            return;
        }
        if (target.elemNull & hand.element) event.damage = 0;
        else if (target.elemWeak & hand.element) event.damage <<= 1;
        else if (target.elemHalf & hand.element) event.damage >>= 1;
    }
    event.damage = std::min(event.damage, FF6_MAX_DAMAGE);
    if (target.invincible) event.damage = 0;
    target.hp = std::max(0, target.hp - event.damage);
    syncParty(target);
    events_.push_back(event);
    clearStatus(targetIndex, FF6_ST_SLEEP | FF6_ST_CONFUSE);  // a blow wakes it
    if (target.hp == 0) {
        setStatus(targetIndex, FF6_ST_DEAD);
        BattleEvent defeated;
        defeated.kind = BattleEvent::Kind::Defeated;
        defeated.target = targetIndex;
        events_.push_back(defeated);
    }
    noteHit(attackerIndex, targetIndex, FF6_CMD_FIGHT, FF6_AI_ATTACK_BATTLE, -1, 0);
}

void Battle::fight(int attacker, int target) {
    Combatant& c = combatants_[static_cast<size_t>(attacker)];
    if (outcome_ != BattleOutcome::Fighting || !c.alive() || c.monster || !c.ready) return;
    if (target < FF6_BATTLE_PARTY_SLOTS || !combatants_[static_cast<size_t>(target)].alive()) return;
    last_ = {0, 0, {target}};
    strike(attacker, target);
}

void Battle::strike(int attacker, int target) {
    Combatant& c = combatants_[static_cast<size_t>(attacker)];
    c.ready = false;
    c.gauge = 0;
    bool struck = false;
    for (int hand = 0; hand < 2; ++hand) {
        if (c.hands[static_cast<size_t>(hand)].power == 0) continue;
        int at = target;
        if (!combatants_[static_cast<size_t>(at)].alive()) {  // the first hand defeated it: the other one picks another
            at = -1;
            for (int i = 0; i < FF6_BATTLE_COMBATANTS && at < 0; ++i) {
                const Combatant& other = combatants_[static_cast<size_t>(i)];
                if (other.alive() && other.monster != c.monster && i != attacker) at = i;
            }
            if (at < 0) break;
        }
        attack(attacker, at, hand);
        struck = true;
    }
    if (!struck) {  // no power in either hand
        BattleEvent miss;
        miss.attacker = attacker;
        miss.target = target;
        events_.push_back(miss);
    }
    checkEnd();
}

// A berserk or confused combatant acts by itself when its gauge is full: the berserk one strikes the monsters, the
// confused one anybody.
void Battle::autoAct(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    std::vector<int> targets;
    const bool confused = (c.status & FF6_ST_CONFUSE) != 0;
    for (int t = 0; t < FF6_BATTLE_COMBATANTS; ++t) {
        const Combatant& other = combatants_[static_cast<size_t>(t)];
        if (!other.alive() || t == index) continue;
        if (confused || other.monster != c.monster) targets.push_back(t);
    }
    if (targets.empty()) {
        c.gauge = 0;
        return;
    }
    strike(index, targets[static_cast<size_t>(random_.below(static_cast<int>(targets.size())))]);
}

// The counters of status and running: one combatant at each tick, its pace overflows every few turns. A character
// that runs and waits for a command leaves then.
void Battle::runCounters() {
    const int phase = counter_ % FF6_COUNTER_PERIOD;
    ++counter_;
    if (phase == FF6_AI_TIMER_PHASE) {  // the timers of the scripts
        ++battleTimer_;
        for (Combatant& other : combatants_) {
            if (other.present && !(other.status & FF6_ST_GONE)) ++other.timer;
        }
    }
    if (phase >= FF6_BATTLE_COMBATANTS) return;
    Combatant& c = combatants_[static_cast<size_t>(phase)];
    if (!c.present) return;
    c.pace += c.paceConstant;
    if (c.pace < 0x100) return;
    c.pace -= 0x100;
    expireCounters(phase);
    checkEnd();
    if (outcome_ != BattleOutcome::Fighting) return;
    if (c.monster || !running_ || !c.alive() || !c.ready) return;
    c.escaped = true;
    c.ready = false;
    BattleEvent event;
    event.kind = BattleEvent::Kind::Escaped;
    event.attacker = phase;
    events_.push_back(event);
    checkEnd();
}

void Battle::tick() {
    if (outcome_ != BattleOutcome::Fighting) return;
    runCounters();
    if (outcome_ != BattleOutcome::Fighting) return;
    for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
        Combatant& c = combatants_[static_cast<size_t>(i)];
        if ((c.status & FF6_ST_MORPH) && c.alive() && --c.morph <= 0) clearStatus(i, FF6_ST_MORPH);
        if (!c.alive() || c.ready || (c.status & FF6_ST_NO_GAUGE)) continue;
        c.gauge += c.atbConstant;
        if (c.gauge < FF6_ATB_FULL) continue;
        c.gauge = FF6_ATB_FULL;
        if (c.status & (FF6_ST_BERSERK | FF6_ST_CONFUSE)) {
            autoAct(i);
            if (outcome_ != BattleOutcome::Fighting) return;
            continue;
        }
        if (!c.monster) {
            c.ready = true;
            c.defending = false;
            c.runic = false;
            continue;
        }
        monsterTurn(i);
        checkEnd();
        if (outcome_ != BattleOutcome::Fighting) return;
    }
}

void Battle::checkEnd() {
    if (outcome_ != BattleOutcome::Fighting) return;
    bool party = false, monsters = false, escaped = false;
    for (const Combatant& c : combatants_) {
        if (c.alive()) (c.monster ? monsters : party) = true;
        escaped = escaped || c.escaped;
    }
    if (!party) {
        outcome_ = escaped ? BattleOutcome::Fled : BattleOutcome::Lost;
    } else if (!monsters) {
        win();
    }
}

// The victory: experience shared by the characters that are alive, all the gil, and the drops.
void Battle::win() {
    outcome_ = BattleOutcome::Won;
    int alive = 0;
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) alive += combatants_[static_cast<size_t>(i)].alive();
    long exp = 0;
    for (const Combatant& c : combatants_) {
        if (!c.monster || !c.present) continue;
        exp += c.exp;
        reward_.gil += c.gil;
        const uint8_t* items = data_.items(c.id);
        const int item = items[random_.byte() < FF6_DROP_RARE_LIMIT ? FF6_MONSTER_ITEM_RARE_DROP : FF6_MONSTER_ITEM_COMMON_DROP];
        if (item != FF6_ITEM_EMPTY) reward_.items.push_back(item);
    }
    reward_.exp = alive ? exp / alive : 0;
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        if (combatants_[static_cast<size_t>(i)].alive()) addExperience(party_, rules_, combatants_[static_cast<size_t>(i)].id, reward_.exp);
    }
    giveGil(party_, reward_.gil);
    for (const int item : reward_.items) giveItem(party_, item);
}

}  // namespace ff6
