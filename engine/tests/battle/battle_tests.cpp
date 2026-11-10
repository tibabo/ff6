#include <cstdlib>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "battle/battle.hpp"
#include "battle/battle_screen.hpp"
#include "battle/encounters.hpp"
#include "state/char_stats.hpp"
#include "state/party_ops.hpp"

namespace {

void testFormulas() {
    CHECK(ff6::invertEvade(0) == 0xFF && ff6::invertEvade(20) == 216 && ff6::invertEvade(128) == 1);
    CHECK(ff6::atbConstant(30, false, 3) == 300);  // (30 + 20) * (64 * 1.5) / 16
    CHECK(ff6::atbConstant(30, false, 3, false, true) == 393);
    CHECK(ff6::atbConstant(30, false, 3, true) == 150);
    CHECK(ff6::atbConstant(30, true, 2) == 240);  // the monsters are slowed by the battle speed
    CHECK(ff6::atbConstant(30, true, 0) > ff6::atbConstant(30, true, 5));
    CHECK(ff6::physicalDamage(16, 56, 5, true, 0, 255) == 11);  // a guard at level 5
    CHECK(ff6::physicalDamage(20, 40, 10, false, 0, 255) == 54);
    CHECK(ff6::physicalDamage(20, 40, 10, false, 100, 255) == 33);  // the defense takes 100 / 255
    CHECK(ff6::physicalDamage(255, 255, 99, false, 0, 255) == FF6_MAX_DAMAGE);
    CHECK(ff6::physicalDamage(20, 40, 10, false, 255, 255) == 1);
    CHECK(ff6::physicalDamage(20, 40, 10, false, 0, 255, true) == 63);  // the two-hand effect: 7/4 of the power
    CHECK(ff6::physicalDamage(20, 40, 10, false, 0, 255, false, true) == 108);  // a critical hit doubles it
    ff6::BattleRandom random(7);
    for (int i = 0; i < 100; ++i) CHECK(random.below(5) >= 0 && random.below(5) < 5);
    bool seen[FF6_BATTLE_TYPE_COUNT] = {};
    for (int i = 0; i < 4000; ++i) seen[ff6::chooseBattleType(0, 4, random)] = true;
    CHECK(seen[0] && seen[1] && seen[2] && seen[3]);
    for (int i = 0; i < 200; ++i) {
        CHECK(ff6::chooseBattleType(0xE0, 4, random) == FF6_BATTLE_TYPE_NORMAL);  // the other types are forbidden
        CHECK(ff6::chooseBattleType(0x10, 4, random) != FF6_BATTLE_TYPE_NORMAL);
        CHECK(ff6::chooseBattleType(0, 2, random) != FF6_BATTLE_TYPE_SIDE);  // too few allies for a side attack
    }
}

}  // namespace

