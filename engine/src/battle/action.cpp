#include <algorithm>

#include "battle/battle.hpp"
#include "state/party_ops.hpp"

namespace ff6 {

Attack attackFromProp(const uint8_t* prop) {
    Attack a;
    a.target = prop[FF6_ATTACK_TARGET];
    a.element = prop[FF6_ATTACK_ELEMENT];
    a.flags = prop[FF6_ATTACK_FLAGS] | (prop[FF6_ATTACK_FLAGS + 1] << 8) | (prop[FF6_ATTACK_FLAGS + 2] << 16);
    a.mp = prop[FF6_ATTACK_MP];
    a.power = prop[FF6_ATTACK_POWER];
    a.misc = prop[FF6_ATTACK_MISC];
    a.hit = prop[FF6_ATTACK_HIT];
    a.special = prop[FF6_ATTACK_SPECIAL];
    a.status = prop[FF6_ATTACK_STATUS12] | (prop[FF6_ATTACK_STATUS12 + 1] << 8) | (static_cast<uint32_t>(prop[FF6_ATTACK_STATUS34]) << 16) |
               (static_cast<uint32_t>(prop[FF6_ATTACK_STATUS34 + 1]) << 24);
    return a;
}

// CalcItemEffect for a consumable: damage that ignores the defense, never dodged and never reflected.
Attack attackFromItem(const uint8_t* prop) {
    Attack a;
    a.item = true;
    a.target = prop[FF6_ITEM_PROP_TARGET];
    a.element = prop[FF6_ITEM_PROP_ELEMENT];
    a.power = prop[FF6_ITEM_PROP_ITEM_POWER];
    a.flags = FF6_AF_IGNORE_DEF | FF6_AF_NO_REFLECT | FF6_AF_NO_DODGE;
    a.status = prop[FF6_ITEM_PROP_ITEM_STATUS12] | (prop[FF6_ITEM_PROP_ITEM_STATUS12 + 1] << 8) |
               (static_cast<uint32_t>(prop[FF6_ITEM_PROP_ITEM_STATUS34]) << 16) |
               (static_cast<uint32_t>(prop[FF6_ITEM_PROP_ITEM_STATUS34 + 1]) << 24);
    const int flags = prop[FF6_ITEM_PROP_FLAGS];
    if (flags & FF6_ITEM_FLAG_FRACTION) a.flags |= FF6_AF_HP_FRACTION;
    if (flags & FF6_ITEM_FLAG_REMOVE_STATUS) a.flags |= FF6_AF_REMOVE_STATUS;
    if (flags & FF6_ITEM_FLAG_MP) a.flags |= FF6_AF_AFFECT_MP;
    if (flags & FF6_ITEM_FLAG_HP) a.flags |= FF6_AF_RESTORATIVE;
    if (flags & FF6_ITEM_FLAG_INVERT_UNDEAD) a.flags |= FF6_AF_INVERT_UNDEAD;
    if (a.status & FF6_ST_DEAD) a.flags |= FF6_AF_INVERT_UNDEAD | FF6_AF_RESURRECT;
    if (prop[FF6_ITEM_PROP_ITEM_SPECIAL] != FF6_ITEM_SPECIAL_NONE) a.special = prop[FF6_ITEM_PROP_ITEM_SPECIAL] + FF6_ITEM_SPECIAL_BASE;
    return a;
}

// CalcItemEffect for a tool: a physical blow of the power and the hit rate of the item that the target can dodge; the drill
// and the chain saw ignore the defense and the autocrossbow does not share its damage.
Attack attackFromTool(int item, const uint8_t* prop) {
    Attack a;
    a.target = prop[FF6_ITEM_PROP_TARGET];
    a.element = prop[FF6_ITEM_PROP_ELEMENT];
    a.power = prop[FF6_ITEM_PROP_ITEM_POWER];
    a.hit = prop[FF6_ITEM_PROP_TOOL_HIT];
    a.flags = FF6_AF_PHYSICAL | FF6_AF_NO_REFLECT;
    if (item == FF6_ITEM_NOISEBLASTER) a.status = FF6_ST_CONFUSE;
    if (item == FF6_ITEM_DRILL || item == FF6_ITEM_CHAIN_SAW) a.flags |= FF6_AF_IGNORE_DEF;
    if (item == FF6_ITEM_AUTOCROSSBOW) a.flags |= FF6_AF_NO_SPLIT;
    return a;
}

Attack Battle::spellAttack(int spell) const { return attackFromProp(rules_.attack(spell)); }

bool Battle::canCast(int caster) const {
    const Combatant& c = combatants_[static_cast<size_t>(caster)];
    return c.alive() && !(c.status & FF6_ST_SILENCE);
}

bool Battle::canTarget(const Attack& attack, int target) const {
    if (target < 0 || target >= FF6_BATTLE_COMBATANTS) return false;
    const Combatant& c = combatants_[static_cast<size_t>(target)];
    if (!c.present || c.escaped || (c.status & FF6_ST_HIDE)) return false;
    if (c.alive()) return true;
    return (attack.has(FF6_AF_RESURRECT) || ((attack.status & FF6_ST_DEAD) && attack.has(FF6_AF_REMOVE_STATUS))) && c.hp == 0 &&
           !(c.status & FF6_ST_PETRIFY);
}

void Battle::syncParty(const Combatant& c) {
    if (c.monster || !c.present) return;
    CharacterState& state = party_.characters[static_cast<size_t>(c.id)];
    state.hp = c.hp;
    state.mp = c.mp;
}

void Battle::refreshPace(Combatant& c) {
    const bool slow = (c.status & FF6_ST_SLOW) != 0, haste = (c.status & FF6_ST_HASTE) != 0;
    c.atbConstant = atbConstant(c.speed, c.monster, battleSpeed_, slow, haste);
    c.paceConstant = slow ? FF6_ATB_SLOW : haste ? FF6_ATB_HASTE : FF6_ATB_NORMAL;
}

// A character is near fatal at an eighth of its hp, or less.
void Battle::updateNearFatal(Combatant& c) {
    if (c.monster) return;
    if (c.hp > 0 && c.hp <= (c.maxHp >> FF6_NEAR_FATAL_SHIFT) && !(c.status & FF6_ST_GONE)) {
        c.status |= FF6_ST_NEAR_FATAL;
    } else {
        c.status &= ~FF6_ST_NEAR_FATAL;
    }
}

// The set tables of the game: what each status brings with it and takes away.
void Battle::setStatus(int index, uint32_t status, int level) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    uint32_t clear = 0;
    if (status & FF6_ST_HASTE) clear |= FF6_ST_SLOW;
    if (status & FF6_ST_SLOW) clear |= FF6_ST_HASTE;
    if (status & FF6_ST_REGEN) clear |= FF6_ST_SAP;
    if (status & FF6_ST_SAP) clear |= FF6_ST_REGEN;
    if (status & FF6_ST_ZOMBIE) clear |= FF6_ST_DEAD;
    if (status & FF6_ST_DEAD) clear |= FF6_ST_DEAD_CURES;
    if (status & FF6_ST_GONE) {
        clear |= FF6_ST_PETRIFY_CURES | (c.status & FF6_ST_POSITIVE & ~(FF6_ST_RERAISE | FF6_ST_HIDE | FF6_ST_INTERCEPTOR));
    }
    const uint32_t before = c.status;
    c.status = (c.status | status) & ~clear;
    if (c.status & FF6_ST_ZOMBIE) c.status &= ~FF6_ST_ZOMBIE_CURES;
    if (status & FF6_ST_SLEEP) c.sleepCounter = FF6_SLEEP_COUNT;
    if (status & FF6_ST_STOP) c.stopCounter = FF6_STOP_COUNT;
    if (status & FF6_ST_REFLECT) c.reflectCounter = FF6_REFLECT_COUNT;
    if (status & FF6_ST_FROZEN) c.freezeCounter = FF6_FREEZE_COUNT;
    if (status & FF6_ST_CONDEMNED) {
        const int spread = level > 0 ? random_.below(level) : 0;
        c.condemned = std::max(FF6_CONDEMNED_BASE - (level + spread), 0) + FF6_CONDEMNED_MIN;
    }
    if (status & FF6_ST_GONE) {
        c.sleepCounter = c.stopCounter = c.reflectCounter = c.freezeCounter = c.condemned = 0;
        c.ready = false;
        if (status & FF6_ST_DEAD) c.hp = 0;
    }
    if ((before ^ c.status) & (FF6_ST_SLOW | FF6_ST_HASTE)) refreshPace(c);
    updateNearFatal(c);
    syncParty(c);
}

