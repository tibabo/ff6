#include <algorithm>

#include "battle/battle.hpp"

namespace ff6 {

// The rages of the monsters that stand in the battle, for the ones that have one, are learned by Gau.
void Battle::learnRages() {
    for (const Combatant& c : combatants_) {
        if (c.monster && c.present && c.id < FF6_RAGE_MONSTERS) party_.rages[static_cast<size_t>(c.id)] = true;
    }
}

// Leap is for the veldt, and needs another character to stay.
bool Battle::canLeap() const {
    int standing = 0;
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) standing += combatants_[static_cast<size_t>(i)].alive();
    return veldt_ && standing >= 2;
}

void Battle::leap(int userIndex) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || !canLeap()) return;
    user.ready = false;
    user.gauge = 0;
    learnRages();
    gauLeaped_ = true;
    BattleEvent event;
    event.kind = BattleEvent::Kind::Leap;
    event.attacker = userIndex;
    events_.push_back(event);
    outcome_ = BattleOutcome::Won;  // the fight is over, with nothing to win
}

// Rage: the character becomes the monster, with what the monster fears and absorbs, and from then on it acts alone with the
// attacks of the monster.
void Battle::rage(int userIndex, int monster) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || monster < 0 || monster >= FF6_RAGE_MONSTERS ||
        !party_.rages[static_cast<size_t>(monster)]) {
        return;
    }
    user.ready = false;
    user.gauge = 0;
    user.rageMonster = monster;
    const uint8_t* prop = data_.monster(monster);
    user.elemAbsorb |= prop[FF6_MONSTER_PROP_ABSORB];
    user.elemWeak |= prop[FF6_MONSTER_PROP_WEAK];
    setStatus(userIndex, FF6_ST_RAGE);
    BattleEvent event;
    event.kind = BattleEvent::Kind::Rage;
    event.attacker = userIndex;
    event.attack = monster;
    events_.push_back(event);
}

// The turn of a character that is a monster: the blow, or the special attack of its rage, one time in two each.
void Battle::rageTurn(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    c.gauge = 0;
    std::vector<int> foes;
    for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {
        if (combatants_[static_cast<size_t>(i)].alive()) foes.push_back(i);
    }
    if (foes.empty()) return;
    const int attackId = data_.monsterRage[static_cast<size_t>(c.rageMonster) * 2 + static_cast<size_t>(random_.byte() & 1)];
    if (attackId >= FF6_ATTACK_BATTLE_BLOW) {  // the blow, or the special attack that a character cannot make
        blows(index, foes[static_cast<size_t>(random_.below(static_cast<int>(foes.size())))]);
        return;
    }
    BattleEvent announce;
    announce.kind = BattleEvent::Kind::Cast;
    announce.attacker = index;
    announce.attack = attackId;
    events_.push_back(announce);
    const Attack attack = spellAttack(attackId);
    perform(index, attack, targetsFor(index, attack, foes[static_cast<size_t>(random_.below(static_cast<int>(foes.size())))]), attackId, -1);
}

// One step of the dance (RandDance): one of its four attacks, the first one in seven times out of sixteen and the last in one.
void Battle::danceAttack(int index) {
    Combatant& c = combatants_[static_cast<size_t>(index)];
    static constexpr int kRates[] = FF6_DANCE_RATES;
    const int roll = random_.byte();
    int step = 0;
    for (const int limit : kRates) step += roll < limit;
    const int attackId = data_.danceProp[static_cast<size_t>(c.danceIndex) * FF6_DANCE_ATTACKS + static_cast<size_t>(step)];
    if (attackId == 0xFF) return;
    BattleEvent announce;
    announce.kind = BattleEvent::Kind::Cast;
    announce.attacker = index;
    announce.attack = attackId;
    events_.push_back(announce);
    const Attack attack = spellAttack(attackId);
    perform(index, attack, targetsFor(index, attack, -1), attackId, -1);
}

