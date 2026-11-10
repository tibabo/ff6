#include <algorithm>

#include "battle/battle.hpp"

namespace ff6 {

// The party can run when no monster that stands holds it back (CANT_ESCAPE).
bool Battle::canRun() const {
    for (const Combatant& c : combatants_) {
        if (c.monster && c.alive() && (data_.monster(c.id)[FF6_MONSTER_PROP_MONSTER_STATUS] & FF6_MONSTER_CANT_ESCAPE)) return false;
    }
    return true;
}

// A combatant that goes out of the battle: a monster that flees, a character that runs.
void Battle::leaves(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    if (!c.alive()) return;
    c.escaped = true;
    c.ready = false;
    BattleEvent event;
    event.kind = BattleEvent::Kind::Escaped;
    event.attacker = index;
    events_.push_back(event);
}

// What a hold or a charm binds is let go: the monster that held or charmed, or the character that was. The ones that were held wait
// for their turn from the start.
void Battle::releaseHolds(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    const auto unbind = [&](int& mine, int Combatant::*theirs) {
        if (mine < 0) return;
        Combatant& other = combatants_[static_cast<size_t>(mine)];
        other.*theirs = -1;
        (c.monster ? other : c).gauge = 0;
        BattleEvent let;
        let.kind = BattleEvent::Kind::Seized;
        let.target = c.monster ? mine : index;
        let.damage = 2;
        events_.push_back(let);
        mine = -1;
    };
    unbind(c.seizeTarget, &Combatant::seizeAttacker);
    unbind(c.seizeAttacker, &Combatant::seizeTarget);
    unbind(c.charmTarget, &Combatant::charmAttacker);
    unbind(c.charmAttacker, &Combatant::charmTarget);
    unbind(c.loveTarget, &Combatant::loveAttacker);
    unbind(c.loveAttacker, &Combatant::loveTarget);
    if (index == zingerTarget_ || index == zingerAttacker_) {  // the monster that clung comes back
        if (zingerAttacker_ >= 0) {
            Combatant& clung = combatants_[static_cast<size_t>(zingerAttacker_)];
            clung.noTarget = false;
            clung.gauge = 0;
        }
        zingerAttacker_ = zingerTarget_ = -1;
    }
}

// The monster that holds a character drains hp from it every few turns, like a spell.
void Battle::seizeDrain(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    if (c.seizeTarget < 0 || !combatants_[static_cast<size_t>(c.seizeTarget)].alive()) return;
    Attack drain;
    drain.power = FF6_SEIZE_POWER;
    drain.hit = FF6_HIT_ALWAYS;
    drain.flags = FF6_AF_DRAIN | FF6_AF_NO_REFLECT | FF6_AF_NO_DODGE;
    perform(index, drain, {c.seizeTarget}, -1, -1);
}

// A combatant that falls with nothing left, hp and mp.
void Battle::defeat(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    if (!c.alive()) return;
    c.hp = 0;
    c.mp = 0;
    setStatus(index, FF6_ST_DEAD);
    syncParty(c);
    BattleEvent defeated;
    defeated.kind = BattleEvent::Kind::Defeated;
    defeated.target = index;
    events_.push_back(defeated);
}

// The effects of the attacker (DoAttackerEffect) that decide what the attack is: who it hits, with what damage, how often.
// False when the attack does not go on.
bool Battle::specialBefore(int userIndex, Attack& a, std::vector<int>& targets, int& base, int& rounds) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    const auto fix = [&](long value) {
        a.fixed = true;
        a.flags |= FF6_AF_IGNORE_DEF | FF6_AF_NO_SPLIT;
        a.power = std::max(a.power, 1);
        base = static_cast<int>(std::min<long>(std::max<long>(value, 0), FF6_MAX_DAMAGE));
    };
    const auto randomOne = [&](const std::vector<int>& from) { return from[static_cast<size_t>(random_.below(static_cast<int>(from.size())))]; };
    switch (a.special) {
        case FF6_SP_WARP:
        case FF6_SP_WARP_STONE:
        case FF6_SP_SMOKE_BOMB:  // the party leaves the battle, if it can
            if (user.monster) break;
            if (!canRun()) {
                BattleEvent failed;
                failed.kind = BattleEvent::Kind::NoEffect;
                failed.attacker = userIndex;
                events_.push_back(failed);
                return false;
            }
            for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) leaves(i);
            checkEnd();
            return false;
        case FF6_SP_MANTRA:  // the hp of the caster, shared by the others
            targets.erase(std::remove(targets.begin(), targets.end(), userIndex), targets.end());
            fix(user.hp / std::max<int>(1, static_cast<int>(targets.size())));
            break;
        case FF6_SP_EXPLODER: fix(user.hp); break;
        case FF6_SP_BLOW_FISH: fix(FF6_BLOW_FISH_DAMAGE); break;
        case FF6_SP_WHITE_WIND: fix(user.hp); break;
        case FF6_SP_REVENGE: fix(user.maxHp - user.hp); break;
        case FF6_SP_STEP_MINE: fix(party_.steps / std::max(a.power, 1)); break;
        case FF6_SP_FLARE_STAR:  // the level of one of the targets, times the power, over the number of targets
            if (!targets.empty()) fix(static_cast<long>(combatants_[static_cast<size_t>(randomOne(targets))].level) * a.power / static_cast<long>(targets.size()));
            break;
        case FF6_SP_N_CROSS:  // each target stands a chance of one in two
            targets.erase(std::remove_if(targets.begin(), targets.end(), [&](int) { return random_.byte() & 1; }), targets.end());
            break;
        case FF6_SP_PEARL_LORE:  // the targets whose level is a multiple of the last digit of the gil
            a.flags |= FF6_AF_LEVEL_DIV;
            a.hit = static_cast<int>(party_.gil % 10);
            break;
        case FF6_SP_QUADRA_SLAM: rounds = FF6_QUADRA_ROUNDS; break;
        case FF6_SP_SUPLEX: {  // one target that can be thrown
            std::vector<int> throwable;
            for (const int t : targets) {
                const Combatant& c = combatants_[static_cast<size_t>(t)];
                if (!c.monster || !(data_.monster(c.id)[FF6_MONSTER_PROP_MONSTER_STATUS] & FF6_MONSTER_CANT_SUPLEX)) throwable.push_back(t);
            }
            if (throwable.empty()) throwable = targets;
            if (!throwable.empty()) targets = {randomOne(throwable)};
            break;
        }
        case FF6_SP_EMPOWERER: {  // it takes mp at full power, then hp at a quarter of it
            Attack mpPass = a;
            mpPass.special = 0;
            mpPass.flags |= FF6_AF_AFFECT_MP;
            resolve(userIndex, mpPass, targets, base, false);
            a.power = std::max(a.power >> 2, 1);
            base = std::max(base >> 2, 1);
            break;
        }
        case FF6_SP_MIND_BLAST:  // a few of the targets, picked at random, are the ones that suffer
            mindBlastMask_ = 0;
            if (!targets.empty()) {
                for (int draw = 0; draw < FF6_MIND_BLAST_DRAWS; ++draw) mindBlastMask_ |= 1 << randomOne(targets);
            }
            break;
        case FF6_SP_GOLEM: golemHp_ = user.hp; break;
        case FF6_SP_DISCARD:  // the monster lets go of what it holds; without a hold there is nothing to do
            if (user.seizeTarget < 0) return false;
            targets = {user.seizeTarget};
            a.flags |= FF6_AF_NO_DODGE;
            break;
        case FF6_SP_FORCEFIELD: {  // one more element that nothing can use in this battle
            std::vector<int> open;
            for (int bit = 0; bit < 8; ++bit) {
                if (!(forcefield_ & (1 << bit))) open.push_back(bit);
            }
            if (open.empty()) return false;
            const int bit = open[static_cast<size_t>(random_.below(static_cast<int>(open.size())))];
            forcefield_ |= 1 << bit;
            BattleEvent field;
            field.kind = BattleEvent::Kind::Forcefield;
            field.attacker = userIndex;
            field.damage = bit;
            events_.push_back(field);
            return false;
        }
        case FF6_SP_SPIRALER:  // the caster is spent by it
            targets.erase(std::remove(targets.begin(), targets.end(), userIndex), targets.end());
            defeat(userIndex);
            break;
        case FF6_SP_PALIDOR:  // what cannot move cannot be jumped at
            targets.erase(std::remove_if(targets.begin(), targets.end(), [&](int t) {
                              return (combatants_[static_cast<size_t>(t)].status & (FF6_ST_PETRIFY | FF6_ST_SLEEP | FF6_ST_STOP | FF6_ST_HIDE | FF6_ST_FROZEN)) != 0;
                          }), targets.end());
            break;
        case FF6_SP_RETORT:  // the stance comes and goes, and hurts nobody
            user.retort = !user.retort;
            return false;
        default: break;
    }
    return true;
}