void Battle::clearStatus(int index, uint32_t status) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    const uint32_t before = c.status;
    c.status &= ~status;
    if (status & FF6_ST_SLEEP) c.sleepCounter = 0;
    if (status & FF6_ST_STOP) c.stopCounter = 0;
    if (status & FF6_ST_REFLECT) c.reflectCounter = 0;
    if (status & FF6_ST_FROZEN) c.freezeCounter = 0;
    if (status & FF6_ST_CONDEMNED) c.condemned = 0;
    if ((before ^ c.status) & (FF6_ST_SLOW | FF6_ST_HASTE)) refreshPace(c);
    updateNearFatal(c);
}

// The hit check of a spell (CheckHit): the target's block against the hit rate, or the special rolls of the ones that kill.
bool Battle::spellHits(const Attack& attack, const Combatant& user, const Combatant& target) {
    (void)user;
    if (attack.has(FF6_AF_INSTANT_DEATH) && (target.blocked & FF6_ST_DEAD)) return false;
    if (attack.has(FF6_AF_RESURRECT)) {
        const bool dead = (target.status & FF6_ST_DEAD) != 0, undead = (target.monsterFlags & FF6_MONSTER_FLAG_UNDEAD) != 0;
        if (dead == undead) return false;
    }
    if (attack.has(FF6_AF_NO_DODGE)) return true;
    if (attack.has(FF6_AF_LEVEL_DIV)) return attack.hit > 0 && target.level % attack.hit == 0;
    const int chance = (attack.hit * invertEvade(attack.has(FF6_AF_PHYSICAL) ? target.evade : target.magicBlock)) >> 8;
    if (attack.has(FF6_AF_STAMINA_DEF)) return random_.below(100) < chance && target.stamina < (random_.byte() & FF6_STAMINA_ROLL);
    if (target.status & FF6_ST_NO_COUNTER_HIT) return true;
    if (attack.hit == FF6_HIT_ALWAYS) return true;
    return random_.below(100) < chance;
}

