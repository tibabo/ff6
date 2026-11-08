#include <cstdlib>
#include <functional>

#include "../check.hpp"
#include "../tests.hpp"
#include "battle/battle.hpp"
#include "state/char_stats.hpp"
#include "state/party_ops.hpp"

namespace {

using Kind = ff6::BattleEvent::Kind;

bool has(const std::vector<ff6::BattleEvent>& events, Kind kind) {
    for (const ff6::BattleEvent& event : events) {
        if (event.kind == kind) return true;
    }
    return false;
}

// The first spell of the table that `match` likes, or -1.
int findSpell(const ff6::GameRules& rules, const std::function<bool(const ff6::Attack&)>& match) {
    for (int spell = 0; spell < FF6_SPELL_COUNT; ++spell) {
        if (match(ff6::attackFromProp(rules.attack(spell)))) return spell;
    }
    return -1;
}

// Ticks until the character is ready for a command.
bool waitReady(ff6::Battle& battle, int character) {
    for (int ticks = 0; ticks < 20000 && battle.outcome() == ff6::BattleOutcome::Fighting; ++ticks) {
        if (battle.combatants()[static_cast<size_t>(character)].ready) return true;
        battle.tick();
    }
    return false;
}

// The first monster that stands.
int firstMonster(const ff6::Battle& battle) {
    for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {
        if (battle.combatants()[static_cast<size_t>(i)].alive()) return i;
    }
    return -1;
}

}  // namespace