// What an attack does to one target apart from the damage (DoTargetEffect).
int Battle::specialTarget(int userIndex, Attack& a, int targetIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    Combatant& target = combatants_[static_cast<size_t>(targetIndex)];
    const auto status = [&](const Combatant& c) { return c.monster ? data_.monster(c.id)[FF6_MONSTER_PROP_MONSTER_STATUS] : 0; };
    switch (a.special) {
        case FF6_SP_MISS_FLYING: return (target.status & FF6_ST_FLOAT) ? 0 : 1;
        case FF6_SP_REFLECT_LORE: return (target.status & FF6_ST_REFLECT) ? 1 : 0;
        case FF6_SP_QUICK:
            if (quickTurns_ > 0) return 0;  // another one is quick already
            quickTarget_ = targetIndex;
            quickTurns_ = FF6_QUICK_TURNS;
            quickWaiting_ = false;
            return 1;
        case FF6_SP_SCAN: {
            if (status(target) & FF6_MONSTER_CANT_SCAN) return 0;
            BattleEvent scanned;
            scanned.kind = BattleEvent::Kind::Scan;
            scanned.attacker = userIndex;
            scanned.target = targetIndex;
            events_.push_back(scanned);
            return 2;
        }
        case FF6_SP_SUPLEX: return (status(target) & FF6_MONSTER_CANT_SUPLEX) ? 0 : 1;
        case FF6_SP_ESCAPE:
        case FF6_SP_SNEEZE:
        case FF6_SP_ENGULF:
        case FF6_SP_BABABREATH:  // the target is taken out of the battle
            leaves(targetIndex);
            return 2;
        case FF6_SP_SEIZE:  // a monster holds a character, who cannot act; one at a time
        case FF6_SP_CHARM: {  // and a charmed one strikes its own party
            const bool seize = a.special == FF6_SP_SEIZE;
            int& mine = seize ? user.seizeTarget : user.charmTarget;
            int& theirs = seize ? target.seizeAttacker : target.charmAttacker;
            if (!user.monster || target.monster || mine >= 0 || theirs >= 0) return 0;
            mine = targetIndex;
            theirs = userIndex;
            target.ready = false;
            target.gauge = 0;
            BattleEvent held;
            held.kind = BattleEvent::Kind::Seized;
            held.target = targetIndex;
            held.damage = seize ? 0 : 1;
            events_.push_back(held);
            return 2;
        }
        case FF6_SP_DISCARD:
            releaseHolds(userIndex);
            return 2;
        case FF6_SP_MIND_BLAST:
        case FF6_SP_EVIL_TOOT: {  // only one of the statuses of the attack reaches the target, at random
            if (a.special == FF6_SP_MIND_BLAST && !(mindBlastMask_ & (1 << targetIndex))) {
                a.status = 0;
                return 1;
            }
            std::vector<uint32_t> bits;
            for (int bit = 0; bit < 32; ++bit) {
                if (a.status & (1u << bit)) bits.push_back(1u << bit);
            }
            if (!bits.empty()) a.status = bits[static_cast<size_t>(random_.below(static_cast<int>(bits.size())))];
            return 1;
        }
        case FF6_SP_PHANTASM:
            target.phantasm = true;
            return 2;
        case FF6_SP_OVERCAST:  // the next time that it falls, it is a zombie
            target.overcast = true;
            return 1;
        case FF6_SP_AIR_ANCHOR:  // what cannot be struck down is not touched
            return (target.blocked & FF6_ST_DEAD) ? 0 : 1;
        case FF6_SP_DOOM:  // death does not take the undead: a character is healed in full, a monster goes away
            if (!(target.monsterFlags & FF6_MONSTER_FLAG_UNDEAD)) return 1;
            if (target.monster) {
                setStatus(targetIndex, FF6_ST_HIDE);
                events_.push_back(BattleEvent{BattleEvent::Kind::Entry, targetIndex, -1, 1});
                return 2;
            }
            a.status &= ~FF6_ST_DEAD;
            target.hp = target.maxHp;
            syncParty(target);
            return 1;
        case FF6_SP_LOVE_TOKEN:  // the target will take the blows that are meant for the caster
            if (userIndex == targetIndex || user.loveTarget >= 0 || target.loveAttacker >= 0) return 0;
            user.loveTarget = targetIndex;
            target.loveAttacker = userIndex;
            events_.push_back(BattleEvent{BattleEvent::Kind::Seized, userIndex, targetIndex, 3});
            return 2;
        case FF6_SP_ZINGER:  // the caster clings to the target and stays out of the fight until it falls
            if (!user.monster || zingerAttacker_ >= 0) return 0;
            zingerAttacker_ = userIndex;
            zingerTarget_ = targetIndex;
            user.noTarget = true;
            user.gauge = 0;
            events_.push_back(BattleEvent{BattleEvent::Kind::Seized, userIndex, targetIndex, 4});
            return 2;
        case FF6_SP_TARGETTING:
            user.targettingTarget = targetIndex;
            return 2;
        case FF6_SP_DISCHORD: target.level = (target.level + 1) >> 1; return 1;
        case FF6_SP_TAPIR:  // it wakes nothing: it heals who sleeps, in full
            if (!(target.status & FF6_ST_SLEEP)) return 0;
            target.hp = target.maxHp;
            target.mp = target.maxMp;
            syncParty(target);
            return 2;
        case FF6_SP_PEP_UP:  // the caster gives all it has: the mp of the target are full
            defeat(userIndex);
            target.mp = target.maxMp;
            syncParty(target);
            return 2;
        case FF6_SP_FALLEN_ONE:
            target.hp = 1;
            syncParty(target);
            return 2;
        case FF6_SP_STUNNER:  // the status of the attack leave the target, if it hits
            if (random_.byte() < a.hit) clearStatus(targetIndex, a.status);
            return 2;
        case FF6_SP_DEBILITATOR: {  // a new element that it fears, and nothing absorbs or resists it any more
            const int open = ~target.elemWeak & 0xFF;
            if (!open) return 0;
            std::vector<int> bits;
            for (int bit = 0; bit < 8; ++bit) {
                if (open & (1 << bit)) bits.push_back(1 << bit);
            }
            const int element = bits[static_cast<size_t>(random_.below(static_cast<int>(bits.size())))];
            target.elemWeak |= element;
            target.elemHalf &= ~element;
            target.elemAbsorb &= ~element;
            return 2;
        }
        case FF6_SP_METAMORPH: {  // a monster falls and leaves an item, if it is the chance of its kind
            if (!target.monster) return 0;
            const int info = data_.monster(target.id)[FF6_MONSTER_PROP_METAMORPH];
            const int item = data_.metamorph[static_cast<size_t>((info & 0x1F) * 4 + random_.below(4))];
            static constexpr int kRates[8] = FF6_METAMORPH_RATES;
            if (random_.byte() >= kRates[info >> 5]) return 0;
            reward_.items.push_back(item);
            BattleEvent got;
            got.kind = BattleEvent::Kind::GotItem;
            got.attacker = userIndex;
            got.item = item;
            events_.push_back(got);
            defeat(targetIndex);
            return 2;
        }
        case FF6_SP_R_POLARITY: target.backRow = !target.backRow; return 2;
        case FF6_SP_WALLCHANGE: {  // one element that it fears, one that it absorbs, and nothing else gets through
            const int weak = 1 << random_.below(8), absorbed = 1 << random_.below(8);
            target.elemWeak = weak;
            target.elemNull = ~weak & 0xFF;
            target.elemAbsorb = absorbed;
            return 2;
        }
        case FF6_SP_STONE:
            if (user.level == target.level) a.bonus = FF6_STONE_BONUS;
            return 1;
        case FF6_SP_RIPPLER: {  // the caster and the target swap the status that the other one does not have
            constexpr uint32_t kSwapped = ~(FF6_ST_GONE | FF6_ST_NEAR_FATAL | FF6_ST_MORPH | FF6_ST_CONTROL | FF6_ST_HIDE | FF6_ST_INTERCEPTOR | FF6_ST_FLOAT);
            const uint32_t mine = user.status & ~target.status & kSwapped, theirs = target.status & ~user.status & kSwapped;
            clearStatus(userIndex, mine);
            clearStatus(targetIndex, theirs);
            setStatus(userIndex, theirs);
            setStatus(targetIndex, mine);
            return 2;
        }
        default: return 1;
    }
}

// What is left to do when the targets were hit.
void Battle::specialAfter(int userIndex, const Attack& a) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (a.special == FF6_SP_EXPLODER && user.alive()) {  // the caster is gone with the blast
        user.hp = 0;
        setStatus(userIndex, FF6_ST_DEAD);
        BattleEvent defeated;
        defeated.kind = BattleEvent::Kind::Defeated;
        defeated.target = userIndex;
        events_.push_back(defeated);
    }
}

}  // namespace ff6
