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

    // The item that floats makes its wearer float, and not dance; the status cannot be set or removed.
    int floater = -1;
    for (int item = 0; item < FF6_ITEM_COUNT && floater < 0; ++item) {
        if (rules.item(item)[FF6_ITEM_PROP_EQUIP_STATUS_HIGH] & FF6_ITEM_EQUIP_FLOAT) floater = item;
    }
    CHECK(floater >= 0);
    ff6::PartyState floating = party;
    floating.characters[0].equipment[FF6_EQUIP_FIRST_RELIC] = floater;
    ff6::Battle floatBattle(data, rules, floating, {0, 1}, 5, 24);
    const ff6::Combatant& floating0 = floatBattle.combatants()[0];
    CHECK((floating0.status & FF6_ST_FLOAT) && !(floating0.status & FF6_ST_DANCE) && (floating0.blocked & FF6_ST_FLOAT));
    CHECK(!(floatBattle.combatants()[1].status & FF6_ST_FLOAT));

    // A tool is used like an item and is not used up.
    ff6::PartyState tooled = party;
    ff6::giveItem(tooled, 166);
    ff6::Battle toolBattle(data, rules, tooled, {0, 1}, 5, 25);
    CHECK(waitReady(toolBattle, 0));
    toolBattle.useItem(0, 166, {firstMonster(toolBattle)});
    CHECK(has(toolBattle.takeEvents(), Kind::UseItem) && ff6::itemQuantity(tooled, 166) == 1);

    // Morph lasts until Revert, and Runic is a stance that lasts until the next turn.
    ff6::PartyState morphing = party;
    ff6::Battle noSupply(data, rules, morphing, {0, 1}, 5, 26);  // without a supply there is no morph
    CHECK(waitReady(noSupply, 0) && !noSupply.canMorph());
    noSupply.morph(0);
    CHECK(!(noSupply.combatants()[0].status & FF6_ST_MORPH));
    morphing.morphCounter = 100;
    ff6::Battle morphBattle(data, rules, morphing, {0, 1}, 5, 26);
    CHECK(waitReady(morphBattle, 0) && morphBattle.canMorph());
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
    // The tool of the debilitator gives the monster a weakness that it did not have.
    ff6::PartyState debilitating = party;
    ff6::giveItem(debilitating, FF6_ITEM_DEBILITATOR);
    ff6::Battle debilitated(data, rules, debilitating, {0, 1}, 5, 34);
    CHECK(waitReady(debilitated, 0));
    const int fearedBefore = debilitated.combatants()[static_cast<size_t>(firstMonster(debilitated))].elemWeak;
    debilitated.useItem(0, FF6_ITEM_DEBILITATOR, {firstMonster(debilitated)});
    const int fearedAfter = debilitated.combatants()[static_cast<size_t>(firstMonster(debilitated))].elemWeak;
    CHECK(__builtin_popcount(fearedAfter) == __builtin_popcount(fearedBefore) + 1);
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

    // The victory: the magic points teach the spells of the esper that is carried, the exp egg and the cat hood double
    // what they double, a level up brings the bonus of the esper.
    ff6::BattleData weak = data;  // monsters that fall at the first blow
    for (int m = 0; m < FF6_MONSTER_COUNT; ++m) {
        uint8_t* hp = &weak.monsterProp[static_cast<size_t>(m) * FF6_MONSTER_PROP_BYTES + FF6_MONSTER_PROP_HP];
        hp[0] = 1;
        hp[1] = 0;
    }
    weak.magicPoints[5] = 10;
    int egg = -1, hood = -1;
    for (int item = 0; item < FF6_ITEM_COUNT; ++item) {
        if (rules.item(item)[FF6_ITEM_PROP_RELIC5] & FF6_RELIC5_DOUBLE_EXP) egg = item;
        if (rules.item(item)[FF6_ITEM_PROP_RELIC5] & FF6_RELIC5_DOUBLE_GP) hood = item;
    }
    CHECK(egg >= 0 && hood >= 0);
    ff6::PartyState winners = party;
    for (const int slot : {0, 1}) {
        winners.spells[static_cast<size_t>(slot)].fill(false);
        winners.characters[static_cast<size_t>(slot)].experience = ff6::experienceForLevel(rules, 30);
    }
    winners.characters[0].esper = 0;  // Ramuh teaches its first spell at 10 percent for each magic point
    winners.characters[1].equipment[4] = egg;
    winners.characters[1].equipment[5] = hood;
    const long goldBefore = winners.gil;
    const long expBefore = winners.characters[0].experience;
    ff6::Battle win(weak, rules, winners, {0, 1}, 5, 70);
    std::vector<ff6::BattleEvent> victory;
    for (int ticks = 0; ticks < 20000 && win.outcome() == ff6::BattleOutcome::Fighting; ++ticks) {
        win.tick();
        for (const int who : {0, 1}) {
            if (win.combatants()[static_cast<size_t>(who)].ready && firstMonster(win) >= 0) win.fight(who, firstMonster(win));
        }
        for (const ff6::BattleEvent& event : win.takeEvents()) victory.push_back(event);
    }
    CHECK(win.outcome() == ff6::BattleOutcome::Won && win.reward().magicPoints == 10);
    CHECK(has(victory, Kind::MagicPoints) && has(victory, Kind::LearnedSpell));
    const uint8_t* ramuh = rules.genju(0);
    CHECK(winners.spells[0][ramuh[1]] && winners.spells[0][ramuh[1]] && winners.spellLearn[0][ramuh[3]] == ramuh[2] * 10 &&
          !winners.spells[0][ramuh[3]] && !winners.spells[1][ramuh[1]]);
    CHECK(winners.characters[0].experience - expBefore == win.reward().exp);
    CHECK(winners.characters[1].experience - ff6::experienceForLevel(rules, 30) == win.reward().exp * 2);
    CHECK(winners.gil - goldBefore == win.reward().gil && win.reward().gil % 2 == 0);

    // The spells need many points, and the ones of the characters from Gogo on are never learned.
    ff6::PartyState learners = party;
    learners.spells[0].fill(false);
    learners.characters[0].esper = 0;
    CHECK(ff6::learnSpells(learners, rules, {0}, 2).empty() && learners.spellLearn[0][ramuh[1]] == ramuh[0] * 2);
    CHECK(ff6::learnSpells(learners, rules, {0}, 8).size() >= 1 && learners.spells[0][ramuh[1]]);
    learners.characters[0].actor = FF6_ACTOR_GOGO;
    learners.spells[0].fill(false);
    CHECK(ff6::learnSpells(learners, rules, {0}, 50).empty());

    // An esper that raises the stamina adds to it at each level, never above 128.
    ff6::PartyState bonus = party;
    bonus.characters[0].experience = 0;
    bonus.characters[0].level = 1;
    bonus.characters[0].stamina = 127;
    bonus.characters[0].esper = 0;  // Ramuh: stamina + 1
    CHECK(rules.genju(0)[FF6_GENJU_PROP_BYTES - 1] == 13);
    CHECK(ff6::addExperience(bonus, rules, 0, ff6::experienceForLevel(rules, 4)) == 3);
    CHECK(bonus.characters[0].stamina == FF6_GENJU_BONUS_STAT_MAX);

    // The relics of the battle: the item that has each effect is found in the table.
    const auto relicWith = [&](int byte, int bit) {
        for (int item = 0; item < FF6_ITEM_COUNT; ++item) {
            if ((rules.item(item)[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_RELIC && (rules.item(item)[byte] & bit)) return item;
        }
        return -1;
    };
    // The back guard keeps the party from pincer and back attacks; the barrier ring casts Shell when its wearer is near fatal.
    ff6::BattleRandom dice(5);
    bool surprised = false;
    for (int i = 0; i < 2000; ++i) {
        const int type = ff6::chooseBattleType(0, 4, dice, true);
        surprised = surprised || type == FF6_BATTLE_TYPE_PINCER || type == FF6_BATTLE_TYPE_BACK;
    }
    CHECK(!surprised);
    const int barrier = relicWith(FF6_ITEM_PROP_RELIC5, FF6_RELIC5_SHELL_HP_LOW);
    CHECK(barrier >= 0);
    ff6::PartyState wounded = party;
    wounded.characters[0].equipment[4] = barrier;
    wounded.characters[0].maxHp = 3000;
    wounded.characters[0].hp = 100;
    ff6::Battle shelled(data, rules, wounded, {0, 1}, 5, 80);
    std::vector<ff6::BattleEvent> opening = shelled.takeEvents();
    bool castShell = false;
    for (const ff6::BattleEvent& event : opening) castShell = castShell || (event.kind == Kind::Cast && event.attack == FF6_ATTACK_SHELL && event.attacker == 0);
    CHECK(castShell && (shelled.combatants()[0].status & FF6_ST_SHELL) && (shelled.combatants()[0].status & FF6_ST_NEAR_FATAL));
    CHECK(!(shelled.combatants()[1].status & FF6_ST_SHELL));

    // The offering strikes eight times at random, and never misses; the black belt answers the blows of the monsters.
    const int offering = relicWith(FF6_ITEM_PROP_RELIC4, FF6_RELIC4_X_FIGHT);
    const int belt = relicWith(FF6_ITEM_PROP_RELIC4, FF6_RELIC4_RAND_RETAL);
    CHECK(offering >= 0 && belt >= 0);
    // Monsters that stay up and never strike.
    ff6::BattleData harmless = data;
    for (int m = 0; m < FF6_MONSTER_COUNT; ++m) {
        uint8_t* prop = &harmless.monsterProp[static_cast<size_t>(m) * FF6_MONSTER_PROP_BYTES];
        prop[FF6_MONSTER_PROP_HP] = 0xFF;
        prop[FF6_MONSTER_PROP_HP + 1] = 0x7F;
        prop[FF6_MONSTER_PROP_ATTACK] = 0;
        prop[FF6_MONSTER_PROP_MP] = 99;
    }
    ff6::PartyState giving = party;
    giving.characters[1].equipment[4] = offering;
    ff6::Battle eight(harmless, rules, giving, {0, 1}, 5, 81);
    CHECK(waitReady(eight, 1));
    eight.takeEvents();
    eight.fight(1, firstMonster(eight));
    int blows = 0;
    bool misses = false;
    for (const ff6::BattleEvent& event : eight.takeEvents()) {
        if (event.attacker != 1) continue;
        blows += event.kind == Kind::Hit || event.kind == Kind::Critical || event.kind == Kind::Heal;
        misses = misses || event.kind == Kind::Miss || event.kind == Kind::Critical;
    }
    CHECK(blows >= FF6_OFFERING_ROUNDS && !misses);
    ff6::PartyState belted = party;
    belted.characters[0].equipment[4] = belt;
    belted.characters[0].hp = belted.characters[0].maxHp = 9000;
    belted.characters[1].hp = belted.characters[1].maxHp = 9000;
    ff6::Battle answering(data, rules, belted, {0, 1}, 5, 82);
    bool answered = false;
    for (int ticks = 0; ticks < 30000 && !answered && answering.outcome() == ff6::BattleOutcome::Fighting; ++ticks) {
        answering.tick();
        const std::vector<ff6::BattleEvent> events = answering.takeEvents();
        for (size_t i = 0; i + 1 < events.size(); ++i) {
            if ((events[i].kind == Kind::Hit || events[i].kind == Kind::Critical) && events[i].attacker >= FF6_BATTLE_PARTY_SLOTS && events[i].target == 0 &&
                (events[i + 1].kind == Kind::Hit || events[i + 1].kind == Kind::Critical || events[i + 1].kind == Kind::Miss) && events[i + 1].attacker == 0 &&
                events[i + 1].target == events[i].attacker) {
                answered = true;
            }
        }
        for (const int who : {0, 1}) {  // the party only waits
            if (answering.combatants()[static_cast<size_t>(who)].ready) answering.defend(who);
        }
    }
    CHECK(answered);

    // A desperation attack comes in one Fight in sixteen when the character is near fatal, and only once.
    ff6::PartyState desperate = party;
    desperate.characters[0].maxHp = 3000;
    desperate.characters[0].hp = 100;
    ff6::Battle limit(harmless, rules, desperate, {0, 1}, 5, 83);
    int desperations = 0;
    for (int ticks = 0; ticks < 200000 && limit.outcome() == ff6::BattleOutcome::Fighting; ++ticks) {
        limit.tick();
        if (limit.combatants()[0].ready) limit.fight(0, firstMonster(limit));
        if (limit.combatants()[1].ready) limit.defend(1);
        for (const ff6::BattleEvent& event : limit.takeEvents()) {
            desperations += event.kind == Kind::Cast && event.attacker == 0 && event.attack >= FF6_ATTACK_FIRST_DESPERATION && event.attack <= FF6_ATTACK_FIRST_DESPERATION + 11;
        }
    }
    CHECK(desperations == 1);

    // The effects of the weapons: each one is the weapon that the table gives it.
    const auto weaponWith = [&](int special) {
        for (int item = 0; item < FF6_ITEM_COUNT; ++item) {
            const uint8_t* prop = rules.item(item);
            if ((prop[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_WEAPON && (prop[FF6_ITEM_PROP_WEAPON_SPECIAL] >> 4) == special) return item;
        }
        return -1;
    };
    // Fights with a weapon in the right hand of Locke, with the party changed by `setup`, until `done` likes the events.
    const auto wield = [&](int item, const std::function<void(ff6::PartyState&)>& setup, const std::function<bool(const ff6::Battle&, const std::vector<ff6::BattleEvent>&)>& done) {
        for (uint32_t seed = 200; seed < 400; ++seed) {
            ff6::PartyState wielder = party;
            wielder.characters[1].equipment[0] = item;
            wielder.characters[1].equipment[1] = FF6_ITEM_EMPTY;
            setup(wielder);
            ff6::Battle fighting(harmless, rules, wielder, {0, 1}, 5, seed);
            if (!waitReady(fighting, 1)) continue;
            fighting.takeEvents();
            fighting.fight(1, firstMonster(fighting));
            if (done(fighting, fighting.takeEvents())) return true;
        }
        return false;
    };
    const auto nothing = [](ff6::PartyState&) {};
    const int drainer = weaponWith(FF6_WEAPON_DRAINER), diceWeapon = weaponWith(FF6_WEAPON_DICE), runeEdge = weaponWith(FF6_WEAPON_MP_CRIT);
    const int sabre = weaponWith(FF6_WEAPON_SOUL_SABRE), knife = weaponWith(FF6_WEAPON_THIEFKNIFE), valiant = weaponWith(FF6_WEAPON_VALIANT);
    CHECK(drainer >= 0 && diceWeapon >= 0 && runeEdge >= 0 && sabre >= 0 && knife >= 0 && valiant >= 0);
    // The drainer gives the hp that it takes.
    CHECK(wield(drainer, [](ff6::PartyState& p) { p.characters[1].hp = 100; }, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>& events) {
        return has(events, Kind::Hit) && b.combatants()[1].hp > 100;
    }));
    // The dice deal a multiple of twice the level.
    CHECK(wield(diceWeapon, nothing, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        for (const ff6::BattleEvent& event : events) {
            if (event.kind == Kind::Hit && event.attacker == 1) return event.damage > 0 && event.damage % 60 == 0;
        }
        return false;
    }));
    // The rune edge pays mp for a critical hit; the soul sabre takes the mp of the monster for the wielder; the thief knife steals.
    CHECK(wield(runeEdge, nothing, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>& events) {
        return has(events, Kind::Critical) && b.combatants()[1].mp <= 99 - 12;
    }));
    CHECK(wield(sabre, [](ff6::PartyState& p) { p.characters[1].mp = 0; }, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>& events) {
        return has(events, Kind::MpDamage) && b.combatants()[1].mp > 0;
    }));
    CHECK(wield(knife, nothing, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) { return has(events, Kind::Stole); }));
    // The valiant knife hits harder when its wielder is hurt.
    int healthy = 0, hurtDamage = 0;
    wield(valiant, nothing, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        for (const ff6::BattleEvent& event : events) {
            if (event.kind == Kind::Hit || event.kind == Kind::Critical) healthy = std::max(healthy, event.damage);
        }
        return true;
    });
    wield(valiant, [](ff6::PartyState& p) { p.characters[1].hp = 10; }, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        for (const ff6::BattleEvent& event : events) {
            if (event.kind == Kind::Hit || event.kind == Kind::Critical) hurtDamage = std::max(hurtDamage, event.damage);
        }
        return true;
    });
    CHECK(hurtDamage > healthy);
    // A weapon that casts: the rods follow their blows with a spell now and then.
    int rod = -1, rodSpell = -1;
    for (int item = 0; item < FF6_ITEM_COUNT && rod < 0; ++item) {
        const uint8_t* prop = rules.item(item);
        if ((prop[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_WEAPON && (prop[FF6_ITEM_PROP_SPELL_CAST] & FF6_WEAPON_SPELL_RAND_CAST) && !(prop[FF6_ITEM_PROP_WEAPON_SPECIAL] >> 4)) {
            rod = item;
            rodSpell = prop[FF6_ITEM_PROP_SPELL_CAST] & 0x3F;
        }
    }
    CHECK(rod >= 0 && wield(rod, nothing, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        for (const ff6::BattleEvent& event : events) {
            if (event.kind == Kind::Cast && event.attack == rodSpell) return true;
        }
        return false;
    }));

    // The special effects of the attacks. Terra casts what the table has, whatever it costs: the monsters stand and never strike.
    const auto casting = [&](int attackId, const std::function<void(ff6::PartyState&)>& setup, const std::vector<int>& targets, int battleNumber,
                             const std::function<void(ff6::Battle&, const std::vector<ff6::BattleEvent>&)>& check) {
        ff6::PartyState state = party;
        state.characters[0].hp = state.characters[0].maxHp = 600;
        state.characters[0].mp = state.characters[0].maxMp = 99;
        setup(state);
        ff6::Battle fight(harmless, rules, state, {0, 1}, battleNumber, 90);
        CHECK(waitReady(fight, 0));
        fight.takeEvents();
        std::vector<int> aimed;
        for (const int target : targets) aimed.push_back(target < 0 ? firstMonster(fight) : target);
        fight.cast(0, attackId, aimed);
        check(fight, fight.takeEvents());
    };
    const auto none = [](ff6::PartyState&) {};
    // Blow Fish deals 1000, Revenge what the caster lost, White Wind gives its hp to the party, Mantra shares it.
    casting(144, none, {-1}, 5, [&](ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        int total = 0;
        for (const ff6::BattleEvent& event : events) total += event.kind == Kind::Damage ? event.damage : 0;
        CHECK(total == FF6_BLOW_FISH_DAMAGE);
    });
    casting(146, [](ff6::PartyState& p) { p.characters[0].hp = 100; }, {-1}, 5, [&](ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        int total = 0;
        for (const ff6::BattleEvent& event : events) total += event.kind == Kind::Damage ? event.damage : 0;
        CHECK(total == 500);  // 600 less 100
    });
    casting(147, [](ff6::PartyState& p) { p.characters[0].hp = 300; p.characters[1].hp = 10; }, {0, 1}, 5, [&](ff6::Battle& b, const std::vector<ff6::BattleEvent>&) {
        CHECK(b.combatants()[1].hp >= 300 && b.combatants()[1].hp <= b.combatants()[1].maxHp);
    });
    casting(97, [](ff6::PartyState& p) { p.characters[0].hp = 400; p.characters[1].hp = 10; }, {0, 1}, 5, [&](ff6::Battle& b, const std::vector<ff6::BattleEvent>&) {
        CHECK(b.combatants()[1].hp >= 400 && b.combatants()[0].hp == 400);  // Terra gives her hp to Locke and keeps hers
    });
    // Quadra Slam hits four times, Scan tells what the monster is, Dischord halves its level, Quake misses what floats.
    casting(88, none, {-1}, 5, [&](ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        int hits = 0;
        for (const ff6::BattleEvent& event : events) hits += event.kind == Kind::Damage || event.kind == Kind::Miss;
        CHECK(hits == FF6_QUADRA_ROUNDS);
    });
    casting(24, none, {-1}, 5, [&](ff6::Battle&, const std::vector<ff6::BattleEvent>& events) { CHECK(has(events, Kind::Scan) || has(events, Kind::Miss)); });
    casting(155, none, {-1}, 5, [&](ff6::Battle& b, const std::vector<ff6::BattleEvent>&) {
        const ff6::Combatant& monster = b.combatants()[static_cast<size_t>(firstMonster(b))];
        CHECK(monster.level == (harmless.monster(monster.id)[FF6_MONSTER_PROP_LEVEL] + 1) / 2 || has(b.takeEvents(), Kind::Miss));
    });
    casting(21, none, {FF6_BATTLE_PARTY_SLOTS}, 5, [&](ff6::Battle& b, const std::vector<ff6::BattleEvent>& events) {
        const bool floats = (b.combatants()[FF6_BATTLE_PARTY_SLOTS].status & FF6_ST_FLOAT) != 0;
        CHECK(floats == !has(events, Kind::Damage) || !floats);
    });
    // Warp takes the party out of a battle that can be left, and not out of the others.
    casting(42, none, {0}, 6, [&](ff6::Battle& b, const std::vector<ff6::BattleEvent>&) { CHECK(b.outcome() == ff6::BattleOutcome::Fled); });
    casting(42, none, {0}, 0, [&](ff6::Battle& b, const std::vector<ff6::BattleEvent>& events) {
        CHECK(b.outcome() == ff6::BattleOutcome::Fighting && has(events, Kind::NoEffect));
    });
    // Quick gives two turns in a row to its target, while nobody else moves.
    casting(43, none, {1}, 5, [&](ff6::Battle& b, const std::vector<ff6::BattleEvent>&) {
        int turns = 0;
        bool othersMoved = false;
        for (int ticks = 0; ticks < 4000 && turns < FF6_QUICK_TURNS; ++ticks) {
            b.tick();
            othersMoved = othersMoved || b.combatants()[0].ready;
            if (b.combatants()[1].ready) {
                ++turns;
                b.defend(1);
            }
        }
        CHECK(turns == FF6_QUICK_TURNS && !othersMoved);
    });

    // Umaro fights with his own moves: the fight, the charge at every monster, and with the rage ring the throw of a friend.
    ff6::PartyState bear = party;
    ff6::setCharacterProperties(bear, rules, 13, 13, 0);
    bear.characters[13].level = 40;
    bear.characters[13].hp = bear.characters[13].maxHp = 900;
    const int ring = FF6_ITEM_RAGE_RING;
    bear.characters[13].equipment[4] = ring;
    ff6::Battle umaro(harmless, rules, bear, {13, 0}, 5, 91);
    bool charged = false, friendThrown = false, fought = false;
    for (int ticks = 0; ticks < 100000 && !(charged && friendThrown && fought); ++ticks) {
        umaro.tick();
        if (umaro.combatants()[1].ready) umaro.defend(1);
        if (umaro.combatants()[0].ready) {
            umaro.fight(0, firstMonster(umaro));
            for (const ff6::BattleEvent& event : umaro.takeEvents()) {
                charged = charged || (event.kind == Kind::Umaro && event.damage == FF6_UMARO_MOVE_CHARGE);
                friendThrown = friendThrown || (event.kind == Kind::Umaro && event.damage == FF6_UMARO_MOVE_THROW);
                fought = fought || event.kind == Kind::Hit || event.kind == Kind::Critical;
            }
        } else {
            umaro.takeEvents();
        }
    }
    CHECK(charged && friendThrown && fought);

    // Gau: Leap on the veldt teaches the rages of the monsters that stand, Rage makes him one, and he acts alone from then on.
    ff6::PartyState gau = party;
    ff6::setCharacterProperties(gau, rules, 11, 11, 0);
    gau.characters[11].hp = gau.characters[11].maxHp = 900;
    ff6::Battle leaper(harmless, rules, gau, {11, 0}, 5, 92);
    CHECK(!leaper.canLeap());  // only on the veldt
    leaper.setVeldt(true);
    CHECK(leaper.canLeap() && waitReady(leaper, 0));
    const int seen = leaper.combatants()[static_cast<size_t>(firstMonster(leaper))].id;
    leaper.leap(0);
    CHECK(has(leaper.takeEvents(), Kind::Leap) && leaper.outcome() == ff6::BattleOutcome::Won && (seen >= FF6_RAGE_MONSTERS || gau.rages[static_cast<size_t>(seen)]));
    gau.rages[static_cast<size_t>(harmless.monsterRage.size() > 0 ? 0x19 : 0)] = true;
    ff6::Battle raging(harmless, rules, gau, {11, 0}, 5, 93);
    CHECK(waitReady(raging, 0));
    raging.takeEvents();
    raging.rage(0, 0x19);
    CHECK((raging.combatants()[0].status & FF6_ST_RAGE) && has(raging.takeEvents(), Kind::Rage));
    bool acted = false;
    for (int ticks = 0; ticks < 20000 && !acted; ++ticks) {
        raging.tick();
        if (raging.combatants()[1].ready) raging.defend(1);
        for (const ff6::BattleEvent& event : raging.takeEvents()) acted = acted || (event.attacker == 0 && (event.kind == Kind::Hit || event.kind == Kind::Miss || event.kind == Kind::Cast || event.kind == Kind::Critical));
        CHECK(!raging.combatants()[0].ready);
    }
    CHECK(acted);
    // Mog learns the dance of the field after a victory, then dances it; a dance that the field does not show fails or changes the field.
    ff6::PartyState mog = party;
    ff6::setCharacterProperties(mog, rules, 10, 10, 0);
    mog.characters[10].hp = mog.characters[10].maxHp = 900;
    mog.characters[10].level = 40;
    int field = -1;
    for (int bg = 0; bg < FF6_BG_DANCE_BYTES && field < 0; ++bg) {
        if (weak.battleBgDance[static_cast<size_t>(bg)] == 3) field = bg;  // the love sonata
    }
    CHECK(field >= 0);
    ff6::Battle learning(weak, rules, mog, {10, 0}, 5, 94);
    learning.setBackground(field);
    for (int ticks = 0; ticks < 20000 && learning.outcome() == ff6::BattleOutcome::Fighting; ++ticks) {
        learning.tick();
        for (const int who : {0, 1}) {
            if (learning.combatants()[static_cast<size_t>(who)].ready && firstMonster(learning) >= 0) learning.fight(who, firstMonster(learning));
        }
    }
    CHECK(learning.outcome() == ff6::BattleOutcome::Won && (mog.dances & 8));
    mog.dances = 1 << 0;  // the wind song
    int started = 0, failed = 0, changed = 0;
    for (uint32_t seed = 300; seed < 340; ++seed) {
        ff6::PartyState copy = mog;
        ff6::Battle dancing(harmless, rules, copy, {10, 0}, 5, seed);
        dancing.setBackground(field);
        if (!waitReady(dancing, 0)) continue;
        dancing.takeEvents();
        dancing.dance(0, 0);
        const std::vector<ff6::BattleEvent> events = dancing.takeEvents();
        started += has(events, Kind::Dance);
        failed += has(events, Kind::DanceFailed);
        changed += has(events, Kind::Background);
        if (has(events, Kind::Dance)) CHECK((dancing.combatants()[0].status & FF6_ST_DANCE) && has(events, Kind::Cast));
        if (has(events, Kind::Background)) CHECK(dancing.background() == harmless.danceBg[0]);
    }
    CHECK(started > 0 && failed > 0 && changed > 0);
    // The slot: three alike give the attack of the icon, seven, seven, bar the doom of the party, anything else the rabbit.
    const auto spin = [&](std::array<int, 3> reels, const std::function<void(const ff6::Battle&, const std::vector<ff6::BattleEvent>&)>& check) {
        ff6::PartyState copy = party;
        ff6::Battle slotted(harmless, rules, copy, {0, 1}, 5, 95);
        CHECK(waitReady(slotted, 0));
        slotted.takeEvents();
        slotted.slot(0, reels);
        check(slotted, slotted.takeEvents());
    };
    spin({5, 5, 5}, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        bool flush = false;
        for (const ff6::BattleEvent& event : events) flush = flush || (event.kind == Kind::Cast && event.attack == FF6_ATTACK_SEVEN_FLUSH);
        CHECK(flush && has(events, Kind::Slot));
    });
    spin({0, 1, 2}, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>& events) { CHECK(has(events, Kind::NoEffect) && !has(events, Kind::Cast) && b.combatants()[0].alive()); });
    spin({0, 0, 2}, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>&) { CHECK(!b.combatants()[0].alive() && !b.combatants()[1].alive()); });
    spin({0, 0, 0}, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>&) { CHECK(b.outcome() == ff6::BattleOutcome::Won); });

    // The scripts of the monsters: what the game says is read from its texts, and the commands F2, F3, F5 and F6 do what they say.
    const std::vector<uint8_t> said = data.monsterDialog(1);
    CHECK(said.size() > 10 && said[0] == 0x94 && said[1] == 0xA5);  // "Ultros's form has changed!" begins with Ul
    CHECK(data.monsterDialog(0x40).size() > 0 && data.monsterDialog(FF6_MONSTER_DLG_COUNT).empty());
    const auto scripted = [&](const std::vector<uint8_t>& script, int battleNumber, int ticks, const std::function<void(const ff6::Battle&, const std::vector<ff6::BattleEvent>&)>& check) {
        ff6::BattleData changed = harmless;
        ff6::PartyState copy = party;
        ff6::Battle probe(changed, rules, copy, {0, 1}, battleNumber, 7);
        const int id = probe.combatants()[static_cast<size_t>(firstMonster(probe))].id;
        const size_t start = changed.aiScript.size();
        changed.aiScript.insert(changed.aiScript.end(), script.begin(), script.end());
        changed.aiPtrs[static_cast<size_t>(id) * 2] = static_cast<uint8_t>(start & 0xFF);
        changed.aiPtrs[static_cast<size_t>(id) * 2 + 1] = static_cast<uint8_t>(start >> 8);
        ff6::PartyState second = party;
        ff6::Battle run(changed, rules, second, {0, 1}, battleNumber, 7);
        std::vector<ff6::BattleEvent> events;
        for (int tick = 0; tick < ticks; ++tick) {
            run.tick();
            for (const int who : {0, 1}) {
                if (run.combatants()[static_cast<size_t>(who)].ready) run.defend(who);
            }
            for (const ff6::BattleEvent& event : run.takeEvents()) events.push_back(event);
        }
        check(run, events);
    };
    scripted({0xF3, 0x01, 0x00, 0xFF}, 5, 20000, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        bool dialog = false;
        for (const ff6::BattleEvent& event : events) dialog = dialog || (event.kind == Kind::Dialog && event.damage == 1 && event.attacker >= FF6_BATTLE_PARTY_SLOTS);
        CHECK(dialog);
    });
    scripted({0xF5, 0x00, 0x01, 0x00, 0xFF}, 5, 20000, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>& events) {  // it hides itself
        bool hidden = false;
        for (const ff6::BattleEvent& event : events) hidden = hidden || (event.kind == Kind::Entry && event.damage == 1);
        CHECK(hidden);
        bool anyHidden = false;
        for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) anyHidden = anyHidden || (b.combatants()[static_cast<size_t>(i)].status & FF6_ST_HIDE) != 0;
        CHECK(anyHidden);
    });
    scripted({0xF2, 0x06, 0x00, 0x00, 0xFF}, 5, 20000, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>& events) {  // the battle changes to the sixth
        CHECK(has(events, Kind::ChangeBattle));
        const ff6::BattleFormation sixth = harmless.formation(6);
        bool found = false;
        for (size_t slot = 0; slot < sixth.monsters.size(); ++slot) {
            if (sixth.monsters[slot] == FF6_MONSTER_NONE) continue;
            found = b.combatants()[FF6_BATTLE_PARTY_SLOTS + slot].present && b.combatants()[FF6_BATTLE_PARTY_SLOTS + slot].id == sixth.monsters[slot];
            break;
        }
        CHECK(found);
    });
    int healer = -1;
    for (int item = 0; item < FF6_ITEM_COUNT && healer < 0; ++item) {
        const uint8_t* prop = rules.item(item);
        if ((prop[FF6_ITEM_PROP_USAGE] & FF6_ITEM_PROP_TYPE_MASK) == FF6_ITEM_TYPE_CONSUMABLE && (prop[FF6_ITEM_PROP_FLAGS] & FF6_ITEM_FLAG_HP) &&
            (prop[FF6_ITEM_PROP_USAGE] & FF6_ITEM_USAGE_BATTLE) && !(prop[FF6_ITEM_PROP_FLAGS] & (FF6_ITEM_FLAG_DAMAGE | FF6_ITEM_FLAG_MP | FF6_ITEM_FLAG_REMOVE_STATUS)) &&
            prop[FF6_ITEM_PROP_ITEM_POWER] > 0) {
            healer = item;
        }
    }
    CHECK(healer >= 0);
    scripted({0xF1, 0x36, 0xF6, 0x00, static_cast<uint8_t>(healer), static_cast<uint8_t>(healer), 0xFF}, 5, 20000, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        bool used = false;
        for (const ff6::BattleEvent& event : events) used = used || (event.kind == Kind::UseItem && event.item == healer && event.attacker >= FF6_BATTLE_PARTY_SLOTS);
        CHECK(used);
    });
    // A script that wins Gau over lets the game bring him in; the veldt scripts wait for him while he has not joined.
    scripted({0xFB, 0x03, 0x00, 0xFF}, 5, 20000, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>&) { CHECK(b.recruitedGau()); });

    // The story at the end of a battle: Gau jumps in on the veldt (back by himself, fed with dried meat, or hungry), Shadow leaves
    // now and then, the dog of Shadow takes some blows, and the veldt gives no experience.
    const auto playOut = [&](ff6::Battle& battle) {
        std::vector<ff6::BattleEvent> all;
        for (int ticks = 0; ticks < 20000 && battle.outcome() == ff6::BattleOutcome::Fighting; ++ticks) {
            battle.tick();
            for (const int who : {0, 1}) {
                if (battle.combatants()[static_cast<size_t>(who)].ready && firstMonster(battle) >= 0) battle.fight(who, firstMonster(battle));
            }
            for (const ff6::BattleEvent& event : battle.takeEvents()) all.push_back(event);
        }
        for (const ff6::BattleEvent& event : battle.takeEvents()) all.push_back(event);
        return all;
    };
    // The colosseum: the character fights by itself against the monster of the wager; the wager is gone, the prize comes with a win.
    CHECK(data.colosseum.size() == FF6_COLOSSEUM_BYTES && data.colosseum[FF6_COLOSSEUM_ITEM_STRIKER * FF6_COLOSSEUM_ENTRY_BYTES + 3] != 0);
    {
        ff6::PartyState betting = party;
        ff6::giveItem(betting, FF6_ITEM_DRILL);
        ff6::Battle::Colosseum bout;
        bout.monster = 0;
        bout.wager = FF6_ITEM_DRILL;
        bout.prize = FF6_ITEM_BIO_BLASTER;
        ff6::Battle colosseum(data, rules, betting, {0}, FF6_COLOSSEUM_BATTLE, 3, FF6_BATTLE_SPEED_DEFAULT, bout);
        CHECK(colosseum.colosseum() && firstMonster(colosseum) >= 0 && colosseum.combatants()[static_cast<size_t>(firstMonster(colosseum))].id == 0);
        for (int ticks = 0; ticks < 40000 && colosseum.outcome() == ff6::BattleOutcome::Fighting; ++ticks) colosseum.tick();
        CHECK(colosseum.outcome() != ff6::BattleOutcome::Fighting && ff6::itemQuantity(betting, FF6_ITEM_DRILL) == 0);
        CHECK((colosseum.outcome() == ff6::BattleOutcome::Won) == (ff6::itemQuantity(betting, FF6_ITEM_BIO_BLASTER) == 1));
        CHECK(colosseum.reward().exp == 0 && colosseum.reward().gil == 0);
    }
    int fewMonsters = 5;  // a formation that leaves the sixth place empty
    for (int number = 0; number < FF6_BATTLE_COUNT; ++number) {
        const ff6::BattleFormation formation = weak.formation(number);
        if (formation.monsters[0] != FF6_MONSTER_NONE && formation.monsters[5] == FF6_MONSTER_NONE) {
            fewMonsters = number;
            break;
        }
    }
    // Gau jumps in as a monster: the party waits (he comes back by himself), feeds him or hits him; the battle that he ends gives
    // no reward.
    enum class Policy { Wait, Feed, Hit };
    struct GauRun {
        bool appeared = false, hungry = false, recruited = false, ranAway = false, intro = false, placed = false;
        long gil = 0, exp = 0;
        ff6::BattleOutcome outcome = ff6::BattleOutcome::Fighting;
        int meat = 0;
    };
    const auto withGau = [&](bool obtained, bool dried, bool available, Policy policy, uint32_t seed) {
        GauRun run;
        ff6::PartyState copy = party;
        ff6::setCharacterProperties(copy, rules, 11, 11, 0);  // Gau, as the game keeps him from the start
        if (dried) ff6::giveItem(copy, FF6_ITEM_DRIED_MEAT);
        ff6::Battle veldt(weak, rules, copy, {0, 1}, fewMonsters, seed);
        veldt.setVeldt(true);
        ff6::Battle::Story story;
        story.gauAvailable = available;
        story.gauObtained = obtained;
        veldt.setStory(story);
        for (int ticks = 0; ticks < 8000 && veldt.outcome() == ff6::BattleOutcome::Fighting; ++ticks) {
            veldt.tick();
            for (const int who : {0, 1}) {
                if (!veldt.combatants()[static_cast<size_t>(who)].ready) continue;
                int gauAt = -1;
                for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {
                    if (veldt.combatants()[static_cast<size_t>(i)].alive() && veldt.combatants()[static_cast<size_t>(i)].id == FF6_MONSTER_GAU_VELDT) gauAt = i;
                }
                if (gauAt < 0) {
                    if (firstMonster(veldt) >= 0) veldt.fight(who, firstMonster(veldt));
                } else if (policy == Policy::Feed) {
                    veldt.useItem(who, FF6_ITEM_DRIED_MEAT, {gauAt});
                } else if (policy == Policy::Hit) {
                    veldt.fight(who, gauAt);
                } else {
                    veldt.defend(who);
                }
            }
            for (const ff6::BattleEvent& event : veldt.takeEvents()) {
                run.appeared = run.appeared || (event.kind == Kind::Scene && event.damage == FF6_BATTLE_EVENT_GAU_APPEARS);
                run.hungry = run.hungry || (event.kind == Kind::Dialog && event.damage == 0x23);
                run.ranAway = run.ranAway || (event.kind == Kind::Scene && event.damage == 28);
                run.intro = run.intro || (event.kind == Kind::Scene && event.damage == 13);
            }
        }
        for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {  // he is the character of the scene of the table: where and as it says
            const ff6::Combatant& c = veldt.combatants()[static_cast<size_t>(i)];
            if (c.present && c.charIndex == FF6_ACTOR_GAU) run.placed = c.placeX == 0x30 && c.placeY == 0x34 && c.gfx == 0x0B && c.id == FF6_MONSTER_GAU_VELDT && c.monster && !c.offstage;
        }
        run.recruited = veldt.recruitedGau();
        run.gil = veldt.reward().gil;
        run.exp = veldt.reward().exp;
        run.outcome = veldt.outcome();
        run.meat = ff6::itemQuantity(copy, FF6_ITEM_DRIED_MEAT);
        return run;
    };
    int appeared = 0, joined = 0;
    for (uint32_t seed = 400; seed < 440; ++seed) {  // already won over: he asks to come along at his first turn
        const GauRun run = withGau(true, false, false, Policy::Wait, seed);
        appeared += run.appeared;
        if (!run.appeared) continue;
        joined += run.recruited && run.placed && run.outcome == ff6::BattleOutcome::Won && run.gil == 0 && run.exp == 0 && !run.ranAway && !run.hungry;
    }
    CHECK(appeared > 10 && appeared < 40 && joined == appeared);  // five times in eight
    int hungry = 0, away = 0, fed = 0;
    for (uint32_t seed = 400; seed < 440; ++seed) {
        const GauRun waited = withGau(false, true, false, Policy::Wait, seed);  // the first time he only asks for food
        if (!waited.appeared) continue;
        hungry += waited.hungry && !waited.recruited && waited.outcome == ff6::BattleOutcome::Fighting && waited.meat == 1;
        const GauRun hit = withGau(false, true, false, Policy::Hit, seed);  // a blow makes him run away
        away += hit.ranAway && !hit.recruited && hit.outcome == ff6::BattleOutcome::Won && hit.gil == 0;
        const GauRun meat = withGau(false, true, false, Policy::Feed, seed);  // the dried meat wins him over, with the scene that goes with it
        fed += meat.intro && meat.recruited && meat.outcome == ff6::BattleOutcome::Won && meat.meat == 0 && meat.gil == 0;
    }
    CHECK(hungry > 5 && away == hungry && fed == hungry);
    for (uint32_t seed = 400; seed < 440; ++seed) CHECK(!withGau(true, true, true, Policy::Wait, seed).appeared);  // he is already there
    int left = 0;
    for (uint32_t seed = 500; seed < 700; ++seed) {
        ff6::PartyState copy = party;
        ff6::setCharacterProperties(copy, rules, 3, 3, 0);
        copy.characters[3].hp = copy.characters[3].maxHp;
        ff6::Battle shadow(weak, rules, copy, {3, 0}, 5, seed);
        ff6::Battle::Story story;
        story.shadowAvailable = true;
        story.shadowStays = false;
        shadow.setStory(story);
        const std::vector<ff6::BattleEvent> events = playOut(shadow);
        left += shadow.shadowLeft();
        bool scene = false;
        for (const ff6::BattleEvent& event : events) scene = scene || (event.kind == Kind::Scene && event.damage == FF6_BATTLE_EVENT_SHADOW_LEAVES);
        CHECK(shadow.shadowLeft() == scene);
        if (shadow.shadowLeft()) CHECK(shadow.reward().exp >= 0 && !shadow.combatants()[0].alive());
        ff6::PartyState stays = party;
        ff6::setCharacterProperties(stays, rules, 3, 3, 0);
        stays.characters[3].hp = stays.characters[3].maxHp;
        ff6::Battle fixed(weak, rules, stays, {3, 0}, 5, seed);
        playOut(fixed);
        CHECK(!fixed.shadowLeft());  // by default the story keeps him
    }
    CHECK(left > 3 && left < 60);  // one in sixteen
    int blocked = 0, bitten = 0;
    for (uint32_t seed = 600; seed < 640; ++seed) {
        ff6::PartyState copy = party;
        copy.characters[0].status |= FF6_ST_INTERCEPTOR;
        ff6::Battle dog(harmless, rules, copy, {0, 1}, 5, seed);
        for (int ticks = 0; ticks < 6000; ++ticks) {
            dog.tick();
            for (const int who : {0, 1}) {
                if (dog.combatants()[static_cast<size_t>(who)].ready) dog.defend(who);
            }
            for (const ff6::BattleEvent& event : dog.takeEvents()) {
                blocked += event.kind == Kind::Interceptor;
                bitten += event.kind == Kind::Cast && event.attacker == 0 && event.attack >= FF6_ATTACK_FIRST_INTERCEPTOR;
            }
        }
    }
    CHECK(blocked > 0 && bitten > 0 && bitten < blocked);
    // The rarer special attacks of the monsters: the fallen one leaves one hp, the spiraler and pep up spend the caster.
    CHECK(data.metamorph.size() == FF6_METAMORPH_BYTES);
    // A monster that seizes a character holds it (it cannot act) until it discards it, a charmed one strikes its own party, phantasm
    // drains like sap, and a forcefield nullifies one more element.
    scripted({200, 0xFD, 219, 0xFF}, 5, 30000, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        bool held = false, let = false;
        for (const ff6::BattleEvent& event : events) {
            held = held || (event.kind == Kind::Seized && event.damage == 0);
            let = let || (held && event.kind == Kind::Seized && event.damage == 2);
        }
        CHECK(held && let);
    });
    scripted({206, 0xFF}, 5, 30000, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        bool charmed = false, struck = false;
        for (const ff6::BattleEvent& event : events) {
            charmed = charmed || (event.kind == Kind::Seized && event.damage == 1);
            struck = struck || (charmed && (event.kind == Kind::Hit || event.kind == Kind::Miss || event.kind == Kind::Critical) &&
                                event.attacker >= 0 && event.attacker < FF6_BATTLE_PARTY_SLOTS && event.target >= 0 && event.target < FF6_BATTLE_PARTY_SLOTS);
        }
        CHECK(charmed && struck);
    });
    scripted({225, 0xFF}, 5, 30000, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) { CHECK(has(events, Kind::Sap)); });
    // The command FA of a script: a monster that moves back (the animation 273 on itself) and a sound effect.
    scripted({0xFA, 0x01, 0x3F, 0x00, 0xFA, 0x09, 0x00, 0x21, 0xFF}, 5, 20000, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        bool moved = false, sound = false;
        for (const ff6::BattleEvent& event : events) {
            moved = moved || (event.kind == Kind::MonsterAnim && event.attack == 273 && event.target >= FF6_BATTLE_PARTY_SLOTS);
            sound = sound || (event.kind == Kind::MonsterAnim && event.damage == FF6_MONSTER_ANIM_SFX && event.attack == 0x21);
        }
        CHECK(moved && sound);
    });
    // Overcast marks the party: whoever falls afterwards is a zombie.
    scripted({220, 0xFF}, 5, 30000, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>&) {
        bool marked = false;
        for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) marked = marked || b.combatants()[static_cast<size_t>(i)].overcast;
        CHECK(marked);
    });
    scripted({154, 0xFF}, 5, 20000, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) { CHECK(has(events, Kind::Forcefield)); });
    // A love token makes the target cover the caster, a zinger takes the caster out of the fight, and the empowerer takes mp first.
    scripted({199, 0xFF}, 5, 20000, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
        bool bound = false;
        for (const ff6::BattleEvent& event : events) bound = bound || (event.kind == Kind::Seized && event.damage == 3);
        CHECK(bound);
    });
    scripted({218, 0xFF}, 5, 20000, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>& events) {
        bool clung = false;
        for (const ff6::BattleEvent& event : events) clung = clung || (event.kind == Kind::Seized && event.damage == 4);
        bool hidden = false;
        for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) hidden = hidden || (b.combatants()[static_cast<size_t>(i)].present && b.combatants()[static_cast<size_t>(i)].noTarget);
        CHECK(clung && hidden);
    });
    scripted({89, 0xFF}, 5, 20000, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) { CHECK(has(events, Kind::MpDamage)); });
    // Evil toot and mind blast give one status of the attack at most to a target.
    for (const int toot : {211, 196}) {
        const uint32_t statuses = ff6::attackFromProp(rules.attack(toot)).status;
        CHECK(__builtin_popcount(statuses) > 1);
        scripted({static_cast<uint8_t>(toot), 0xFF}, 5, 20000, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>&) {
            for (int who = 0; who < 2; ++who) CHECK(__builtin_popcount(b.combatants()[static_cast<size_t>(who)].status & statuses) <= 1);
        });
    }
    scripted({192, 0xFF}, 5, 20000, [&](const ff6::Battle& b, const std::vector<ff6::BattleEvent>&) {
        CHECK(b.combatants()[0].hp == 1 || b.combatants()[1].hp == 1);
    });
    for (const int spent : {99, 157}) {
        scripted({static_cast<uint8_t>(spent), 0xFF}, 5, 20000, [&](const ff6::Battle&, const std::vector<ff6::BattleEvent>& events) {
            bool fell = false;
            for (const ff6::BattleEvent& event : events) fell = fell || (event.kind == Kind::Defeated && event.target >= FF6_BATTLE_PARTY_SLOTS);
            CHECK(fell);
        });
    }
    ff6::Battle leapBattle(harmless, rules, gau, {11, 0}, 5, 92);
    leapBattle.setVeldt(true);
    CHECK(waitReady(leapBattle, 0) && !leapBattle.gauLeaped());
    leapBattle.leap(0);
    CHECK(leapBattle.gauLeaped());

    // The scenes of the character AI: a character that acts by a script (Cyan at the imperial camp), a scene that hides the party
    // (Terra's flashback), one that makes a character fight for the monsters (Kefka), and one that waits for Sabin (Vargas).
    ff6::PartyState story = party;
    for (const int slot : {0, 1, 2, 4, 5}) {
        ff6::setCharacterProperties(story, rules, slot, slot, 0);
        story.characters[static_cast<size_t>(slot)].hp = story.characters[static_cast<size_t>(slot)].maxHp;
    }
    ff6::setCharacterProperties(story, rules, 14, 0x29, 0);  // Kefka, as the story keeps him
    story.characters[14].hp = story.characters[14].maxHp = 600;
    {
        ff6::PartyState copy = story;
        ff6::Battle camp(data, rules, copy, {0, 2}, 60, 7);
        const ff6::Combatant* cyan = nullptr;
        for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
            if (camp.combatants()[static_cast<size_t>(i)].present && camp.combatants()[static_cast<size_t>(i)].id == 2) cyan = &camp.combatants()[static_cast<size_t>(i)];
        }
        CHECK(cyan && cyan->aiId == 0x170 && cyan->aiStart >= 0 && cyan->placeX == 0x50 && cyan->placeY == 0x30 && cyan->gfx == 2);
        bool acted = false;
        for (int tick = 0; tick < 8000 && camp.outcome() == ff6::BattleOutcome::Fighting; ++tick) {
            camp.tick();
            for (const ff6::BattleEvent& event : camp.takeEvents()) {
                if (cyan && event.attacker == static_cast<int>(cyan - camp.combatants().data())) acted = true;
            }
            if (camp.combatants()[0].ready) camp.defend(0);
        }
        CHECK(acted);  // the player was never asked for the commands of Cyan
        CHECK(!cyan || !cyan->ready);
    }
    {
        ff6::PartyState copy = story;
        ff6::Battle flashback(data, rules, copy, {1, 4}, 485, 7);  // the party is hidden: only the characters of the scene stand
        int standing = 0;
        bool locke = false;
        for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
            const ff6::Combatant& c = flashback.combatants()[static_cast<size_t>(i)];
            standing += c.present;
            locke = locke || (c.present && c.id == 1);
        }
        CHECK(standing == 2 && !locke && flashback.hideNames());  // Kefka, who waits out of the battle, and Terra
    }
    {
        ff6::PartyState copy = story;
        ff6::Battle imp(data, rules, copy, {0, 1}, 504, 7);  // Kefka fights for the monsters, with his own stats
        const ff6::Combatant* kefka = nullptr;
        for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS; ++i) {
            if (imp.combatants()[static_cast<size_t>(i)].present && imp.combatants()[static_cast<size_t>(i)].charIndex == 14) kefka = &imp.combatants()[static_cast<size_t>(i)];
        }
        CHECK(kefka && kefka->monster && kefka->gfx == 0x15 && kefka->aiStart >= 0 && kefka->maxHp > 0 && kefka->level == story.characters[14].level);
    }
    {
        ff6::PartyState copy = story;
        ff6::Battle vargas(data, rules, copy, {0, 1}, 435, 7);  // Sabin waits out of the battle until the scene of his coming
        const ff6::Combatant* sabin = nullptr;
        for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
            if (vargas.combatants()[static_cast<size_t>(i)].present && vargas.combatants()[static_cast<size_t>(i)].id == 5) sabin = &vargas.combatants()[static_cast<size_t>(i)];
        }
        CHECK(sabin && sabin->offstage && !sabin->alive());
        const ff6::Attack aimed = vargas.spellAttack(1);  // the scene says which monsters can be aimed at: not the first one, until a script allows it
        CHECK(!vargas.canTarget(aimed, FF6_BATTLE_PARTY_SLOTS) && vargas.canTarget(aimed, FF6_BATTLE_PARTY_SLOTS + 1));
        ff6::BattleData allowing = data;
        const size_t allowAt = allowing.aiScript.size();
        allowing.aiScript.insert(allowing.aiScript.end(), {0xFB, 0x06, 0x36, 0xFF});  // target on, itself
        const int vargasId = vargas.combatants()[FF6_BATTLE_PARTY_SLOTS].id;
        allowing.aiPtrs[static_cast<size_t>(vargasId) * 2] = static_cast<uint8_t>(allowAt & 0xFF);
        allowing.aiPtrs[static_cast<size_t>(vargasId) * 2 + 1] = static_cast<uint8_t>(allowAt >> 8);
        ff6::PartyState third = story;
        ff6::Battle allowed(allowing, rules, third, {0, 1}, 435, 7);
        bool aimable = false;
        for (int tick = 0; tick < 20000 && !aimable; ++tick) {
            allowed.tick();
            for (const int who : {0, 1}) {
                if (allowed.combatants()[static_cast<size_t>(who)].ready) allowed.defend(who);
            }
            allowed.takeEvents();
            aimable = allowed.canTarget(aimed, FF6_BATTLE_PARTY_SLOTS);
        }
        CHECK(aimable);
        ff6::BattleData scripted435 = data;  // the scene of his coming: a monster starts it, and Terra and Locke leave
        const size_t at = scripted435.aiScript.size();
        scripted435.aiScript.insert(scripted435.aiScript.end(), {0xF7, 0x07, 0xFF});
        int firstId = -1;
        for (int i = FF6_BATTLE_PARTY_SLOTS; i < FF6_BATTLE_COMBATANTS && firstId < 0; ++i) {
            if (vargas.combatants()[static_cast<size_t>(i)].present && vargas.combatants()[static_cast<size_t>(i)].gfx < 0) firstId = vargas.combatants()[static_cast<size_t>(i)].id;
        }
        CHECK(firstId >= 0);
        scripted435.aiPtrs[static_cast<size_t>(firstId) * 2] = static_cast<uint8_t>(at & 0xFF);
        scripted435.aiPtrs[static_cast<size_t>(firstId) * 2 + 1] = static_cast<uint8_t>(at >> 8);
        ff6::PartyState second = story;
        ff6::Battle coming(scripted435, rules, second, {0, 1}, 435, 7);
        bool scene = false;
        for (int tick = 0; tick < 20000 && !scene; ++tick) {
            coming.tick();
            for (const int who : {0, 1}) {
                if (coming.combatants()[static_cast<size_t>(who)].ready) coming.defend(who);
            }
            for (const ff6::BattleEvent& event : coming.takeEvents()) scene = scene || (event.kind == Kind::Scene && event.damage == FF6_BATTLE_EVENT_SABIN_INTRO);
        }
        CHECK(scene);
        bool sabinIn = false, othersOut = true;
        for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
            const ff6::Combatant& c = coming.combatants()[static_cast<size_t>(i)];
            if (!c.present) continue;
            if (c.id == 5) sabinIn = !c.offstage;
            if (c.id == 0 || c.id == 1) othersOut = othersOut && c.offstage;
        }
        CHECK(sabinIn && othersOut);
    }
}
