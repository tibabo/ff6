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

int physicalDamage(int power, int strength, int level, bool monster, int defense, int random, bool twoHand, bool critical, bool halve,
    bool genji, int extra) {
    int attack = monster ? power * FF6_MONSTER_POWER_FACTOR : power;
    if (twoHand) attack += ((attack >> 1) + attack) >> 1;
    int damage = ((((attack + strength) * level) & 0xFFFF) * level) >> 8;
    if (!monster) damage = ((power * 2 + damage) >> 1) + damage;
    damage = std::min(damage + extra, 0xFFFF);
    if (halve) damage >>= 1;
    if (genji) damage -= damage >> 2;
    if (critical) damage = std::min(damage + 2 * (damage >> 1), 0xFFFF);
    damage = ((damage * (random | FF6_DAMAGE_VARIANCE_MIN)) >> 8) + 1;
    damage = ((damage * (0xFF - defense)) >> 8) + 1;
    return std::min(damage, FF6_MAX_DAMAGE);
}

int chooseBattleType(int flags, int alliesAlive, BattleRandom& random, bool backGuard) {
    static constexpr int kRates[FF6_BATTLE_TYPE_COUNT] = FF6_BATTLE_TYPE_RATES;
    bool allowed[FF6_BATTLE_TYPE_COUNT];
    for (int type = 0; type < FF6_BATTLE_TYPE_COUNT; ++type) allowed[type] = !(flags & (1 << (FF6_BATTLE_TYPE_FLAGS_SHIFT + type)));
    if (backGuard) {
        if (allowed[FF6_BATTLE_TYPE_NORMAL] || allowed[FF6_BATTLE_TYPE_BACK] || allowed[FF6_BATTLE_TYPE_SIDE]) allowed[FF6_BATTLE_TYPE_PINCER] = false;
        if (allowed[FF6_BATTLE_TYPE_NORMAL] || allowed[FF6_BATTLE_TYPE_PINCER] || allowed[FF6_BATTLE_TYPE_SIDE]) allowed[FF6_BATTLE_TYPE_BACK] = false;
    }
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

// What a character brings to a battle: its level, hp, stats, hands and relics, what its equipment blocks and starts with.
void Battle::fillCharacter(Combatant& c, int index) {
    const CharacterState& state = party_.characters[static_cast<size_t>(index)];
    const CharacterStats stats = calcStats(rules_, state);
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
        c.hands[hand].special = stats.handSpecial[hand];
        c.hands[hand].spell = stats.handSpell[hand];
        if ((stats.relic3 & FF6_RELIC3_MAX_HIT) && c.hands[hand].power > 0) c.hands[hand].hitRate = FF6_HIT_ALWAYS;
    }
    c.relic1 = stats.relic1;
    c.relic2 = stats.relic2;
    c.relic3 = stats.relic3;
    c.relic4 = stats.relic4;
    c.relic5 = stats.relic5;
    if (stats.relic5 & FF6_RELIC5_MAKE_UNDEAD) c.monsterFlags |= FF6_MONSTER_FLAG_UNDEAD;
    c.evade = stats.evade;
    c.defense = stats.defense;
    c.magicDefense = stats.magicDefense;
    c.magicBlock = stats.magicBlock;
    for (int slot = 0; slot < FF6_MENU_EQUIP_SLOTS; ++slot) {
        const int id = state.equipment[static_cast<size_t>(slot)] & 0xFF;
        if (id == FF6_ITEM_EMPTY) continue;
        const uint8_t* item = rules_.item(id);
        const bool weapon = (item[FF6_ITEM_PROP_TYPE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_WEAPON;
        c.blocked |= static_cast<uint32_t>(item[FF6_ITEM_PROP_IMMUNE] | (item[FF6_ITEM_PROP_IMMUNE + 1] << 8));
        c.status |= static_cast<uint32_t>((item[FF6_ITEM_PROP_EQUIP_STATUS_LOW] << 8) | ((item[FF6_ITEM_PROP_EQUIP_STATUS_HIGH] & ~FF6_ITEM_EQUIP_FLOAT) << 16));
        if (item[FF6_ITEM_PROP_EQUIP_STATUS_HIGH] & FF6_ITEM_EQUIP_FLOAT) c.status |= FF6_ST_FLOAT, c.blocked |= FF6_ST_FLOAT;  // not dance: a floating item
        c.elemAbsorb |= item[FF6_ITEM_PROP_ELEM_ABSORB];
        c.elemNull |= item[FF6_ITEM_PROP_ELEM_NULL];
        c.elemWeak |= item[FF6_ITEM_PROP_ELEM_WEAK];
        if (weapon) {
            if (slot < 2) c.hands[static_cast<size_t>(slot)].element = item[FF6_ITEM_PROP_ELEMENT];
        } else {
            c.elemHalf |= item[FF6_ITEM_PROP_ELEMENT];
        }
    }
    c.status |= state.status;
    if (c.hp == 0) c.status = FF6_ST_DEAD;
    updateNearFatal(c);
}

Battle::Battle(const BattleData& data, const GameRules& rules, PartyState& party, const std::vector<int>& characters,
    int battle, uint32_t seed, int battleSpeed, const Colosseum& colosseum)
    : data_(data), rules_(rules), party_(party), random_(seed), battleSpeed_(battleSpeed), battleNumber_(battle), colosseum_(colosseum) {
    std::vector<int> fighters(characters.begin(), characters.begin() + static_cast<std::ptrdiff_t>(std::min(characters.size(), static_cast<size_t>(FF6_BATTLE_PARTY_SLOTS))));
    const BattleFormation scene = data.formation(battle);
    applyCharAi(scene, fighters);
    for (size_t i = 0; i < fighters.size(); ++i) {
        Combatant& c = combatants_[i];
        c.present = true;
        c.id = fighters[i];
        fillCharacter(c, fighters[i]);
        applyScenePlace(c, i);
    }
    const BattleFormation formation = data.formation(battle);
    initMonsters(formation);
    placeEnemyChars();
    int alliesAlive = 0;
    for (Combatant& c : combatants_) {
        if (!c.present) continue;
        refreshPace(c);
        alliesAlive += !c.monster && c.hp > 0;
    }
    // ChooseBattleType and InitBattleType: the relics of the party change the chances.
    int partyRelic2 = 0;
    for (const Combatant& c : combatants_) partyRelic2 |= c.relic2;
    type_ = chooseBattleType(formation.flags, alliesAlive, random_, (partyRelic2 & FF6_RELIC2_BACK_GUARD) != 0);
    const bool strikes = type_ == FF6_BATTLE_TYPE_NORMAL || type_ == FF6_BATTLE_TYPE_SIDE;
    if (strikes && !(formation.aux & FF6_BATTLE_AUX_NO_PREEMPTIVE)) {
        const int chance = (FF6_PREEMPTIVE_BASE | (type_ * FF6_PREEMPTIVE_STEP)) * ((partyRelic2 & FF6_RELIC2_GALE_HAIRPIN) ? 2 : 1);
        preemptive_ = random_.byte() < chance;
    }
    initGauges();
    started_ = true;
    for (size_t i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        if (combatants_[i].present && (combatants_[i].status & FF6_ST_NEAR_FATAL)) lowHpSpells(static_cast<int>(i));
    }
    checkEnd();
}

// The monsters of a formation take their places: each one from its properties, with its script (InitMonsters). A change of battle
// does it again, with the party left as it is.
void Battle::initMonsters(const BattleFormation& formation) {
    std::vector<std::pair<size_t, Combatant>> kept;  // the characters that fight for the monsters stay through a change of battle
    for (size_t i = FF6_BATTLE_PARTY_SLOTS; i < combatants_.size(); ++i) {
        if (combatants_[i].present && combatants_[i].charIndex >= 0) kept.emplace_back(i, combatants_[i]);
    }
    for (size_t i = FF6_BATTLE_PARTY_SLOTS; i < combatants_.size(); ++i) combatants_[i] = Combatant{};
    for (size_t i = 0; i < formation.monsters.size(); ++i) {
        if (formation.monsters[i] != FF6_MONSTER_NONE) loadMonster(FF6_BATTLE_PARTY_SLOTS + i, colosseum_.monster >= 0 ? colosseum_.monster : formation.monsters[i]);
    }
    for (const auto& stays : kept) combatants_[stays.first] = stays.second;
}

// One monster, at the place `at` of the combatants: from its properties, with its script.
void Battle::loadMonster(size_t at, int monster) {
    const uint8_t* prop = data_.monster(monster);
    Combatant& c = combatants_[at];
    c = Combatant{};
    c.present = true;
    c.monster = true;
    c.id = monster;
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
    const uint8_t* items = data_.items(c.id);
    c.stealItems = {items[0], items[1]};
    c.gil = prop[FF6_MONSTER_PROP_GIL] | (prop[FF6_MONSTER_PROP_GIL + 1] << 8);
    initAi(static_cast<int>(at));
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

// The chance in percent that a blow or a spell lands (CheckHit): the hit rate against the evade or the block of the target. A
// physical blow is halved by the blindness of the attacker and by the beads of the target, a quarter better against a target
// that is blind, zombie, confused, slowed or about to be raised, and a quarter worse against one that is poisoned, drained,
// near fatal or hasted.
int Battle::hitChance(int hitRate, const Combatant& attacker, const Combatant& target, bool physical) const {
    int rate = hitRate;
    if (physical) {
        if (attacker.status & FF6_ST_BLIND) rate >>= 1;
        if (target.relic4 & FF6_RELIC4_RAND_EVADE) rate >>= 1;
        if (target.status & (FF6_ST_BLIND | FF6_ST_ZOMBIE | FF6_ST_CONFUSE | FF6_ST_SLOW | FF6_ST_RERAISE)) rate = std::min(rate + (rate >> 2), 0xFF);
        if (target.status & (FF6_ST_POISON | FF6_ST_SAP | FF6_ST_NEAR_FATAL | FF6_ST_HASTE)) rate -= rate >> 2;
    }
    return (rate * invertEvade(physical ? target.evade : target.magicBlock)) >> 8;
}

bool Battle::hits(const Hand& hand, const Combatant& attacker, const Combatant& target) {
    if (hand.hitRate == FF6_HIT_ALWAYS || (target.status & FF6_ST_NO_COUNTER_HIT)) return true;
    return random_.below(100) < hitChance(hand.hitRate, attacker, target, true);
}

// CoverEffect: a monster that aims a blow at a character that is near fatal hits instead the ally with the true knight that
// has the most hp left, if it can fight.
int Battle::coverFor(int attackerIndex, int targetIndex) const {
    const Combatant& attacker = combatants_[static_cast<size_t>(attackerIndex)];
    const Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    if (target.loveTarget >= 0 && target.loveTarget != attackerIndex && combatants_[static_cast<size_t>(target.loveTarget)].alive()) return target.loveTarget;  // a love token
    if (!attacker.monster || target.monster || !(target.status & FF6_ST_NEAR_FATAL) || (target.status & FF6_ST_VANISH)) return targetIndex;
    constexpr uint32_t kHelpless = FF6_ST_ZOMBIE | FF6_ST_VANISH | FF6_ST_GONE | FF6_ST_SLEEP | FF6_ST_CONFUSE | FF6_ST_STOP |
                                   FF6_ST_CONTROL | FF6_ST_HIDE | FF6_ST_FROZEN;
    int cover = -1, most = 0;
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        const Combatant& ally = combatants_[static_cast<size_t>(i)];
        if (i == targetIndex || !ally.alive() || !(ally.relic4 & FF6_RELIC4_COVER) || (ally.status & kHelpless)) continue;
        if (ally.hp >= most) {
            most = ally.hp;
            cover = i;
        }
    }
    return cover >= 0 ? cover : targetIndex;
}

// One blow of a hand (CalcDmg and the effects of the weapon): the weapon may drain, kill, steal, break, cast a spell or hit
// harder for what the target is.
void Battle::attack(int attackerIndex, int targetIndex, int handIndex) {
    targetIndex = coverFor(attackerIndex, targetIndex);
    Combatant& attacker = combatants_[static_cast<size_t>(attackerIndex)];
    Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    const Hand hand = attacker.hands[static_cast<size_t>(handIndex)];
    const bool offering = !attacker.monster && (attacker.relic4 & FF6_RELIC4_X_FIGHT);  // it cannot miss, nor hit hard
    const int special = attacker.monster ? 0 : hand.special;
    BattleEvent event;
    event.attacker = attackerIndex;
    event.target = targetIndex;
    event.hand = handIndex;
    if (attacker.monster && !target.monster && (target.status & FF6_ST_INTERCEPTOR) && (random_.byte() & 1)) {  // the dog takes the blow
        BattleEvent block;
        block.kind = BattleEvent::Kind::Interceptor;
        block.attacker = attackerIndex;
        block.target = targetIndex;
        events_.push_back(block);
        if ((random_.byte() & 1) && !countering_ && combatants_[static_cast<size_t>(attackerIndex)].alive()) {  // and bites back
            const int biteId = FF6_ATTACK_FIRST_INTERCEPTOR + (random_.byte() & 1);
            const Attack bite = spellAttack(biteId);
            BattleEvent announce;
            announce.kind = BattleEvent::Kind::Cast;
            announce.attacker = targetIndex;
            announce.attack = biteId;
            events_.push_back(announce);
            countering_ = true;
            perform(targetIndex, bite, {attackerIndex}, biteId, -1);
            countering_ = false;
        }
        return;
    }
    if (special == FF6_WEAPON_TEMPEST && random_.byte() < FF6_WEAPON_HALF_LIMIT && !countering_ && !offering) {  // half the time a gale instead
        BattleEvent announce;
        announce.kind = BattleEvent::Kind::Cast;
        announce.attacker = attackerIndex;
        announce.attack = FF6_ATTACK_WIND_SLASH;
        events_.push_back(announce);
        perform(attackerIndex, spellAttack(FF6_ATTACK_WIND_SLASH), targetsFor(attackerIndex, spellAttack(FF6_ATTACK_WIND_SLASH), targetIndex),
                FF6_ATTACK_WIND_SLASH, -1);
        return;
    }
    bool sure = offering, noCritical = offering, forceCritical = false, ignoreDefense = false;
    int extra = 0, boost = 0, fixedDamage = -1;
    switch (special) {
        case FF6_WEAPON_ATMA: ignoreDefense = noCritical = true; break;
        case FF6_WEAPON_VALIANT: ignoreDefense = true; extra = attacker.maxHp - attacker.hp; break;
        case FF6_WEAPON_HEAL_ROD: ignoreDefense = true; break;
        case FF6_WEAPON_MAN_EATER: boost = (target.monsterFlags & FF6_MONSTER_FLAG_HUMAN) ? 2 : 0; break;
        case FF6_WEAPON_STRONG_VS_FLYING:  // half the time it hits harder, and a lot harder against what floats
            if (random_.byte() < FF6_WEAPON_HALF_LIMIT) boost = (target.status & FF6_ST_FLOAT) ? 4 : 1;
            break;
        case FF6_WEAPON_OGRE_NIX:  // it breaks when a random number up to the last digit of the hp is zero
            if (!countering_ && random_.below(attacker.hp % 10 + 1) == 0) {
                party_.characters[static_cast<size_t>(attacker.id)].equipment[static_cast<size_t>(handIndex)] = FF6_ITEM_EMPTY;
                attacker.hands[static_cast<size_t>(handIndex)] = Hand{};
            }
            [[fallthrough]];
        case FF6_WEAPON_MP_CRIT:
        case FF6_WEAPON_MORE_MP_CRIT: {  // the mp that the critical hit costs
            const int cost = (special == FF6_WEAPON_MORE_MP_CRIT ? 28 : 12) + (random_.byte() & 7);
            if (!offering && attacker.mp >= cost) {
                attacker.mp -= cost;
                syncParty(attacker);
                forceCritical = true;
            }
            break;
        }
        case FF6_WEAPON_DICE: {  // two dice, or three for the fixed dice, times twice the level; three alike add that value again
            sure = true;
            const int roll = random_.byte();
            const int first = roll % 6 + 1, second = (roll % FF6_DICE_SPAN) / 16 + 1;
            const int third = hand.hitRate >= 3 ? random_.below(6) + 1 : 0;
            const int product = first * second * (third ? third : 1) * attacker.level * 2;
            const int matches = third && first == second && second == third ? first : 0;
            fixedDamage = std::min(product * (matches + 1), FF6_MAX_DAMAGE);
            break;
        }
        default: break;
    }
    if (hand.power == 0 || (!sure && !hits(hand, attacker, target))) {
        events_.push_back(event);  // a miss
        return;
    }
    const bool critical = !noCritical && (forceCritical || random_.byte() < FF6_CRITICAL_LIMIT);
    event.kind = critical ? BattleEvent::Kind::Critical : BattleEvent::Kind::Hit;
    if (fixedDamage >= 0) {
        event.damage = fixedDamage;
    } else {
        event.damage = physicalDamage(hand.power, attacker.strength, attacker.level, attacker.monster, ignoreDefense ? 0 : target.defense,
            random_.byte(), hand.twoHand, critical, offering, !attacker.monster && (attacker.relic4 & FF6_RELIC4_GENJI_GLOVE), extra);
        event.damage += event.damage * boost / 2;
        if (special == FF6_WEAPON_ATMA) {  // what is left of the hp of the wielder shapes the blow
            const int scale = (attacker.hp / 256 + 1) * attacker.level / (attacker.maxHp / 256 + 1);
            event.damage = ((event.damage * scale) >> 5) + 1;
        }
        if (target.defending) event.damage = std::max(event.damage >> 1, 1);
        if (!attacker.monster && (attacker.relic1 & FF6_RELIC1_ATLAS)) event.damage += event.damage >> FF6_RELIC_DAMAGE_SHIFT;
        if (attacker.backRow && !hand.longRange) event.damage = std::max(event.damage >> 1, 1);
        if (attacker.status & FF6_ST_MORPH) event.damage <<= 1;
        event.damage *= blowMultiplier_;
        if (target.backRow) event.damage = std::max(event.damage >> 1, 1);
    }
    if (special != FF6_WEAPON_HEAL_ROD && hand.element) {  // the element of the weapon against what the target absorbs, resists or fears
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
    if (special == FF6_WEAPON_HEAL_ROD) {  // the rod heals what it touches
        event.kind = BattleEvent::Kind::Heal;
        event.damage = std::min(event.damage, target.maxHp - target.hp);
        target.hp += event.damage;
        syncParty(target);
        events_.push_back(event);
        return;
    }
    if (special == FF6_WEAPON_SOUL_SABRE) {  // it takes mp, and gives it to the wielder
        event.kind = BattleEvent::Kind::MpDamage;
        event.damage = std::min(event.damage, target.mp);
        target.mp -= event.damage;
        attacker.mp = std::min(attacker.mp + event.damage, attacker.maxMp);
        syncParty(target);
        syncParty(attacker);
        events_.push_back(event);
        return;
    }
    if (attacker.monster && !target.monster && golemHp_ > 0) {  // the golem takes the blow for the party
        const int absorbed = std::min(golemHp_, event.damage);
        golemHp_ -= absorbed;
        event.damage -= absorbed;
    }
    event.damage = std::min(event.damage, target.hp);
    target.hp = std::max(0, target.hp - event.damage);
    if (special == FF6_WEAPON_DRAINER) {  // the wielder gets the hp that the target loses
        attacker.hp = std::min(attacker.hp + event.damage, attacker.maxHp);
        updateNearFatal(attacker);
        syncParty(attacker);
    }
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
    if (target.alive()) {
        // The knives that kill, and the one that steals.
        if (special == FF6_WEAPON_INSTANT_DEATH || special == FF6_WEAPON_SCIMITAR) {
            const bool undead = special == FF6_WEAPON_INSTANT_DEATH && target.undead();
            if (undead && !target.monster) {  // death heals an undead character
                target.hp = target.maxHp;
                syncParty(target);
            } else if (!(target.blocked & FF6_ST_DEAD) && (undead || (random_.byte() < FF6_WEAPON_HALF_LIMIT / 2 && target.stamina < (random_.byte() & FF6_STAMINA_ROLL)))) {
                target.hp = 0;
                setStatus(targetIndex, FF6_ST_DEAD);
                BattleEvent fallen;
                fallen.kind = BattleEvent::Kind::StatusSet;
                fallen.attacker = attackerIndex;
                fallen.target = targetIndex;
                fallen.status = FF6_ST_DEAD;
                events_.push_back(fallen);
                BattleEvent defeated;
                defeated.kind = BattleEvent::Kind::Defeated;
                defeated.target = targetIndex;
                events_.push_back(defeated);
            }
        } else if (special == FF6_WEAPON_THIEFKNIFE && !countering_ && random_.byte() < FF6_WEAPON_HALF_LIMIT) {
            tryStealing(attackerIndex, targetIndex);
        }
    }
    // A weapon that casts: now and then it follows the blow with its spell, which costs nothing.
    if (target.alive() && hand.spell >= 0 && !offering && !countering_ && random_.byte() < FF6_WEAPON_SPELL_LIMIT) {
        BattleEvent announce;
        announce.kind = BattleEvent::Kind::Cast;
        announce.attacker = attackerIndex;
        announce.attack = hand.spell;
        events_.push_back(announce);
        const Attack spell = spellAttack(hand.spell);
        perform(attackerIndex, spell, targetsFor(attackerIndex, spell, targetIndex), hand.spell, -1);
    }
    // The retort stance answers every blow, and the black belt three times in four.
    if (attacker.monster && !target.monster && target.alive() && !countering_ &&
        (target.retort || ((target.relic4 & FF6_RELIC4_RAND_RETAL) && random_.byte() < 0xC0))) {
        counterBlow(targetIndex, attackerIndex);
    }
}

// A blow that answers another one: it takes no turn, but for the characters that cannot act.
void Battle::counterBlow(int index, int target) {
    constexpr uint32_t kCannot = FF6_ST_DEAD | FF6_ST_PETRIFY | FF6_ST_MAGITEK | FF6_ST_ZOMBIE | FF6_ST_SLEEP | FF6_ST_CONFUSE | FF6_ST_DANCE |
                                 FF6_ST_STOP | FF6_ST_FROZEN | FF6_ST_CONTROL | FF6_ST_HIDE;
    if ((combatants_[static_cast<size_t>(index)].status & kCannot) || !combatants_[static_cast<size_t>(target)].alive()) return;
    countering_ = true;
    blows(index, target);
    countering_ = false;
}

void Battle::fight(int attacker, int target) {
    Combatant& c = combatants_[static_cast<size_t>(attacker)];
    if (outcome_ != BattleOutcome::Fighting || !c.alive() || c.monster || !c.ready) return;
    if (target < FF6_BATTLE_PARTY_SLOTS || !combatants_[static_cast<size_t>(target)].alive()) return;
    last_ = {0, 0, {target}};
    if (desperation(attacker, target)) return;
    if (party_.characters[static_cast<size_t>(c.id)].actor == FF6_ACTOR_UMARO) {
        umaroFight(attacker, target);
        return;
    }
    strike(attacker, target);
}

// Umaro does not fight like the others: he charges at every monster, throws a friend at one, or calls a storm, with the odds
// that the rage ring (the throw, when there is a friend to throw) and the blizzard orb (the storm) bring.
void Battle::umaroFight(int index, int target) {
    Combatant& umaro = combatants_[static_cast<size_t>(index)];
    static constexpr int kTable[] = FF6_UMARO_TABLE;
    bool ring = false, orb = false;
    int friends = 0;
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) friends += i != index && combatants_[static_cast<size_t>(i)].alive();
    for (const int slot : {4, 5}) {
        const int item = party_.characters[static_cast<size_t>(umaro.id)].equipment[static_cast<size_t>(slot)] & 0xFF;
        ring = ring || (item == FF6_ITEM_RAGE_RING && friends > 0);
        orb = orb || item == FF6_ITEM_BLIZZARD_ORB;
    }
    const int row = (ring ? 4 : 0) + (orb ? 8 : 0);
    const int allowed = 0xC0 | (ring ? 0x10 : 0) | (orb ? 0x20 : 0);  // fight and charge, then the throw (bit 4) and the storm (bit 5)
    int thresholds[4];
    int sum = 0;
    for (int y = 3; y >= 0; --y) {
        if (allowed & (0x10 << y)) sum += kTable[row + 3 - y] + 1;
        thresholds[y] = sum;
    }
    const int roll = random_.byte();
    int move = 3;  // 3 fight, 2 charge, 1 storm, 0 throw
    while (move > 0 && roll >= thresholds[move]) --move;
    umaro.ready = false;
    umaro.gauge = 0;
    last_ = {0, 0, {target}};
    if (move == 3) {
        blows(index, target);
        checkEnd();
        return;
    }
    if (move == 1) {
        BattleEvent announce;
        announce.kind = BattleEvent::Kind::Cast;
        announce.attacker = index;
        announce.attack = FF6_ATTACK_STORM;
        events_.push_back(announce);
        const Attack storm = spellAttack(FF6_ATTACK_STORM);
        perform(index, storm, targetsFor(index, storm, target), FF6_ATTACK_STORM, -1);
        checkEnd();
        return;
    }
    Attack blow;
    blow.flags = FF6_AF_PHYSICAL | FF6_AF_IGNORE_DEF | FF6_AF_NO_SPLIT;
    blow.hit = umaro.strength;
    blow.power = std::min(umaro.hands[0].power + umaro.hands[1].power, 255);
    BattleEvent announce;
    announce.kind = BattleEvent::Kind::Umaro;
    announce.attacker = index;
    std::vector<int> hit;
    if (move == 2) {  // the charge hits all the monsters
        announce.damage = FF6_UMARO_MOVE_CHARGE;
        for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {
            if (combatants_[static_cast<size_t>(i)].alive()) hit.push_back(i);
        }
    } else {  // a friend flies at one monster, and adds what it can do to the blow
        std::vector<int> throwable;
        for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
            const Combatant& other = combatants_[static_cast<size_t>(i)];
            if (i != index && other.alive() && !(other.status & (FF6_ST_HIDE | FF6_ST_SLEEP | FF6_ST_CONFUSE))) throwable.push_back(i);
        }
        if (throwable.empty() || !combatants_[static_cast<size_t>(target)].alive()) {
            blows(index, target);
            checkEnd();
            return;
        }
        const int thrown = throwable[static_cast<size_t>(random_.below(static_cast<int>(throwable.size())))];
        const Combatant& friendly = combatants_[static_cast<size_t>(thrown)];
        blow.power = std::min(blow.power + friendly.hands[0].power + friendly.hands[1].power, 255);
        announce.damage = FF6_UMARO_MOVE_THROW;
        announce.target = thrown;
        hit.push_back(target);
    }
    events_.push_back(announce);
    for (const int victim : hit) resolve(index, blow, {victim}, 0, true);
    checkEnd();
}

// A character that is near fatal sometimes answers the command Fight with a desperation attack of its own, once in a battle,
// after three turns, for the characters up to Mog and for Gogo.
bool Battle::desperation(int index, int target) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    const int actor = party_.characters[static_cast<size_t>(c.id)].actor;
    if (c.desperationUsed || turns_ < FF6_DESPERATION_MIN_TURNS || !(c.status & FF6_ST_NEAR_FATAL) ||
        (c.status & (FF6_ST_CONFUSE | FF6_ST_IMAGE | FF6_ST_VANISH | FF6_ST_ZOMBIE)) || (actor >= FF6_ACTOR_GAU && actor != FF6_ACTOR_GOGO)) {
        return false;
    }
    if ((random_.byte() & FF6_DESPERATION_MASK) != 0) return false;
    c.desperationUsed = true;
    c.ready = false;
    c.gauge = 0;
    const int attackId = FF6_ATTACK_FIRST_DESPERATION + (actor == FF6_ACTOR_GOGO ? FF6_ACTOR_GAU : actor);
    const Attack attack = spellAttack(attackId);
    BattleEvent announce;
    announce.kind = BattleEvent::Kind::Cast;
    announce.attacker = index;
    announce.attack = attackId;
    events_.push_back(announce);
    perform(index, attack, targetsFor(index, attack, target), attackId, -1);
    checkEnd();
    return true;
}