// CalcTargetDmg, MagicStatusEffect and ApplyDmg for one target of an attack: `base` is the damage before the target's
// defenses. The statuses that leave come before the hp, the ones that arrive after it.
void Battle::applyTo(int userIndex, const Attack& a, int targetIndex, int base) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    BattleEvent event;
    event.attacker = userIndex;
    event.target = targetIndex;
    if (!spellHits(a, user, target)) {
        events_.push_back(event);
        return;
    }
    const bool mp = a.has(FF6_AF_AFFECT_MP);
    uint32_t add = 0, remove = 0;
    if (a.has(FF6_AF_REMOVE_STATUS)) {
        remove = a.status & target.status;
    } else if (a.has(FF6_AF_TOGGLE_STATUS)) {
        add = a.status & ~target.status;
        remove = a.status & target.status;
    } else {
        add = a.status & ~target.status;
    }
    add &= ~target.blocked;
    remove &= ~target.blocked;
    if (!a.has(FF6_AF_PHYSICAL) && (target.status & FF6_ST_VANISH) && !(a.status & FF6_ST_VANISH)) remove |= FF6_ST_VANISH;
    if ((a.element & FF6_ELEMENT_FIRE) && (target.status & FF6_ST_FROZEN) && !(a.status & FF6_ST_FROZEN)) remove |= FF6_ST_FROZEN;
    // Life and its like kill the undead.
    const bool undeadMonster = (target.monsterFlags & FF6_MONSTER_FLAG_UNDEAD) != 0;
    if (undeadMonster && a.has(FF6_AF_INVERT_UNDEAD) && a.has(FF6_AF_REMOVE_STATUS) && (a.status & (FF6_ST_DEAD | FF6_ST_ZOMBIE))) {
        add |= FF6_ST_DEAD;
        remove &= ~FF6_ST_DEAD;
    }
    if ((a.misc & FF6_ATTACK_MISC_STATUS_BLOCK) && a.status && !(add | remove)) {
        events_.push_back(event);  // the target is immune: a miss
        return;
    }

    int value = 0;
    bool heal = a.has(FF6_AF_RESTORATIVE);
    if (a.special == FF6_SPECIAL_ELIXIR) {
        heal = true;
        value = target.maxHp - target.hp;
        const int mpGain = target.maxMp - target.mp;
        if (mpGain > 0) {
            target.mp = target.maxMp;
            BattleEvent restored = event;
            restored.kind = BattleEvent::Kind::MpHeal;
            restored.damage = mpGain;
            events_.push_back(restored);
        }
    } else if (a.power > 0 || (a.has(FF6_AF_PHYSICAL) && !user.monster && user.hands[0].power > 0)) {
        if (a.has(FF6_AF_HP_FRACTION)) {
            const int whole = a.item || a.has(FF6_AF_RESURRECT) ? (mp ? target.maxMp : target.maxHp) : (mp ? target.mp : target.hp);
            value = std::max((whole * a.power) >> FF6_FRACTION_SHIFT, 1);
        } else if (a.has(FF6_AF_PHYSICAL)) {
            const int power = a.power > 0 ? a.power : user.hands[0].power;
            value = physicalDamage(power, user.strength, user.level, user.monster, a.has(FF6_AF_IGNORE_DEF) ? 0 : target.defense, random_.byte());
        } else {
            value = base;
            value = ((value * (random_.byte() | FF6_DAMAGE_VARIANCE_MIN)) >> 8) + 1;
            if (!a.has(FF6_AF_IGNORE_DEF)) {
                const int defense = target.magicDefense;
                value = defense >= 0xFF ? 1 : ((value * (0xFF - defense)) >> 8) + 1;
                if (target.status & FF6_ST_SHELL) value = ((value * FF6_DAMAGE_RESIST_NUM) >> 8) + 1;
            }
            if (!a.has(FF6_AF_RESTORATIVE) && !user.monster && !target.monster) value >>= 1;  // friendly fire
        }
        if (target.status & FF6_ST_PETRIFY) {
            value = 0;
        } else {
            value += value * a.bonus / 2;
            if (a.has(FF6_AF_INVERT_UNDEAD)) {
                if (undeadMonster) {
                    if (a.status & (FF6_ST_DEAD | FF6_ST_ZOMBIE)) value = 0;
                    heal = false;
                }
                if ((undeadMonster || (target.status & FF6_ST_ZOMBIE)) && a.has(FF6_AF_DRAIN)) heal = !heal;
            }
            if (a.element) {
                if (target.elemAbsorb & a.element) {
                    heal = !heal;
                } else if (target.elemNull & a.element) {
                    value = 0;
                } else if ((target.elemWeak & a.element) && value < 0x8000) {
                    value <<= 1;
                } else if (target.elemHalf & a.element) {
                    value >>= 1;
                }
            }
        }
        value = std::min(value, FF6_MAX_DAMAGE);
        if (target.invincible && !heal) value = 0;
        if (a.has(FF6_AF_DRAIN) && !heal) value = std::min(value, mp ? target.mp : target.hp);
    }

    if (remove) clearStatus(targetIndex, remove);
    if (value > 0 && target.present) {
        int& points = mp ? target.mp : target.hp;
        const int limit = mp ? target.maxMp : target.maxHp;
        int change = 0;
        if (heal) {
            change = std::min(value, limit - points);
            points += change;
        } else {
            change = std::min(value, points);
            points -= change;
        }
        BattleEvent shown = event;
        shown.damage = heal ? change : value;
        shown.kind = mp ? (heal ? BattleEvent::Kind::MpHeal : BattleEvent::Kind::MpDamage)
                        : (heal ? BattleEvent::Kind::Heal : BattleEvent::Kind::Damage);
        events_.push_back(shown);
        if (a.has(FF6_AF_DRAIN)) {  // what the target loses, the user gets
            int& mine = mp ? user.mp : user.hp;
            const int mineLimit = mp ? user.maxMp : user.maxHp;
            mine = heal ? std::max(mine - change, 0) : std::min(mine + change, mineLimit);
            syncParty(user);
            if (!mp && user.hp == 0) setStatus(userIndex, FF6_ST_DEAD);
        }
        if (mp && target.mp == 0 && (target.monsterFlags & FF6_MONSTER_FLAG_DIE_AT_ZERO_MP)) add |= FF6_ST_DEAD;
        if (!mp && target.hp == 0 && !heal) add |= FF6_ST_DEAD;
    } else if (!(add | remove)) {
        BattleEvent none = event;
        none.kind = BattleEvent::Kind::NoEffect;
        events_.push_back(none);
    }
    if (add) {
        if ((add & FF6_ST_DEAD) && (target.status & FF6_ST_ZOMBIE)) add &= ~FF6_ST_DEAD;  // a zombie does not die
    }
    if (add) {
        setStatus(targetIndex, add, user.level);
        BattleEvent changed = event;
        changed.kind = BattleEvent::Kind::StatusSet;
        changed.status = add;
        events_.push_back(changed);
        if (add & FF6_ST_DEAD) {
            BattleEvent fallen = event;
            fallen.kind = BattleEvent::Kind::Defeated;
            events_.push_back(fallen);
        }
    }
    if (remove) {
        BattleEvent changed = event;
        changed.kind = BattleEvent::Kind::StatusCleared;
        changed.status = remove;
        events_.push_back(changed);
    }
    if (target.hp == 0 && !(target.status & FF6_ST_GONE)) target.hp = 1;  // brought back with no hp of its own
    updateNearFatal(target);
    syncParty(target);
    if (target.hp == 0) target.ready = false;
}