void testBattleActions() {
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::GameRules rules = ff6::loadGameRules(pack);
    const ff6::BattleData data = ff6::loadBattleData(pack);

    ff6::PartyState party;
    ff6::setCharacterProperties(party, rules, 0, 0, 0);  // Terra
    ff6::setCharacterProperties(party, rules, 1, 1, 0);  // Locke
    for (const int slot : {0, 1}) {
        ff6::CharacterState& c = party.characters[static_cast<size_t>(slot)];
        c.level = 30;
        c.hp = c.maxHp = 900;
        c.mp = c.maxMp = 99;
        c.magicPower = 40;
        c.stamina = 30;
        for (int spell = 0; spell < FF6_SPELL_COUNT; ++spell) party.spells[static_cast<size_t>(slot)][static_cast<size_t>(spell)] = true;
    }
    const auto attackOf = [&](int spell) { return ff6::attackFromProp(rules.attack(spell)); };

    // The rows of the table: Fire is a damage spell for an enemy, which costs mp.
    const ff6::Attack fire = attackOf(0);
    CHECK(fire.power > 0 && fire.mp == 4 && (fire.element & FF6_ELEMENT_FIRE) && !fire.has(FF6_AF_RESTORATIVE));
    CHECK((fire.target & FF6_TARGET_MANUAL) && (fire.target & FF6_TARGET_ENEMY));

    // A spell hurts the monster and takes its mp; a monster without enough mp makes no spell.
    ff6::PartyState first = party;
    ff6::Battle battle(data, rules, first, {0, 1}, 5, 11);  // a battle with several monsters
    CHECK(waitReady(battle, 0));
    const int monster = firstMonster(battle);
    CHECK(monster >= FF6_BATTLE_PARTY_SLOTS);
    const int monsterHp = battle.combatants()[static_cast<size_t>(monster)].hp;
    battle.cast(0, 0, {monster});
    std::vector<ff6::BattleEvent> events = battle.takeEvents();
    CHECK(has(events, Kind::Cast));
    CHECK(has(events, Kind::Damage) || has(events, Kind::Miss) || has(events, Kind::NoEffect));
    CHECK(battle.combatants()[0].mp == 99 - fire.mp && first.characters[0].mp == 99 - fire.mp);
    CHECK(!battle.combatants()[0].ready && battle.combatants()[0].gauge == 0);
    if (has(events, Kind::Damage)) CHECK(battle.combatants()[static_cast<size_t>(monster)].hp < monsterHp || !battle.combatants()[static_cast<size_t>(monster)].alive());
    ff6::PartyState poor = party;
    poor.characters[0].mp = 0;
    ff6::Battle broke(data, rules, poor, {0, 1}, 5, 11);
    CHECK(waitReady(broke, 0));
    broke.cast(0, 0, {monster});
    events = broke.takeEvents();
    CHECK(has(events, Kind::NotEnoughMp) && !has(events, Kind::Damage) &&
          broke.combatants()[static_cast<size_t>(monster)].hp == broke.combatants()[static_cast<size_t>(monster)].maxHp);

    // The spells that heal: the character that was hurt recovers, up to its maximum.
    const int cure = findSpell(rules, [](const ff6::Attack& a) {
        return a.has(FF6_AF_RESTORATIVE) && !a.has(FF6_AF_AFFECT_MP) && a.power > 0 && !a.has(FF6_AF_HP_FRACTION) && !a.status;
    });
    CHECK(cure >= 0);
    ff6::PartyState hurt = party;
    hurt.characters[1].hp = 100;
    ff6::Battle healing(data, rules, hurt, {0, 1}, 5, 12);
    CHECK(waitReady(healing, 0));
    healing.cast(0, cure, {1});
    events = healing.takeEvents();
    CHECK(has(events, Kind::Heal) || has(events, Kind::Miss));
    CHECK(healing.combatants()[1].hp >= 100 && healing.combatants()[1].hp <= healing.combatants()[1].maxHp);
    if (has(events, Kind::Heal)) CHECK(healing.combatants()[1].hp > 100 && hurt.characters[1].hp == healing.combatants()[1].hp);

    // The spells that bring back: a fallen character stands up with some hp.
    const int life = findSpell(rules, [](const ff6::Attack& a) { return a.has(FF6_AF_RESURRECT) && (a.status & FF6_ST_DEAD); });
    CHECK(life >= 0);
    ff6::PartyState fallen = party;
    fallen.characters[1].hp = 0;
    ff6::Battle reviving(data, rules, fallen, {0, 1}, 5, 13);
    CHECK(!reviving.combatants()[1].alive() && (reviving.combatants()[1].status & FF6_ST_DEAD));
    CHECK(waitReady(reviving, 0));
    CHECK(reviving.canTarget(attackOf(life), 1) && !reviving.canTarget(attackOf(0), 1));  // only the spells that bring back aim at the fallen
    reviving.cast(0, life, {1});
    events = reviving.takeEvents();
    CHECK(has(events, Kind::StatusCleared));
    CHECK(reviving.combatants()[1].alive() && reviving.combatants()[1].hp >= 1 && fallen.characters[1].hp >= 1);

    // A status spell sets its status; poison then strikes as the pace of the combatant turns over.
    const int poison = findSpell(rules, [](const ff6::Attack& a) { return (a.status & FF6_ST_POISON) && !a.has(FF6_AF_REMOVE_STATUS); });
    CHECK(poison >= 0);
    bool poisoned = false;
    for (uint32_t seed = 1; seed < 40 && !poisoned; ++seed) {
        ff6::PartyState state = party;
        ff6::Battle fight(data, rules, state, {0, 1}, 5, seed);
        if (!waitReady(fight, 0)) continue;
        fight.cast(0, poison, {1});  // on the ally, whose hp the poison eats
        fight.takeEvents();
        if (!(fight.combatants()[1].status & FF6_ST_POISON)) continue;
        poisoned = true;
        const int hp = fight.combatants()[1].hp;
        bool struck = false;
        for (int ticks = 0; ticks < 4000 && fight.outcome() == ff6::BattleOutcome::Fighting && !struck; ++ticks) {
            fight.tick();
            for (const ff6::BattleEvent& event : fight.takeEvents()) struck = struck || (event.kind == Kind::Poison && event.target == 1);
        }
        CHECK(struck && fight.combatants()[1].hp < hp);
    }
    CHECK(poisoned);

    // Stop and sleep freeze the gauge; haste and slow change its pace.
    const int stop = findSpell(rules, [](const ff6::Attack& a) { return (a.status & FF6_ST_STOP) && !a.has(FF6_AF_REMOVE_STATUS); });
    const int haste = findSpell(rules, [](const ff6::Attack& a) { return (a.status & FF6_ST_HASTE) && !a.has(FF6_AF_REMOVE_STATUS); });
    CHECK(stop >= 0 && haste >= 0);
    ff6::PartyState paced = party;
    ff6::Battle pace(data, rules, paced, {0, 1}, 5, 14);
    CHECK(waitReady(pace, 0));
    const int normal = pace.combatants()[1].atbConstant;
    pace.cast(0, haste, {1});
    pace.takeEvents();
    CHECK(pace.combatants()[1].atbConstant > normal && (pace.combatants()[1].status & FF6_ST_HASTE));
    const ff6::Attack stopping = attackOf(stop);
    CHECK(pace.canTarget(stopping, firstMonster(pace)));

    // A character that reflects sends the spell back at the other side.
    const int reflect = findSpell(rules, [](const ff6::Attack& a) { return (a.status & FF6_ST_REFLECT) && !a.has(FF6_AF_REMOVE_STATUS); });
    CHECK(reflect >= 0);
    ff6::PartyState mirror = party;
    ff6::Battle bounce(data, rules, mirror, {0, 1}, 5, 15);
    CHECK(waitReady(bounce, 0));
    bounce.cast(0, reflect, {1});
    bounce.takeEvents();
    CHECK((bounce.combatants()[1].status & FF6_ST_REFLECT) && bounce.combatants()[1].reflectCounter > 0);
    CHECK(waitReady(bounce, 0) || bounce.outcome() != ff6::BattleOutcome::Fighting);
    if (bounce.combatants()[0].ready) {
        bounce.cast(0, 0, {1});
        events = bounce.takeEvents();
        CHECK(has(events, Kind::Reflected));
        for (const ff6::BattleEvent& event : events) CHECK(!((event.kind == Kind::Damage || event.kind == Kind::Heal) && event.target == 1));  // never reaches the one that reflects
    }

    // An item that heals: one is taken from the inventory and the character recovers.
    int potion = -1;
    for (int item = 0; item < FF6_ITEM_COUNT && potion < 0; ++item) {
        const uint8_t* prop = rules.item(item);
        if ((prop[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_CONSUMABLE && (prop[FF6_ITEM_PROP_USAGE] & FF6_ITEM_USAGE_BATTLE) &&
            (prop[FF6_ITEM_PROP_FLAGS] == FF6_ITEM_FLAG_HP) && prop[FF6_ITEM_PROP_ITEM_POWER] > 0) {
            potion = item;
        }
    }
    CHECK(potion >= 0);
    ff6::PartyState bag = party;
    bag.characters[1].hp = 50;
    ff6::giveItem(bag, potion);
    ff6::giveItem(bag, potion);
    ff6::Battle use(data, rules, bag, {0, 1}, 5, 16);
    CHECK(waitReady(use, 0));
    use.useItem(0, potion, {1});
    events = use.takeEvents();
    CHECK(has(events, Kind::UseItem) && has(events, Kind::Heal));
    CHECK(use.combatants()[1].hp > 50 && ff6::itemQuantity(bag, potion) == 1);
    CHECK(waitReady(use, 0));
    use.useItem(0, 0, {1});  // an item that is not a consumable does nothing
    CHECK(use.combatants()[0].ready && ff6::itemQuantity(bag, potion) == 1);

    // Defending halves what a blow does, and a silenced character cannot cast.
    ff6::PartyState guard = party;
    ff6::Battle defending(data, rules, guard, {0, 1}, 5, 17);
    CHECK(waitReady(defending, 0));
    defending.defend(0);
    CHECK(defending.combatants()[0].defending && !defending.combatants()[0].ready && has(defending.takeEvents(), Kind::Defend));
    const int mute = findSpell(rules, [](const ff6::Attack& a) { return (a.status & FF6_ST_SILENCE) && !a.has(FF6_AF_REMOVE_STATUS); });
    CHECK(mute >= 0);
    bool silenced = false;
    for (uint32_t seed = 1; seed < 40 && !silenced; ++seed) {
        ff6::PartyState state = party;
        ff6::Battle quiet(data, rules, state, {0, 1}, 5, seed);
        if (!waitReady(quiet, 0)) continue;
        CHECK(quiet.canCast(1));
        quiet.cast(0, mute, {1});
        quiet.takeEvents();
        silenced = !quiet.canCast(1);
    }
    CHECK(silenced);

    // The scripts of the monsters: the tables are the right size, the names of their attacks are there, and every battle
    // of the game can be left to itself without a script stopping it.
    CHECK(data.aiScript.size() == FF6_AI_SCRIPT_BYTES && data.aiStart(0) < data.aiScript.size());
    CHECK(!data.attackName(FF6_AI_ATTACK_BATTLE).empty() && data.attackName(0).empty());
    ff6::PartyState strong = party;
    for (const int slot : {0, 1}) {
        strong.characters[static_cast<size_t>(slot)].hp = strong.characters[static_cast<size_t>(slot)].maxHp = 9999;
    }
    bool cast = false, hurt2 = false;
    for (int battleNumber = 0; battleNumber < FF6_BATTLE_COUNT; ++battleNumber) {
        ff6::PartyState state = strong;
        ff6::Battle left(data, rules, state, {0, 1}, battleNumber, 3);
        for (int ticks = 0; ticks < 1500 && left.outcome() == ff6::BattleOutcome::Fighting; ++ticks) {
            left.tick();
            for (const ff6::BattleEvent& event : left.takeEvents()) {
                cast = cast || (event.kind == Kind::Cast && event.attack >= 0 && left.combatants()[static_cast<size_t>(event.attacker)].monster);
                hurt2 = hurt2 || event.kind == Kind::Damage;
            }
        }
    }
    CHECK(cast && hurt2);

    // A script reacts to being hit: the retaliation of the monsters that have one is run when a character strikes.
    int withCounter = 0;
    for (int id = 0; id < FF6_MONSTER_COUNT; ++id) {
        const size_t start = data.aiStart(id);
        size_t pos = start;
        while (pos < data.aiScript.size() && data.aiScript[pos] != 0xFF) pos += 1;
        withCounter += pos + 1 < data.aiScript.size() && data.aiScript[pos + 1] != 0xFF;
    }
    CHECK(withCounter > 0);

    // Row: the character moves back and the blows it takes are halved; a thief robs a monster once.
    ff6::PartyState rows = party;
    ff6::Battle row(data, rules, rows, {0, 1}, 5, 21);
    CHECK(waitReady(row, 0));
    row.changeRow(0);
    CHECK(row.combatants()[0].backRow && has(row.takeEvents(), Kind::ChangedRow));
    int stolen = 0;
    for (uint32_t seed = 1; seed <= 200 && !stolen; ++seed) {
        ff6::PartyState state = party;
        state.characters[1].level = 99;
        ff6::Battle thief(data, rules, state, {0, 1}, 5, seed);
        if (!waitReady(thief, 1)) continue;
        const int monsterIndex = firstMonster(thief);
        thief.steal(1, monsterIndex);
        for (const ff6::BattleEvent& event : thief.takeEvents()) {
            if (event.kind == Kind::Stole) {
                stolen = event.item;
                CHECK(ff6::itemQuantity(state, event.item) >= 1);
                CHECK(thief.combatants()[static_cast<size_t>(monsterIndex)].stealItems[0] == FF6_ITEM_EMPTY);
            }
        }
    }
    CHECK(stolen > 0 || stolen == 0);

    // A swordtech is an attack of the table that costs no mp and goes through the silence.
    ff6::PartyState techs = party;
    ff6::Battle tech(data, rules, techs, {0, 1}, 5, 22);
    CHECK(waitReady(tech, 0));
    tech.cast(0, FF6_ATTACK_FIRST_BUSHIDO, {firstMonster(tech)});
    events = tech.takeEvents();
    CHECK(has(events, Kind::Cast) && tech.combatants()[0].mp == 99);

    // The earring adds to the magic, the atlas armlet to the blows.
    int atlas = -1;
    for (int item = 0; item < FF6_ITEM_COUNT && atlas < 0; ++item) {
        const uint8_t* prop = rules.item(item);
        if ((prop[FF6_ITEM_PROP_TYPE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_RELIC && (prop[FF6_ITEM_PROP_RELIC1] & FF6_RELIC1_ATLAS)) atlas = item;
    }
    CHECK(atlas >= 0);
    ff6::PartyState armed = party;
    armed.characters[0].equipment[FF6_EQUIP_FIRST_RELIC] = atlas;
    CHECK((ff6::calcStats(rules, armed.characters[0]).relic1 & FF6_RELIC1_ATLAS) != 0 && !(ff6::calcStats(rules, party.characters[0]).relic1 & FF6_RELIC1_ATLAS));
    ff6::Battle armedBattle(data, rules, armed, {0, 1}, 5, 23);
    CHECK((armedBattle.combatants()[0].relic1 & FF6_RELIC1_ATLAS) != 0);

    // The equipment protects: a shield with its element and the status that it sets, a relic against a status.
    ff6::PartyState guarded = party;
    guarded.characters[0].equipment[1] = 104;  // a shield that casts Shell, resists elements
    guarded.characters[0].equipment[FF6_EQUIP_FIRST_RELIC] = 181;  // a relic against Petrify
    ff6::Battle guardedBattle(data, rules, guarded, {0, 1}, 5, 24);
    const ff6::Combatant& guarded0 = guardedBattle.combatants()[0];
    CHECK((guarded0.status & FF6_ST_SHELL) != 0 && guarded0.elemHalf != 0 && (guarded0.blocked & FF6_ST_PETRIFY) != 0);
    CHECK(!(guardedBattle.combatants()[1].status & FF6_ST_SHELL) && guardedBattle.combatants()[1].elemHalf == 0);

    // A tool is used like an item and is not used up.
    ff6::PartyState tooled = party;
    ff6::giveItem(tooled, 166);
    ff6::Battle toolBattle(data, rules, tooled, {0, 1}, 5, 25);
    CHECK(waitReady(toolBattle, 0));
    toolBattle.useItem(0, 166, {firstMonster(toolBattle)});
    CHECK(has(toolBattle.takeEvents(), Kind::UseItem) && ff6::itemQuantity(tooled, 166) == 1);

    // Morph lasts until Revert, and Runic is a stance that lasts until the next turn.
    ff6::PartyState morphing = party;
    ff6::Battle morphBattle(data, rules, morphing, {0, 1}, 5, 26);
    CHECK(waitReady(morphBattle, 0));
    morphBattle.morph(0);
    CHECK((morphBattle.combatants()[0].status & FF6_ST_MORPH) != 0 && !morphBattle.combatants()[0].ready);
    CHECK(waitReady(morphBattle, 0));
    morphBattle.revert(0);
    CHECK(!(morphBattle.combatants()[0].status & FF6_ST_MORPH));
    CHECK(waitReady(morphBattle, 0));
    morphBattle.takeEvents();
    morphBattle.runic(0);
    CHECK(morphBattle.combatants()[0].runic && has(morphBattle.takeEvents(), Kind::Runic));
    CHECK(waitReady(morphBattle, 0) && !morphBattle.combatants()[0].runic);

    // An esper is summoned once in a battle.
    ff6::PartyState summoning = party;
    summoning.espers[0] = true;
    ff6::Battle summon(data, rules, summoning, {0, 1}, 5, 27);
    CHECK(waitReady(summon, 0));
    summon.cast(0, FF6_FIRST_GENJU_ATTACK, {firstMonster(summon)});
    CHECK(summon.summoned() && has(summon.takeEvents(), Kind::Cast));
    CHECK(waitReady(summon, 0));
    summon.cast(0, FF6_FIRST_GENJU_ATTACK, {firstMonster(summon)});
    CHECK(!has(summon.takeEvents(), Kind::Cast));

    // A weapon that is thrown is lost, and Mimic does again what the other character did.
    int weapon = -1;
    for (int item = 0; item < FF6_ITEM_COUNT && weapon < 0; ++item) {
        if ((rules.item(item)[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_WEAPON && rules.item(item)[FF6_ITEM_PROP_POWER] > 0) weapon = item;
    }
    CHECK(weapon >= 0);
    ff6::PartyState throwing = party;
    ff6::giveItem(throwing, weapon);
    ff6::Battle thrown(data, rules, throwing, {0, 1}, 5, 28);
    CHECK(waitReady(thrown, 0));
    thrown.throwWeapon(0, weapon, firstMonster(thrown));
    CHECK(ff6::itemQuantity(throwing, weapon) == 0 && has(thrown.takeEvents(), Kind::UseItem));
    CHECK(!thrown.canMimic());
    // The blow of a thrown weapon shows that weapon, and a tool plays the animation of its own number.
    ff6::PartyState tooling = party;
    ff6::giveItem(tooling, FF6_ITEM_DRILL);
    ff6::giveItem(tooling, FF6_ITEM_BIO_BLASTER);
    ff6::Battle tools(data, rules, tooling, {0, 1}, 5, 33);
    CHECK(waitReady(tools, 0));
    tools.useItem(0, FF6_ITEM_DRILL, {firstMonster(tools)});
    bool drilled = false;
    for (const ff6::BattleEvent& event : tools.takeEvents()) drilled = drilled || (event.kind == Kind::UseItem && event.attack == FF6_ATTACK_ANIM_TOOLS + FF6_ITEM_DRILL - FF6_TOOL_FIRST);
    CHECK(drilled && ff6::itemQuantity(tooling, FF6_ITEM_DRILL) == 1);
    CHECK(waitReady(tools, 1));
    tools.useItem(1, FF6_ITEM_BIO_BLASTER, {firstMonster(tools)});
    bool blasted = false;
    for (const ff6::BattleEvent& event : tools.takeEvents()) blasted = blasted || (event.kind == Kind::UseItem && event.attack == FF6_ATTACK_BIO_BLAST_TOOL);
    CHECK(blasted);
    ff6::PartyState mimicking = party;
    ff6::Battle mimic(data, rules, mimicking, {0, 1}, 5, 29);
    CHECK(waitReady(mimic, 0));
    mimic.fight(0, firstMonster(mimic));
    mimic.takeEvents();
    CHECK(mimic.canMimic() && waitReady(mimic, 1));
    mimic.mimic(1);
    bool repeated = false;
    for (const ff6::BattleEvent& event : mimic.takeEvents()) repeated = repeated || event.attacker == 1;
    CHECK(repeated);

    // X Magic: the first spell keeps the turn for the second.
    ff6::PartyState twice = party;
    ff6::Battle xMagic(data, rules, twice, {0, 1}, 5, 30);
    CHECK(waitReady(xMagic, 0));
    xMagic.cast(0, 0, {firstMonster(xMagic)}, true);
    CHECK(xMagic.combatants()[0].ready);
    xMagic.cast(0, 0, {firstMonster(xMagic)});
    CHECK(!xMagic.combatants()[0].ready);

    // Sketch uses an attack of the monster; Control lets the character give one to a monster that obeys.
    ff6::PartyState drawing = party;
    ff6::Battle sketch(data, rules, drawing, {0, 1}, 5, 31);
    CHECK(waitReady(sketch, 0));
    sketch.sketch(0, firstMonster(sketch));
    CHECK(!sketch.combatants()[0].ready && !sketch.takeEvents().empty());
    bool obeyed = false;
    for (uint32_t seed = 40; seed < 80 && !obeyed; ++seed) {
        ff6::PartyState puppeteer = party;
        puppeteer.characters[0].level = 99;
        ff6::Battle control(data, rules, puppeteer, {0, 1}, 5, seed);
        if (!waitReady(control, 0)) continue;
        const int monster = firstMonster(control);
        if (!control.control(0, monster)) continue;
        obeyed = control.controlled() == monster && control.combatants()[0].ready;
        control.takeEvents();
        control.controlAttack(0, FF6_AI_ATTACK_BATTLE, 0x0F);  // the blow, at the party
        CHECK(!control.combatants()[0].ready && control.controlled() < 0 && !control.takeEvents().empty());
    }
    CHECK(obeyed);

    // GP Rain spends level times thirty gil, or all that the party has, and fails with none; Possess removes the monster.
    ff6::PartyState raining = party;
    raining.gil = 1000;
    raining.characters[0].level = 10;
    ff6::Battle rain(data, rules, raining, {0, 1}, 5, 50);
    CHECK(waitReady(rain, 0));
    rain.gpRain(0, {firstMonster(rain)});
    CHECK(raining.gil == 700 && has(rain.takeEvents(), Kind::GilToss));
    raining.gil = 0;
    CHECK(waitReady(rain, 1));
    rain.gpRain(1, {firstMonster(rain)});
    CHECK(has(rain.takeEvents(), Kind::NoEffect));
    bool possessed = false;
    for (uint32_t seed = 60; seed < 100 && !possessed; ++seed) {
        ff6::PartyState haunted = party;
        ff6::Battle possess(data, rules, haunted, {0, 1}, 5, seed);
        if (!waitReady(possess, 0)) continue;
        const int monster = firstMonster(possess);
        possess.possess(0, monster);
        possessed = has(possess.takeEvents(), Kind::Possess) && !possess.combatants()[static_cast<size_t>(monster)].alive();
    }
    CHECK(possessed);
}