// Where an attack goes when nobody chose: the combatant of a single target, or the side that the attack means.
std::vector<int> Battle::targetsFor(int user, const Attack& attack, int chosen) {
    const int init = attack.target & FF6_TARGET_INIT_MASK;
    const bool opponents = (attack.target & FF6_TARGET_ENEMY) != 0;
    const bool monsterUser = combatants_[static_cast<size_t>(user)].monster;
    if (init == 0) {
        if (!opponents) return {user};
        if (chosen >= 0 && canTarget(attack, chosen)) return {chosen};
    }
    std::vector<int> side;
    for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
        const Combatant& other = combatants_[static_cast<size_t>(i)];
        if (!canTarget(attack, i) || (init != FF6_TARGET_INIT_ALL && (other.monster != monsterUser) != opponents)) continue;
        side.push_back(i);
    }
    if (!side.empty() && (init == 0 || attack.has(FF6_AF_RANDOM_TARGET))) {
        return {side[static_cast<size_t>(random_.below(static_cast<int>(side.size())))]};
    }
    return side;
}

void Battle::strike(int attacker, int target) {
    Combatant& c = combatants_[static_cast<size_t>(attacker)];
    c.ready = false;
    c.gauge = 0;
    blows(attacker, target);
    checkEnd();
}

// The hands strike: each one that has a weapon, and the offering makes eight rounds of them, each at an opponent picked at
// random. The second hand picks another one if the first defeated the target.
void Battle::blows(int attacker, int target) {
    Combatant& c = combatants_[static_cast<size_t>(attacker)];
    const bool offering = !c.monster && (c.relic4 & FF6_RELIC4_X_FIGHT);
    const auto pick = [&](int preferred) {
        std::vector<int> found;
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            const Combatant& other = combatants_[static_cast<size_t>(i)];
            if (other.alive() && other.monster != c.monster && i != attacker) found.push_back(i);
        }
        if (found.empty()) return -1;
        if (offering) return found[static_cast<size_t>(random_.below(static_cast<int>(found.size())))];
        return combatants_[static_cast<size_t>(preferred)].alive() ? preferred : found.front();
    };
    bool struck = false;
    for (int round = 0; round < (offering ? FF6_OFFERING_ROUNDS : 1); ++round) {
        for (int hand = 0; hand < 2; ++hand) {
            if (c.hands[static_cast<size_t>(hand)].power == 0) continue;
            const int at = pick(target);
            if (at < 0) break;
            attack(attacker, at, hand);
            struck = true;
        }
    }
    if (!struck) {  // no power in either hand
        BattleEvent miss;
        miss.attacker = attacker;
        miss.target = target;
        events_.push_back(miss);
    }
}