void Battle::resolve(int user, const Attack& attack, const std::vector<int>& targets, int base, bool reflected) {
    std::vector<int> bounced;
    for (const int target : targets) {
        if (!canTarget(attack, target)) continue;
        Combatant& stance = combatants_[static_cast<size_t>(target)];
        if (stance.runic && attack.has(FF6_AF_RUNIC) && user >= FF6_BATTLE_PARTY_SLOTS) {  // the sword takes the magic in
            const int gained = std::min(std::max(attack.mp, 1), stance.maxMp - stance.mp);
            stance.mp += gained;
            syncParty(stance);
            BattleEvent absorbed;
            absorbed.kind = BattleEvent::Kind::MpHeal;
            absorbed.attacker = user;
            absorbed.target = target;
            absorbed.damage = gained;
            events_.push_back(absorbed);
            continue;
        }
        if (!reflected && !attack.has(FF6_AF_NO_REFLECT) && (combatants_[static_cast<size_t>(target)].status & FF6_ST_REFLECT)) {
            bounced.push_back(target);
            continue;
        }
        applyTo(user, attack, target, base);
    }
    if (bounced.empty()) return;
    // A reflected attack goes back at the other side, with half the damage.
    std::vector<int> back;
    for (const int target : bounced) {
        BattleEvent event;
        event.kind = BattleEvent::Kind::Reflected;
        event.attacker = user;
        event.target = target;
        events_.push_back(event);
        const bool toMonsters = target < FF6_BATTLE_PARTY_SLOTS;
        std::vector<int> choices;
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            if (combatants_[static_cast<size_t>(i)].alive() && (i >= FF6_BATTLE_PARTY_SLOTS) == toMonsters) choices.push_back(i);
        }
        if (!choices.empty()) back.push_back(choices[static_cast<size_t>(random_.below(static_cast<int>(choices.size())))]);
    }
    resolve(user, attack, back, base >> FF6_REFLECT_HALF, true);
}