void testBattle() {
    testFormulas();
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::GameRules rules = ff6::loadGameRules(pack);
    const ff6::BattleData data = ff6::loadBattleData(pack);

    ff6::PartyState party;
    ff6::setCharacterProperties(party, rules, 0, 0, 0);  // Terra
    party.characters[0].level = 20;
    party.characters[0].hp = party.characters[0].maxHp = 900;
    party.characters[0].vigor = 40;

    {  // The statuses that last come into the battle with the character
        ff6::PartyState sick = party;
        sick.characters[0].status = FF6_ST_POISON | FF6_ST_BLIND;
        const ff6::Battle ill(data, rules, sick, {0}, 0, 1);
        CHECK((ill.combatants()[0].status & (FF6_ST_POISON | FF6_ST_BLIND)) == (FF6_ST_POISON | FF6_ST_BLIND));
    }

    // The first battle: one monster, which the party defeats.
    ff6::Battle battle(data, rules, party, {0}, 0, 1);
    CHECK(battle.combatants()[0].alive() && battle.combatants()[FF6_BATTLE_PARTY_SLOTS].alive());
    CHECK(!battle.combatants()[FF6_BATTLE_PARTY_SLOTS + 1].present);
    CHECK(battle.combatants()[FF6_BATTLE_PARTY_SLOTS].id == 0x19);
    CHECK(battle.combatants()[0].atbConstant == ff6::atbConstant(battle.combatants()[0].speed, false, FF6_BATTLE_SPEED_DEFAULT));
    CHECK(battle.battleType() == FF6_BATTLE_TYPE_NORMAL);  // the other types are forbidden for this battle
    const ff6::CharacterStats stats = ff6::calcStats(rules, party.characters[0]);
    CHECK(stats.handPower[0] + stats.handPower[1] == stats.attackPower);
    CHECK(stats.handHitRate[0] == rules.item(party.characters[0].equipment[0] & 0xFF)[FF6_ITEM_PROP_HIT_OR_MAGIC_DEFENSE]);
    CHECK(stats.handHitRate[1] > 0);  // the fist hits too
    for (const ff6::Combatant& c : battle.combatants()) CHECK(c.gauge >= 0 && c.gauge < FF6_ATB_FULL);
    bool readySeen = false;
    for (int ticks = 0; ticks < 20000 && battle.outcome() == ff6::BattleOutcome::Fighting; ++ticks) {
        battle.tick();
        const int ready = battle.readyCharacter();
        if (ready >= 0) {
            readySeen = true;
            battle.fight(ready, FF6_BATTLE_PARTY_SLOTS);
        }
    }
    CHECK(readySeen);
    CHECK(battle.outcome() == ff6::BattleOutcome::Won);
    const uint8_t* monster = data.monster(0x19);
    CHECK(battle.reward().exp == (monster[FF6_MONSTER_PROP_EXP] | (monster[FF6_MONSTER_PROP_EXP + 1] << 8)));
    CHECK(party.gil == battle.reward().gil);
    CHECK(party.characters[0].experience > 0 || battle.reward().exp == 0);
    CHECK(!battle.takeEvents().empty() && battle.takeEvents().empty());

    // The party that is all at 0 hp has lost before it starts.
    ff6::PartyState dead = party;
    dead.characters[0].hp = 0;
    CHECK(ff6::Battle(data, rules, dead, {0}, 0, 1).outcome() == ff6::BattleOutcome::Lost);

    // The guard of the first battle holds the party back: nothing makes it run.
    ff6::Battle held(data, rules, party, {0}, 0, 1);
    CHECK(!held.canRun());
    held.setRunning(true);
    for (int ticks = 0; ticks < 2000; ++ticks) held.tick();
    CHECK(!held.combatants()[0].escaped);

    // A weak character that runs leaves or dies, and the hp of the party follow the battle.
    ff6::PartyState weak = party;
    weak.characters[0].hp = 1;
    ff6::Battle losing(data, rules, weak, {0}, 6, 3);  // a battle that can be left
    losing.setRunning(true);
    for (int ticks = 0; ticks < 20000 && losing.outcome() == ff6::BattleOutcome::Fighting; ++ticks) losing.tick();
    CHECK(losing.outcome() != ff6::BattleOutcome::Fighting);
    CHECK(losing.outcome() == ff6::BattleOutcome::Fled || weak.characters[0].hp == 0);

    // A character that runs leaves once its gauge is full, at its next turn of the counter, so never at the start.
    for (uint32_t seed = 1; seed <= 20; ++seed) {
        ff6::PartyState copy = party;
        ff6::Battle runner(data, rules, copy, {0}, 6, seed);
        runner.setRunning(true);
        int ticks = 0;
        for (; ticks < 20000 && runner.outcome() == ff6::BattleOutcome::Fighting; ++ticks) runner.tick();
        CHECK(runner.outcome() == ff6::BattleOutcome::Fled);
        CHECK(ticks > 1 && runner.combatants()[0].escaped);
        bool escaped = false;
        for (const ff6::BattleEvent& event : runner.takeEvents()) escaped = escaped || event.kind == ff6::BattleEvent::Kind::Escaped;
        CHECK(escaped);
    }

    // Without running, the character waits for its command.
    ff6::PartyState waiting = party;
    ff6::Battle idle(data, rules, waiting, {0}, 0, 5);
    for (int ticks = 0; ticks < 400 && idle.readyCharacter() < 0; ++ticks) idle.tick();
    CHECK(idle.readyCharacter() == 0 && idle.outcome() == ff6::BattleOutcome::Fighting);

    // The screen: the player fights until the end, pressing the button at every frame.
    const ff6::MenuData menu = ff6::loadMenuData(pack);
    const ff6::CommonData common = ff6::loadCommonData(pack);
    ff6::MenuScreen screen(common, menu);
    test::RecordingSound sound;
    ff6::BattleScreen view(screen, sound);
    const ff6::BattleGfx gfx = ff6::loadBattleGfx(pack);
    ff6::InputState press;
    press.confirm = true;
    ff6::PartyState played = party;
    view.start(data, rules, played, {0}, 0, 1, FF6_BATTLE_SPEED_DEFAULT, true);
    CHECK(view.active() && view.mode() == ff6::BattleMode::Fighting);
    view.setGfx(&gfx);
    view.draw();  // the monster of the first battle is painted where the formation puts it
    const ff6::BattleImage& picture = gfx.monster(0x19);
    size_t painted = 0;
    for (int y = 72; y < 72 + picture.height && y < FF6_BATTLE_MONSTERS_Y * FF6_MENU_TILE_SIZE; ++y) {
        for (int x = 48; x < 48 + picture.width; ++x) {
            const uint8_t* pixel = &screen.pixels()[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL];
            painted += pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0;
        }
    }
    CHECK(painted > 100);
    size_t standing = 0;  // Terra, first of the party
    for (int y = 68; y < 68 + FF6_CHAR_HEIGHT; ++y) {
        for (int x = 195; x < 195 + FF6_CHAR_WIDTH; ++x) {
            const uint8_t* pixel = &screen.pixels()[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL];
            standing += pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0;
        }
    }
    CHECK(standing > 20);
    view.start(data, rules, played, {0}, 0, 1, FF6_BATTLE_SPEED_DEFAULT, true, 0);  // with the field of the first background
    view.draw();
    size_t lit = 0;
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
            const uint8_t* pixel = &screen.pixels()[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL];
            lit += pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0;
        }
    }
    CHECK(lit > 1000);
    view.start(data, rules, played, {0}, 0, 1, FF6_BATTLE_SPEED_DEFAULT, true);
    bool sawCommand = false, sawTarget = false, sawPopup = false, drew = false;
    for (int frame = 0; frame < 60000 && !view.finished(); ++frame) {
        sawCommand = sawCommand || view.mode() == ff6::BattleMode::Command;
        sawTarget = sawTarget || view.mode() == ff6::BattleMode::Target;
        sawPopup = sawPopup || (view.popupTarget() >= 0 && view.popupDamage() > 0);
        if (frame % 50 == 0 || view.mode() == ff6::BattleMode::Command) {
            view.draw();
            for (const uint8_t byte : screen.pixels()) drew = drew || byte != 0;
        }
        view.update(press);
    }
    CHECK(view.finished() && view.outcome() == ff6::BattleOutcome::Won);
    CHECK(sawCommand && sawTarget && sawPopup && drew);
    CHECK(played.gil == party.gil + view.battle()->reward().gil && played.characters[0].experience >= party.characters[0].experience);
    bool selected = false;
    for (const auto& command : sound.commands) selected = selected || command.command == FF6_SFX_MENU_SELECT;
    CHECK(selected);
    view.close();
    CHECK(!view.active());

    // Run is in the window that right brings: the party leaves once the gauge is full, and the buttons let go stop it.
    ff6::PartyState escaping = party;
    view.start(data, rules, escaping, {0}, 6, 4, FF6_BATTLE_SPEED_DEFAULT, true);
    ff6::InputState right;
    right.direction = ff6::Right;
    for (int frame = 0; frame < 60000 && view.mode() != ff6::BattleMode::Command; ++frame) {
        view.update(frame % 3 == 0 ? press : ff6::InputState());  // the messages go by
    }
    CHECK(view.mode() == ff6::BattleMode::Command && view.command() == 0);
    view.update(right);
    view.update(ff6::InputState());
    view.update(press);
    for (int frame = 0; frame < 60000 && !view.finished(); ++frame) view.update(frame % 3 == 0 ? press : ff6::InputState());
    CHECK(view.finished() && view.outcome() == ff6::BattleOutcome::Fled);

    // Setzer's Slot: the reels turn on the screen and the button stops them one after the other.
    ff6::PartyState gambler = party;
    ff6::setCharacterProperties(gambler, rules, 9, 9, 0);
    gambler.characters[9].hp = gambler.characters[9].maxHp = 900;
    gambler.characters[9].mp = gambler.characters[9].maxMp = 99;
    view.start(data, rules, gambler, {9}, 5, 3, FF6_BATTLE_SPEED_DEFAULT, true);
    ff6::InputState down;
    down.direction = ff6::Down;
    for (int frame = 0; frame < 60000 && view.mode() != ff6::BattleMode::Command; ++frame) view.update(frame % 3 == 0 ? press : ff6::InputState());
    CHECK(view.mode() == ff6::BattleMode::Command);
    view.update(down);
    view.update(ff6::InputState());
    view.update(press);
    CHECK(view.mode() == ff6::BattleMode::Slot);
    view.draw();
    for (int stop = 0; stop < 3; ++stop) {
        for (int frame = 0; frame < 7; ++frame) view.update(ff6::InputState());
        view.draw();
        view.update(press);
    }
    CHECK(view.mode() != ff6::BattleMode::Slot);
    view.close();

    // The events that start the random battles and the monsters in boxes are where the game keeps them.
    CHECK(common.eventScript[FF6_EVENT_SCRIPT_RAND_BATTLE] == FF6_EVENT_RANDOM_BATTLE);
    bool boxBattle = false;
    for (size_t i = 0; i < 16; ++i) boxBattle = boxBattle || common.eventScript[FF6_EVENT_SCRIPT_TREASURE_MONSTER + i] == FF6_EVENT_BOX_BATTLE;
    CHECK(boxBattle);

    // The random battles of a map: a battle comes after some steps, the more often the higher the rate of the map.
    CHECK(data.subRate(41) == 1 && data.subRate(-1) == 0 && data.subGroup(FF6_BATTLE_MAP_COUNT) == 0);
    CHECK(ff6::Encounters().step(data, 41, false) == -1 && ff6::Encounters().step(data, 1, true) == -1);
    int lowMap = -1, highMap = -1;
    for (int map = FF6_FIRST_SUB_MAP; map < FF6_BATTLE_MAP_COUNT; ++map) {
        if (data.subRate(map) == 1 && lowMap < 0) lowMap = map;
        if (data.subRate(map) == 2 && highMap < 0) highMap = map;
    }
    CHECK(lowMap >= 0 && highMap >= 0);
    const auto battles = [&](int map) {
        ff6::Encounters walk;
        int count = 0;
        for (int step = 0; step < 5000; ++step) {
            const int battle = walk.step(data, map, true);
            if (battle < 0) continue;
            ++count;
            CHECK((battle & ~FF6_BATTLE_RANDOM_FLAG) + (battle & FF6_BATTLE_RANDOM_FLAG ? FF6_BATTLE_RANDOM_SPAN : 1) <= FF6_BATTLE_COUNT);
        }
        return count;
    };
    CHECK(battles(lowMap) > 5 && battles(highMap) > battles(lowMap));
}