// A berserk or confused combatant acts by itself when its gauge is full: the berserk one strikes the monsters, the
// confused one anybody.
void Battle::autoAct(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    std::vector<int> targets;
    const bool confused = (c.status & FF6_ST_CONFUSE) != 0;
    const bool charmed = c.charmAttacker >= 0 && !(c.status & (FF6_ST_CONFUSE | FF6_ST_BERSERK));  // it strikes its own party
    for (int t = 0; t < FF6_BATTLE_COMBATANTS; ++t) {
        const Combatant& other = combatants_[static_cast<size_t>(t)];
        if (!other.alive() || t == index) continue;
        if (charmed ? other.monster == c.monster : (confused || other.monster != c.monster)) targets.push_back(t);
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
    if (quickTurns_ > 0) {  // Quick: the others wait while its target takes its turns
        Combatant& q = combatants_[static_cast<size_t>(quickTarget_)];
        if (!q.alive() || (q.status & FF6_ST_NO_GAUGE)) {
            quickTurns_ = 0;
        } else if (q.ready) {  // the command is awaited
            quickWaiting_ = true;
            return;
        } else {
            if (quickWaiting_) {  // the command was given
                quickWaiting_ = false;
                --quickTurns_;
            }
            if (quickTurns_ > 0) {
                q.gauge = FF6_ATB_FULL;
                ++turns_;
                if ((q.status & (FF6_ST_BERSERK | FF6_ST_CONFUSE)) || (colosseum_.monster >= 0 && !q.monster)) {
                    autoAct(quickTarget_);
                    --quickTurns_;
                } else if (!q.monster) {
                    q.ready = true;
                    q.defending = false;
                    q.runic = false;
                    quickWaiting_ = true;
                } else {
                    monsterTurn(quickTarget_);
                    --quickTurns_;
                }
                checkEnd();
            }
            return;
        }
    }
    runCounters();
    if (outcome_ != BattleOutcome::Fighting) return;
    for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
        Combatant& c = combatants_[static_cast<size_t>(i)];
        if ((c.status & FF6_ST_MORPH) && c.alive()) {  // the supply of the morph runs out as it goes
            --c.morph;
            party_.morphCounter = std::max(c.morph, 0) / FF6_MORPH_TICKS_PER_SUPPLY;
            if (c.morph <= 0) clearStatus(i, FF6_ST_MORPH);
        }
        if (!c.alive() || c.ready || (c.status & FF6_ST_NO_GAUGE) || c.seizeAttacker >= 0 || i == zingerAttacker_) continue;
        c.gauge += c.atbConstant;
        if (c.gauge < FF6_ATB_FULL) continue;
        c.gauge = FF6_ATB_FULL;
        ++turns_;
        if ((c.status & (FF6_ST_BERSERK | FF6_ST_CONFUSE)) || c.charmAttacker >= 0 || (colosseum_.monster >= 0 && !c.monster)) {
            autoAct(i);
            if (outcome_ != BattleOutcome::Fighting) return;
            continue;
        }
        if (!c.monster && (c.status & FF6_ST_RAGE) && c.rageMonster >= 0) {  // a rage goes on alone
            rageTurn(i);
            checkEnd();
            if (outcome_ != BattleOutcome::Fighting) return;
            continue;
        }
        if (!c.monster && (c.status & FF6_ST_DANCE) && c.danceIndex >= 0) {  // so does a dance
            c.gauge = 0;
            danceAttack(i);
            checkEnd();
            if (outcome_ != BattleOutcome::Fighting) return;
            continue;
        }
        if (!c.monster) {
            if (c.aiStart >= 0) {  // a character of a scene acts by its script
                monsterTurn(i);
                checkEnd();
                if (outcome_ != BattleOutcome::Fighting) return;
                continue;
            }
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
        settleWager();
    } else if (!monsters && !holdEnd_) {  // a script can hold the end of the battle while the monsters are away
        if (colosseum_.monster < 0 && gauAppears()) return;
        if (colosseum_.monster < 0) shadowLeaves();
        win();
    }
}

// The end of a bout of the colosseum: the item that was wagered is gone.
void Battle::settleWager() {
    if (colosseum_.monster >= 0 && colosseum_.wager != FF6_ITEM_EMPTY) takeItem(party_, colosseum_.wager);
}

// On the veldt, once the monsters are gone, Gau may jump in. He is the character of the scene of the game (the table of the character AI,
// where he waits out of the battle), a monster by his script, that the party can feed (with dried meat), hit (he runs away), or
// wait for (when he has already been won over he asks to join at his first turn). The battle that he ends gives no reward.
bool Battle::gauAppears() {
    int members = 0, standing = 0;
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        members += combatants_[static_cast<size_t>(i)].present;
        standing += combatants_[static_cast<size_t>(i)].alive();
    }
    Combatant* gau = nullptr;  // he may be there already, when the scene of the battle is his
    for (size_t i = FF6_BATTLE_PARTY_SLOTS; i < combatants_.size(); ++i) {
        if (combatants_[i].present && combatants_[i].charIndex == FF6_ACTOR_GAU) gau = &combatants_[i];
    }
    if (gauShown_ || !veldt_ || type_ != FF6_BATTLE_TYPE_NORMAL || story_.gauAvailable || members >= FF6_BATTLE_PARTY_SLOTS ||
        (!gau && combatants_[FF6_BATTLE_COMBATANTS - 1].present) || random_.byte() >= FF6_GAU_APPEAR_LIMIT ||
        (!story_.gauObtained && standing < 2)) {
        return false;
    }
    if (!gau) {
        SceneSlot slot;
        if (!readSceneSlot(&data_.charAi[static_cast<size_t>(FF6_CHAR_AI_GAU) * FF6_CHAR_AI_BYTES + 4], slot)) return false;
        gau = placeEnemyChar(slot);
        if (!gau) return false;
    }
    gauShown_ = true;
    gau->offstage = false;  // add_char_target
    gau->invincible = true;  // a blow only makes him run away
    gau->gauge = 0;
    BattleEvent scene;
    scene.kind = BattleEvent::Kind::Scene;
    scene.damage = FF6_BATTLE_EVENT_GAU_APPEARS;
    events_.push_back(scene);
    return true;
}