// The base damage of a spell (CalcMagicDmg): the power, times four from level 1, and the magic power times the level.
void Battle::perform(int userIndex, const Attack& attack, std::vector<int> targets, int spell, int item) {
    const Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    const int level = attack.item ? 0 : user.level;
    int base = 0;
    if (attack.power > 0 && !attack.has(FF6_AF_HP_FRACTION)) {
        base = (level >= 1 ? attack.power * 4 : attack.power) + ((((user.magicPower * attack.power) & 0xFFFF) * level) >> 5);
        base = std::min(base, 0xFFFF);
        if (!user.monster) {  // the earrings
            if (user.relic1 & FF6_RELIC1_EARRING) base += base >> 1;
            else if (user.relic3 & FF6_RELIC3_SINGLE_EARRING) base += base >> FF6_RELIC_DAMAGE_SHIFT;
            base = std::min(base, 0xFFFF);
        }
        if (user.status & FF6_ST_MORPH) base = std::min(base << 1, 0xFFFF);
    }
    targets.erase(std::remove_if(targets.begin(), targets.end(), [&](int t) { return !canTarget(attack, t); }), targets.end());
    if (!attack.has(FF6_AF_NO_SPLIT) && static_cast<int>(targets.size()) >= FF6_DAMAGE_SPLIT_TARGETS) base >>= 1;
    resolve(userIndex, attack, targets, base, false);
    if (attack.has(FF6_AF_LORE) && spell >= FF6_ATTACK_FIRST_LORE && spell < FF6_ATTACK_FIRST_LORE + FF6_LORE_COUNT) {
        for (const int target : targets) {
            const Combatant& victim = combatants_[static_cast<size_t>(target)];
            const unsigned bit = 1u << (spell - FF6_ATTACK_FIRST_LORE);
            if (victim.monster || !victim.alive() || party_.characters[static_cast<size_t>(victim.id)].actor != FF6_ACTOR_STRAGO || (party_.lores & bit)) continue;
            party_.lores |= bit;
            BattleEvent learned;
            learned.kind = BattleEvent::Kind::Learned;
            learned.attacker = userIndex;
            learned.target = target;
            learned.attack = spell;
            events_.push_back(learned);
        }
    }
    for (const int target : targets) noteHit(userIndex, target, item >= 0 ? FF6_CMD_ITEM : FF6_CMD_MAGIC, spell >= 0 ? spell : item, item, attack.element);
}

void Battle::cast(int casterIndex, int spell, const std::vector<int>& targets, bool keepTurn) {
    Combatant& caster = combatants_[static_cast<size_t>(casterIndex)];
    if (outcome_ != BattleOutcome::Fighting || caster.monster || !caster.ready || !caster.alive()) return;
    if (spell < 0 || spell >= FF6_MAGIC_PROP_COUNT || targets.empty()) return;
    const bool magic = spell < FF6_SPELL_COUNT;
    if (magic && !canCast(casterIndex)) return;
    Attack attack = spellAttack(spell);
    const bool summon = spell >= FF6_FIRST_GENJU_ATTACK && spell < FF6_FIRST_NAMED_ATTACK;
    if (summon && summoned_) return;
    if (magic && (caster.relic3 & FF6_RELIC3_ONE_MP)) attack.mp = std::min(attack.mp, 1);
    else if (magic && (caster.relic3 & FF6_RELIC3_HALF_MP)) attack.mp /= 2;
    if (!keepTurn) {  // the first spell of X Magic leaves the turn to the second
        caster.ready = false;
        caster.gauge = 0;
    }
    last_ = {1, spell, targets};
    BattleEvent announce;
    announce.kind = BattleEvent::Kind::Cast;
    announce.attacker = casterIndex;
    announce.attack = spell;
    events_.push_back(announce);
    if (caster.mp < attack.mp) {
        BattleEvent lacking = announce;
        lacking.kind = BattleEvent::Kind::NotEnoughMp;
        events_.push_back(lacking);
        return;
    }
    caster.mp -= attack.mp;
    if (summon) summoned_ = true;
    syncParty(caster);
    perform(casterIndex, attack, targets, spell, -1);
    checkEnd();
}

void Battle::useItem(int userIndex, int item, const std::vector<int>& targets) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || targets.empty()) return;
    if (item < 0 || item >= FF6_ITEM_COUNT || itemQuantity(party_, item) <= 0) return;
    const uint8_t* prop = rules_.item(item);
    const int type = prop[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK;
    if (type != FF6_ITEM_TYPE_CONSUMABLE && type != FF6_ITEM_TYPE_TOOL) return;
    const Attack attack = attackFromItem(prop);
    user.ready = false;
    user.gauge = 0;
    last_ = {2, item, targets};
    if (type == FF6_ITEM_TYPE_CONSUMABLE) takeItem(party_, item);
    BattleEvent announce;
    announce.kind = BattleEvent::Kind::UseItem;
    announce.attacker = userIndex;
    announce.item = item;
    if (type == FF6_ITEM_TYPE_TOOL) {  // a tool plays the animation of the attack that it is
        int spell = item == FF6_ITEM_BIO_BLASTER ? FF6_ATTACK_BIO_BLAST_TOOL : item == FF6_ITEM_FLASH ? FF6_ATTACK_FLASH_TOOL : -1;
        Attack tool = spell >= 0 ? spellAttack(spell) : attackFromTool(item, prop);
        announce.attack = spell >= 0 ? spell : FF6_ATTACK_ANIM_TOOLS + item - FF6_TOOL_FIRST;
        if (item == FF6_ITEM_CHAIN_SAW && (random_.byte() & FF6_CHAIN_SAW_KILL_MASK) == 0) {
            tool.power = 0;
            tool.flags = FF6_AF_INSTANT_DEATH | FF6_AF_NO_REFLECT;
            tool.status = FF6_ST_DEAD;
            announce.attack = FF6_ATTACK_ANIM_TOOLS + FF6_TOOL_CHAIN_SAW_KILL;
        }
        events_.push_back(announce);
        perform(userIndex, tool, targets, spell, item);
        checkEnd();
        return;
    }
    events_.push_back(announce);
    perform(userIndex, attack, targets, -1, item);
    checkEnd();
}