// Dance: if the field does not show the dance, the dance changes it half of the time, and fails the other half. Then the
// dancer goes on by itself, turn after turn.
void Battle::dance(int userIndex, int dance) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive() || dance < 0 || dance >= FF6_DANCE_COUNT ||
        !(party_.dances & (1u << dance))) {
        return;
    }
    user.ready = false;
    user.gauge = 0;
    user.danceIndex = dance;
    const bool matches = background_ >= 0 && background_ < FF6_BG_DANCE_BYTES && data_.battleBgDance[static_cast<size_t>(background_)] == dance;
    if (!matches && (random_.byte() & 1) == 0) {
        user.danceIndex = -1;
        BattleEvent failed;
        failed.kind = BattleEvent::Kind::DanceFailed;
        failed.attacker = userIndex;
        events_.push_back(failed);
        return;
    }
    BattleEvent started;
    started.kind = BattleEvent::Kind::Dance;
    started.attacker = userIndex;
    started.damage = dance;
    events_.push_back(started);
    if (!matches) {
        background_ = data_.danceBg[static_cast<size_t>(dance)];
        BattleEvent changed;
        changed.kind = BattleEvent::Kind::Background;
        changed.damage = background_;
        events_.push_back(changed);
    }
    setStatus(userIndex, FF6_ST_DANCE);
    danceAttack(userIndex);
    checkEnd();
}

// Slot (GetSlotAttack, SlotAttackTbl): three alike give the attack of their icon, seven, seven and bar kill the party, and
// anything else is the rabbit that does nothing.
void Battle::slot(int userIndex, const std::array<int, 3>& reels) {
    Combatant& user = combatants_[static_cast<size_t>(userIndex)];
    if (outcome_ != BattleOutcome::Fighting || user.monster || !user.ready || !user.alive()) return;
    user.ready = false;
    user.gauge = 0;
    int result = 7;
    if (reels[0] == reels[1] && reels[1] == reels[2]) result = reels[0] + 1;
    else if (reels[0] == FF6_SLOT_ICON_SEVEN && reels[1] == FF6_SLOT_ICON_SEVEN && reels[2] == FF6_SLOT_ICON_BAR) result = 0;
    BattleEvent shown;
    shown.kind = BattleEvent::Kind::Slot;
    shown.attacker = userIndex;
    shown.damage = result;
    events_.push_back(shown);
    static constexpr int kAttacks[] = {FF6_ATTACK_L5_DOOM, FF6_ATTACK_L5_DOOM, FF6_ATTACK_BAHAMUT, -1, FF6_ATTACK_H_BOMB,
                                       FF6_ATTACK_CHOCOBOP, FF6_ATTACK_SEVEN_FLUSH, FF6_ATTACK_LAGOMORPH};
    int attackId = kAttacks[result];
    if (attackId == FF6_ATTACK_LAGOMORPH) {
        BattleEvent nothing;
        nothing.kind = BattleEvent::Kind::NoEffect;
        nothing.attacker = userIndex;
        events_.push_back(nothing);
        return;
    }
    if (attackId < 0) attackId = FF6_FIRST_GENJU_ATTACK + random_.below(FF6_ESPER_COUNT);  // the bar summons any esper
    Attack attack = spellAttack(attackId);
    attack.mp = 0;
    std::vector<int> targets;
    if (result < 2) {  // the joker doom: all the characters, or all the monsters
        attack.flags = (attack.flags & ~FF6_AF_LEVEL_DIV) | FF6_AF_INSTANT_DEATH | FF6_AF_NO_DODGE | FF6_AF_NO_REFLECT;
        attack.status = FF6_ST_DEAD;
        attack.target = FF6_TARGET_INIT_ALL;
        for (int i = 0; i < FF6_BATTLE_COMBATANTS; ++i) {
            if (combatants_[static_cast<size_t>(i)].alive() && (i < FF6_BATTLE_PARTY_SLOTS) == (result == 0)) targets.push_back(i);
        }
    } else {
        targets = targetsFor(userIndex, attack, -1);
    }
    BattleEvent announce;
    announce.kind = BattleEvent::Kind::Cast;
    announce.attacker = userIndex;
    announce.attack = attackId;
    events_.push_back(announce);
    perform(userIndex, attack, targets, attackId, -1);
    checkEnd();
}

}  // namespace ff6