// After a victory in a normal battle, Shadow may take his fee and go, unless the story keeps him.
void Battle::shadowLeaves() {
    if (!story_.shadowAvailable || story_.shadowStays || type_ != FF6_BATTLE_TYPE_NORMAL) return;
    int standing = 0;
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) standing += combatants_[static_cast<size_t>(i)].alive();
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        Combatant& c = combatants_[static_cast<size_t>(i)];
        if (!c.present || party_.characters[static_cast<size_t>(c.id)].actor != FF6_ACTOR_SHADOW) continue;
        if (random_.byte() < FF6_SHADOW_LEAVE_LIMIT && standing >= 2 && c.alive() && !(c.status & (FF6_ST_PETRIFY | FF6_ST_ZOMBIE))) {
            c.escaped = true;
            shadowLeft_ = true;
            BattleEvent scene;
            scene.kind = BattleEvent::Kind::Scene;
            scene.damage = FF6_BATTLE_EVENT_SHADOW_LEAVES;
            events_.push_back(scene);
        }
        break;
    }
}

// The victory (WinBattle): experience shared by the characters that are alive, all the gil, the drops, and the magic points
// that teach the spells of the espers and items that they wear.
void Battle::win() {
    outcome_ = BattleOutcome::Won;
    victory_ = true;
    if (colosseum_.monster >= 0) {  // the prize, and no experience, gil or drops
        if (colosseum_.prize != FF6_ITEM_EMPTY) {
            giveItem(party_, colosseum_.prize);
            reward_.items.push_back(colosseum_.prize);
        }
        settleWager();
        return;
    }
    std::vector<int> alive;
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        if (combatants_[static_cast<size_t>(i)].alive()) alive.push_back(i);
    }
    long exp = 0;
    for (const Combatant& c : combatants_) {
        if (!c.monster || !c.present) continue;
        if (!veldt_) exp += c.exp;  // the veldt gives gil and items, no experience
        reward_.gil += c.gil;
        const uint8_t* items = data_.items(c.id);
        const int item = items[random_.byte() < FF6_DROP_RARE_LIMIT ? FF6_MONSTER_ITEM_RARE_DROP : FF6_MONSTER_ITEM_COMMON_DROP];
        if (item != FF6_ITEM_EMPTY) reward_.items.push_back(item);
    }
    reward_.exp = alive.empty() ? 0 : exp / static_cast<long>(alive.size());
    bool goldDoubled = false;
    std::vector<int> characters;
    for (const int i : alive) {
        const Combatant& c = combatants_[static_cast<size_t>(i)];
        if ((c.relic5 & FF6_RELIC5_DOUBLE_GP) && !goldDoubled) {  // the cat hood doubles the gil once
            goldDoubled = true;
            reward_.gil *= 2;
        }
        characters.push_back(c.id);
    }
    for (const int i : alive) {
        const Combatant& c = combatants_[static_cast<size_t>(i)];
        const int gained = addExperience(party_, rules_, c.id, (c.relic5 & FF6_RELIC5_DOUBLE_EXP) ? reward_.exp * 2 : reward_.exp);
        if (gained > 0) {
            BattleEvent level;
            level.kind = BattleEvent::Kind::GainLevel;
            level.attacker = i;
            level.damage = party_.characters[static_cast<size_t>(c.id)].level;
            events_.push_back(level);
        }
    }
    giveGil(party_, std::min(reward_.gil, static_cast<long>(FF6_MAX_GIL)));
    for (const int item : reward_.items) giveItem(party_, item);
    for (const int i : alive) {  // Mog learns the dance of the field that he fought on
        if (party_.characters[static_cast<size_t>(combatants_[static_cast<size_t>(i)].id)].actor != FF6_ACTOR_MOG) continue;
        const int dance = background_ >= 0 && background_ < FF6_BG_DANCE_BYTES ? data_.battleBgDance[static_cast<size_t>(background_)] : 0xFF;
        if (dance < FF6_DANCE_COUNT && !(party_.dances & (1u << dance))) {
            party_.dances |= 1u << dance;
            BattleEvent learned;
            learned.kind = BattleEvent::Kind::LearnedDance;
            learned.damage = dance;
            learned.attacker = i;
            events_.push_back(learned);
        }
    }
    reward_.magicPoints = battleNumber_ >= 0 && battleNumber_ < FF6_MAGIC_POINT_BYTES ? data_.magicPoints[static_cast<size_t>(battleNumber_)] : 0;
    if (reward_.magicPoints > 0 && !alive.empty()) {
        BattleEvent earned;
        earned.kind = BattleEvent::Kind::MagicPoints;
        earned.damage = reward_.magicPoints;
        events_.push_back(earned);
        for (const LearnedSpell& spell : learnSpells(party_, rules_, characters, reward_.magicPoints)) {
            BattleEvent learned;
            learned.kind = BattleEvent::Kind::LearnedSpell;
            for (const int i : alive) {
                if (combatants_[static_cast<size_t>(i)].id == spell.character) learned.attacker = i;
            }
            learned.attack = spell.spell;
            events_.push_back(learned);
        }
        for (const int i : alive) {  // Terra gathers what she can morph with
            const Combatant& c = combatants_[static_cast<size_t>(i)];
            if (party_.characters[static_cast<size_t>(c.id)].actor == FF6_ACTOR_TERRA) {
                party_.morphCounter = std::min(party_.morphCounter + reward_.magicPoints * 2, 255);
                break;
            }
        }
    }
}

}  // namespace ff6