void Battle::defend(int userIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive()) return;
    user.ready = false;
    user.gauge = 0;
    user.defending = true;
    BattleEvent event;
    event.kind = BattleEvent::Kind::Defend;
    event.attacker = userIndex;
    events_.push_back(event);
}

void Battle::morph(int userIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || (user.status & FF6_ST_MORPH)) return;
    user.ready = false;
    user.gauge = 0;
    user.morph = FF6_MORPH_TICKS;
    setStatus(userIndex, FF6_ST_MORPH);
}

void Battle::revert(int userIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || !(user.status & FF6_ST_MORPH)) return;
    user.ready = false;
    user.gauge = 0;
    user.morph = 0;
    clearStatus(userIndex, FF6_ST_MORPH);
}

void Battle::runic(int userIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive()) return;
    user.ready = false;
    user.gauge = 0;
    user.runic = true;
    BattleEvent event;
    event.kind = BattleEvent::Kind::Runic;
    event.attacker = userIndex;
    events_.push_back(event);
}

void Battle::throwWeapon(int userIndex, int item, int targetIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    const Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || !target.monster || !target.alive()) return;
    if (item < 0 || item >= FF6_ITEM_COUNT || itemQuantity(party_, item) <= 0) return;
    const uint8_t* prop = rules_.item(item);
    if ((prop[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK) != FF6_ITEM_TYPE_WEAPON) return;
    takeItem(party_, item);
    BattleEvent announce;
    announce.kind = BattleEvent::Kind::UseItem;
    announce.attacker = userIndex;
    announce.item = item;
    events_.push_back(announce);
    const Hand saved = user.hands[0];
    user.hands[0].power = prop[FF6_ITEM_PROP_POWER] * 2;
    user.hands[0].hitRate = FF6_HIT_ALWAYS;
    user.hands[0].twoHand = false;
    user.hands[0].element = prop[FF6_ITEM_PROP_ELEMENT];
    user.ready = false;
    user.gauge = 0;
    const size_t before = events_.size();
    attack(userIndex, targetIndex, 0);
    for (size_t i = before; i < events_.size(); ++i) events_[i].item = item;  // the blow shows the weapon that flies
    user.hands[0] = saved;
    checkEnd();
}

void Battle::jump(int userIndex, int targetIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive()) return;
    if (targetIndex < FF6_BATTLE_PARTY_SLOTS || !combatants_[static_cast<size_t>(targetIndex)].alive()) return;
    doubleBlow_ = true;
    strike(userIndex, targetIndex);
    doubleBlow_ = false;
}

void Battle::mimic(int userIndex) {
    const LastAction last = last_;
    const Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (last.kind < 0 || user.monster || !user.ready || !user.alive()) return;
    std::vector<int> targets;
    for (const int t : last.targets) {
        if (combatants_[static_cast<size_t>(t)].alive()) targets.push_back(t);
    }
    if (targets.empty()) {  // the targets are gone: whoever of the same side stands
        const bool monsters = !last.targets.empty() && last.targets.front() >= FF6_BATTLE_PARTY_SLOTS;
        for (int t = 0; t < FF6_BATTLE_COMBATANTS && targets.empty(); ++t) {
            if (combatants_[static_cast<size_t>(t)].alive() && (t >= FF6_BATTLE_PARTY_SLOTS) == monsters) targets.push_back(t);
        }
    }
    if (targets.empty()) return;
    if (last.kind == 0) fight(userIndex, targets.front());
    else if (last.kind == 1) cast(userIndex, last.value, targets);
    else useItem(userIndex, last.value, targets);
    last_ = last;
}

// CheckSketchHit: the attacker hits if its level reaches the one of the target, and otherwise with the chance of the
// quotient of the levels in 256ths; the beret and the coronet take a third off the level of the target.
bool Battle::sketchHits(const Combatant& user, const Combatant& target, bool boosted) {
    int level = target.level;
    if (boosted) level = (level * FF6_SKETCH_RATE_SCALE) >> 8;
    level = std::max(level, 1);
    const int quotient = user.level * 256 / level;
    return quotient >= 256 || random_.byte() < quotient;
}

void Battle::sketch(int userIndex, int targetIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    const Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || !target.monster || !target.alive()) return;
    const bool cannot = (data_.monster(target.id)[FF6_MONSTER_PROP_MONSTER_STATUS] & FF6_MONSTER_CANT_SKETCH) != 0;
    if (cannot || !sketchHits(user, target, (user.relic3 & FF6_RELIC3_SKETCH_RATE) != 0)) {
        user.ready = false;
        user.gauge = 0;
        BattleEvent miss;
        miss.kind = BattleEvent::Kind::NoEffect;
        miss.attacker = userIndex;
        miss.target = targetIndex;
        events_.push_back(miss);
        return;
    }
    const int attackId = data_.sketchAttack(target.id, random_.byte() >= FF6_SKETCH_SECOND_LIMIT ? 1 : 0);
    if (attackId == FF6_AI_ATTACK_BATTLE || attackId == FF6_AI_ATTACK_SPECIAL) {  // the blow of the monster: a blow
        strike(userIndex, targetIndex);
        return;
    }
    user.ready = false;
    user.gauge = 0;
    BattleEvent announce;
    announce.kind = attackId >= FF6_AI_ATTACK_NOTHING ? BattleEvent::Kind::NoEffect : BattleEvent::Kind::Cast;
    announce.attacker = userIndex;
    announce.target = targetIndex;
    announce.attack = attackId;
    events_.push_back(announce);
    if (attackId >= FF6_AI_ATTACK_NOTHING) return;
    const Attack attack = attackFromProp(rules_.attack(attackId));
    perform(userIndex, attack, {(attack.target & FF6_TARGET_ENEMY) ? targetIndex : userIndex}, attackId, -1);
    checkEnd();
}

bool Battle::control(int userIndex, int targetIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    const Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || !target.monster || !target.alive()) return false;
    constexpr uint32_t kRefuses = FF6_ST_DEAD | FF6_ST_PETRIFY | FF6_ST_VANISH | FF6_ST_ZOMBIE | FF6_ST_SLEEP | FF6_ST_CONFUSE |
                                  FF6_ST_BERSERK | FF6_ST_RAGE | FF6_ST_HIDE | FF6_ST_MORPH;
    const bool cannot = (data_.monster(target.id)[FF6_MONSTER_PROP_MONSTER_STATUS] & FF6_MONSTER_CANT_CONTROL) != 0 ||
                        (target.status & (kRefuses | FF6_ST_CONTROL)) != 0;
    if (!cannot && sketchHits(user, target, (user.relic3 & FF6_RELIC3_CONTROL_RATE) != 0)) {
        controlled_ = targetIndex;
        setStatus(targetIndex, FF6_ST_CONTROL);
        return true;
    }
    user.ready = false;
    user.gauge = 0;
    BattleEvent event;
    event.kind = BattleEvent::Kind::NoEffect;
    event.attacker = userIndex;
    event.target = targetIndex;
    events_.push_back(event);
    return false;
}

void Battle::controlAttack(int userIndex, int attackId, uint16_t mask) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (controlled_ < 0 || user.monster || !user.ready || !user.alive()) return;
    const int monster = controlled_;
    controlled_ = -1;
    user.ready = false;
    user.gauge = 0;
    aiAttack(monster, attackId, mask);
    clearStatus(monster, FF6_ST_CONTROL);
    checkEnd();
}

// GP Rain: the character throws its level times thirty gil, or what the party has, and the monsters share twice that.
void Battle::gpRain(int userIndex, const std::vector<int>& targets) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive()) return;
    std::vector<int> hit;
    for (const int t : targets) {
        if (t >= FF6_BATTLE_PARTY_SLOTS && combatants_[static_cast<size_t>(t)].alive()) hit.push_back(t);
    }
    if (hit.empty()) return;
    user.ready = false;
    user.gauge = 0;
    const long spent = std::min<long>(party_.gil, static_cast<long>(user.level) * FF6_GP_RAIN_PER_LEVEL);
    BattleEvent announce;
    announce.kind = spent > 0 ? BattleEvent::Kind::GilToss : BattleEvent::Kind::NoEffect;
    announce.attacker = userIndex;
    events_.push_back(announce);
    if (spent <= 0) return;
    party_.gil -= spent;
    const int each = static_cast<int>(std::min<long>(spent * FF6_GP_RAIN_MULTIPLIER / static_cast<long>(hit.size()), FF6_MAX_DAMAGE));
    for (const int t : hit) {
        Combatant& target = combatants_[static_cast<size_t>(t)];
        BattleEvent event;
        event.kind = BattleEvent::Kind::Damage;
        event.attacker = userIndex;
        event.target = t;
        event.damage = std::min(each, target.hp);
        target.hp -= event.damage;
        events_.push_back(event);
        if (target.hp == 0) {
            setStatus(t, FF6_ST_DEAD);
            BattleEvent fallen;
            fallen.kind = BattleEvent::Kind::Defeated;
            fallen.target = t;
            events_.push_back(fallen);
        }
    }
    checkEnd();
}

// Possess: it works six times in ten; the monster and the character both leave the battle.
void Battle::possess(int userIndex, int targetIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || !target.monster || !target.alive()) return;
    user.ready = false;
    user.gauge = 0;
    BattleEvent event;
    event.attacker = userIndex;
    event.target = targetIndex;
    if (random_.byte() >= FF6_POSSESS_LIMIT) {
        event.kind = BattleEvent::Kind::NoEffect;
        events_.push_back(event);
        return;
    }
    event.kind = BattleEvent::Kind::Possess;
    events_.push_back(event);
    target.hp = 0;
    setStatus(targetIndex, FF6_ST_DEAD);
    BattleEvent fallen;
    fallen.kind = BattleEvent::Kind::Defeated;
    fallen.target = targetIndex;
    events_.push_back(fallen);
    user.escaped = true;
    checkEnd();
}

// Capture: a blow, and the effect of Steal on the target if it dealt damage.
void Battle::capture(int userIndex, int targetIndex) {
    const Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    const Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || !target.monster || !target.alive()) return;
    const size_t before = events_.size();
    strike(userIndex, targetIndex);
    bool hurt = false;
    for (size_t i = before; i < events_.size(); ++i) {
        hurt = hurt || ((events_[i].kind == BattleEvent::Kind::Hit || events_[i].kind == BattleEvent::Kind::Critical) && events_[i].damage > 0);
    }
    if (hurt && combatants_[static_cast<size_t>(targetIndex)].monster) tryStealing(userIndex, targetIndex);
}

void Battle::changeRow(int userIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive()) return;
    user.ready = false;
    user.gauge = 0;
    user.backRow = !user.backRow;
    BattleEvent event;
    event.kind = BattleEvent::Kind::ChangedRow;
    event.attacker = userIndex;
    event.damage = user.backRow;
    events_.push_back(event);
}

// Steal (the target effect of the game): the chance grows with the level of the thief, and the rare item comes one time
// in eight; a monster that was robbed has nothing more.
void Battle::steal(int thiefIndex, int targetIndex) {
    Combatant& thief = combatants_[static_cast<size_t>(thiefIndex)];
    Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    if (outcome_ != BattleOutcome::Fighting || thief.monster || !thief.ready || !thief.alive() || !target.monster || !target.alive()) return;
    thief.ready = false;
    thief.gauge = 0;
    tryStealing(thiefIndex, targetIndex);
}

void Battle::tryStealing(int thiefIndex, int targetIndex) {
    Combatant& thief = combatants_[static_cast<size_t>(thiefIndex)];
    Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    BattleEvent event;
    event.attacker = thiefIndex;
    event.target = targetIndex;
    event.kind = BattleEvent::Kind::StealFailed;
    if (target.stealItems[0] != FF6_ITEM_EMPTY || target.stealItems[1] != FF6_ITEM_EMPTY) {
        int chance = thief.level + FF6_STEAL_BASE - target.level;
        if (thief.relic3 & FF6_RELIC3_STEAL) chance *= 2;
        const int item = target.stealItems[random_.byte() < FF6_STEAL_RARE_LIMIT ? 0 : 1];
        if (chance > 0 && random_.below(100) < chance && item != FF6_ITEM_EMPTY) {
            event.kind = BattleEvent::Kind::Stole;
            event.item = item;
            target.stealItems = {FF6_ITEM_EMPTY, FF6_ITEM_EMPTY};
            giveItem(party_, item);
        }
    }
    events_.push_back(event);
    noteHit(thiefIndex, targetIndex, FF6_CMD_STEAL, FF6_CMD_STEAL, -1, 0);
}

// Poison, seizure and regen: a spell of power max hp * stamina / 1024 + 2 on the combatant itself, without defense.
void Battle::damageStatus(int index, bool heal, BattleEvent::Kind kind) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    int power = (c.maxHp * c.stamina) / FF6_DOT_DIVISOR;
    power = power >= FF6_DOT_CAP + FF6_DOT_ADD ? 0xFF : power + FF6_DOT_ADD;
    const int value = ((power * (random_.byte() | FF6_DAMAGE_VARIANCE_MIN)) >> 8) + 1;
    BattleEvent event;
    event.kind = kind;
    event.target = index;
    if (heal) {
        event.damage = std::min(value, c.maxHp - c.hp);
        c.hp += event.damage;
    } else {
        event.damage = std::min(value, c.hp);
        c.hp -= event.damage;
    }
    events_.push_back(event);
    if (c.hp == 0) {
        setStatus(index, FF6_ST_DEAD);
        BattleEvent fallen;
        fallen.kind = BattleEvent::Kind::Defeated;
        fallen.target = index;
        events_.push_back(fallen);
    }
    updateNearFatal(c);
    syncParty(c);
}

// What happens each time the pace of a combatant overflows: the counters of the status run down, and poison, seizure
// and regen strike. A stopped combatant only counts its stop down.
void Battle::expireCounters(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    if (!c.alive()) return;
    if (c.stopCounter > 0) {
        if (--c.stopCounter == 0) clearStatus(index, FF6_ST_STOP);
        return;
    }
    if (c.condemned >= 2 && --c.condemned == 1) {
        setStatus(index, FF6_ST_DEAD);
        BattleEvent fallen;
        fallen.kind = BattleEvent::Kind::Defeated;
        fallen.target = index;
        events_.push_back(fallen);
        return;
    }
    if (c.reflectCounter > 0 && --c.reflectCounter == 0) clearStatus(index, FF6_ST_REFLECT);
    if (c.freezeCounter > 0 && --c.freezeCounter == 0) clearStatus(index, FF6_ST_FROZEN);
    if (c.sleepCounter > 0 && --c.sleepCounter == 0) clearStatus(index, FF6_ST_SLEEP);
    const int phase = ++c.dot % FF6_DOT_PERIOD;
    if (phase == FF6_DOT_POISON_PHASE && (c.status & FF6_ST_POISON)) {
        damageStatus(index, false, BattleEvent::Kind::Poison);
    } else if ((FF6_DOT_REGEN_PHASES >> phase) & 1) {
        if (c.status & FF6_ST_SAP) {
            damageStatus(index, false, BattleEvent::Kind::Sap);
        } else if (c.status & FF6_ST_REGEN) {
            damageStatus(index, true, BattleEvent::Kind::Regen);
        }
    }
}

}  // namespace ff6
