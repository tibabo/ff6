#include <cstdio>
#include <stdexcept>
#include <string>

#include "../check.hpp"
#include "../tests.hpp"
#include "data/json.hpp"
#include "data/save.hpp"

namespace {

template <typename Function>
bool throws(Function function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

ff6::SaveState sampleState() {
    ff6::SaveState state;
    state.map = 0x1C;
    state.x = 8;
    state.y = 45;
    state.facing = 0;
    state.activeParty = 2;
    state.characters[0].enabled = true;
    state.characters[0].visible = true;
    state.characters[0].party = 2;
    state.characters[0].slot = 1;
    state.characters[0].map = 0x1C;
    state.characters[0].gfx = 164;
    state.characters[0].palette = 7;
    state.characters[0].x = 8;
    state.characters[0].y = 45;
    state.characters[0].facing = 3;
    state.eventBits = {0, 17, 1023};
    state.battleFlags = {5};
    state.spritePalettes = {9, 8, 7, 6, 5, 4, 3, 2};
    state.hasParent = true;
    state.parentMap = 0x1E;
    state.parentX = 3;
    state.parentY = 4;
    state.parentFacing = 1;
    state.config = {0x5C, 0x41, 0x80};
    state.hasColors = true;
    state.fontColor = 0x1234;
    state.windowColors[5] = 0x7FFF;
    state.hasParty = true;
    ff6::CharacterState& terra = state.party.characters[0];
    terra.actor = 0;
    terra.level = 12;
    terra.hp = 150;
    terra.maxHp = 210;
    terra.mp = 30;
    terra.maxMp = 44;
    terra.experience = 1234;
    terra.status = FF6_ST_POISON | FF6_ST_BLIND;
    terra.vigor = 31;
    terra.speed = 33;
    terra.stamina = 28;
    terra.magicPower = 39;
    terra.equipment = {1, 90, 105, 132, FF6_ITEM_EMPTY, 7};
    state.party.inventory[0] = {233, 5};
    state.party.inventory[1] = {0, 99};
    state.party.gil = 1234567;
    state.party.steps = 4321;
    state.party.treasures[11] = true;
    state.party.treasures[511] = true;
    state.party.veldtBattles[17] = true;
    state.party.veldtBattles[511] = true;
    state.party.veldtPointer = 63;
    state.doomGazeCounter = 99;
    state.airshipX = 137;
    state.airshipY = 199;
    state.party.spells[0][45] = true;
    state.party.spellLearn[0][3] = 70;
    state.party.rages[0x19] = true;
    state.party.rages[255] = true;
    state.party.dances = 0x81;
    state.party.morphCounter = 123;
    state.party.spells[6][1] = true;
    state.party.espers[3] = true;
    state.party.swordtechs = 0x0F;
    state.party.blitzes = 0x81;
    return state;
}

}  // namespace

void testSave() {
    // The parser reads what a save is made of.
    const ff6::Json value = ff6::parseJson(" {\"a\": [1, -2, true, null], \"b\": \"x\\\"y\\n\", \"c\": {}} ");
    CHECK(value.type == ff6::Json::Type::Object && value.members.size() == 3);
    const ff6::Json* a = value.find("a");
    CHECK(a && a->items.size() == 4 && a->items[1].number == -2 && a->items[2].boolean);
    CHECK(a->items[3].type == ff6::Json::Type::Null);
    const ff6::Json* b = value.find("b");
    CHECK(b && b->text == "x\"y\n");
    CHECK(value.find("missing") == nullptr);
    CHECK(ff6::jsonQuote("a\"b\\c\n") == "\"a\\\"b\\\\c\\n\"");

    // And refuses what is not valid, with an error instead of a crash.
    for (const char* bad : {"", "{", "[1,]", "{\"a\" 1}", "[1] 2", "\"abc", "tru", "1.5", "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]"}) {
        CHECK(throws([&] { ff6::parseJson(bad); }));
    }

    // A save comes back the same from its text.
    const ff6::SaveState state = sampleState();
    const ff6::SaveState back = ff6::saveFromJson(ff6::saveToJson(state));
    CHECK(back.map == state.map && back.x == state.x && back.y == state.y && back.facing == state.facing);
    CHECK(back.activeParty == 2 && back.eventBits == state.eventBits && back.battleFlags == state.battleFlags);
    CHECK(back.spritePalettes == state.spritePalettes);
    CHECK(back.hasParent && back.parentMap == 0x1E && back.parentX == 3 && back.parentY == 4 && back.parentFacing == 1);
    const ff6::SaveCharacter& character = back.characters[0];
    CHECK(character.enabled && character.visible && character.party == 2 && character.slot == 1 && character.map == 0x1C);
    CHECK(character.gfx == 164 && character.palette == 7 && character.x == 8 && character.y == 45 && character.facing == 3);
    CHECK(!back.characters[1].enabled);
    {  // the event variables and the timers are kept too
        ff6::SaveState withVariables;
        withVariables.variables.assign(FF6_EVENT_VAR_COUNT, 0);
        withVariables.variables[7] = 65535;
        withVariables.timers = {0x10, 90, 0x1234, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        const ff6::SaveState kept = ff6::saveFromJson(ff6::saveToJson(withVariables));
        CHECK(kept.variables == withVariables.variables && kept.timers == withVariables.timers);
        CHECK(back.variables.empty() && back.timers.empty());
    }
    CHECK(back.config == state.config);
    CHECK(back.hasColors && back.fontColor == 0x1234 && back.windowColors == state.windowColors);
    CHECK(back.hasParty && back.party.gil == 1234567 && back.party.steps == 4321);
    const ff6::CharacterState& terra = back.party.characters[0];
    CHECK(terra.level == 12 && terra.hp == 150 && terra.maxHp == 210 && terra.mp == 30 && terra.maxMp == 44);
    CHECK(terra.experience == 1234 && terra.vigor == 31 && terra.speed == 33 && terra.stamina == 28 && terra.magicPower == 39);
    CHECK(terra.equipment == state.party.characters[0].equipment);
    CHECK(terra.status == (FF6_ST_POISON | FF6_ST_BLIND) && back.party.characters[1].status == 0);
    CHECK(back.party.inventory[0].item == 233 && back.party.inventory[0].quantity == 5);
    CHECK(back.party.inventory[1].item == 0 && back.party.inventory[1].quantity == 99);
    CHECK(back.party.inventory[2].item == FF6_ITEM_EMPTY);
    CHECK(back.party.treasures[11] && back.party.treasures[511] && !back.party.treasures[12]);
    CHECK(back.airshipX == 137 && back.airshipY == 199 && back.doomGazeCounter == 99);
    CHECK(back.party.veldtBattles[17] && back.party.veldtBattles[511] && !back.party.veldtBattles[18] && back.party.veldtPointer == 63);
    CHECK(back.party.spells[0][45] && back.party.spells[6][1] && !back.party.spells[0][1] && !back.party.spells[1][45]);
    CHECK(back.party.spellLearn[0][3] == 70 && back.party.spellLearn[0][4] == 0 && back.party.rages[0x19] && back.party.rages[255] &&
          !back.party.rages[0x1A] && back.party.dances == 0x81 && back.party.morphCounter == 123);
    CHECK(back.party.espers[3] && !back.party.espers[4] && back.party.swordtechs == 0x0F && back.party.blitzes == 0x81);
    CHECK(!ff6::saveFromJson(ff6::saveToJson(ff6::SaveState())).hasParty);  // an older save has none
    {  // a save without config has the defaults of a new game
        std::string old = ff6::saveToJson(ff6::SaveState());
        old.erase(old.find(",\n  \"config\""), old.find("]", old.find("\"config\"")) + 1 - old.find(",\n  \"config\""));
        CHECK(ff6::saveFromJson(old).config[0] == FF6_CONFIG_BATTLE_DEFAULT);
    }

    // Values that do not exist are refused.
    CHECK(throws([] { ff6::saveFromJson("{}"); }));
    CHECK(throws([] { ff6::saveFromJson("[]"); }));
    std::string text = ff6::saveToJson(state);
    const size_t version = text.find("\"version\": 1");
    CHECK(version != std::string::npos);
    std::string other = text;
    other.replace(version, 12, "\"version\": 99");
    CHECK(throws([&] { ff6::saveFromJson(other); }));
    ff6::SaveState broken = state;
    broken.map = 5000;
    CHECK(throws([&] { ff6::saveFromJson(ff6::saveToJson(broken)); }));
    broken = state;
    broken.facing = 7;
    CHECK(throws([&] { ff6::saveFromJson(ff6::saveToJson(broken)); }));
    broken = state;
    broken.characters[3].palette = 8;
    CHECK(throws([&] { ff6::saveFromJson(ff6::saveToJson(broken)); }));
    broken = state;
    broken.config[1] = 300;
    CHECK(throws([&] { ff6::saveFromJson(ff6::saveToJson(broken)); }));
    broken = state;
    broken.windowColors[0] = 0x8000;
    CHECK(throws([&] { ff6::saveFromJson(ff6::saveToJson(broken)); }));
    broken = state;
    broken.eventBits.push_back(100000);
    CHECK(throws([&] { ff6::saveFromJson(ff6::saveToJson(broken)); }));
    broken = state;
    broken.party.gil = FF6_MAX_GIL + 1;
    CHECK(throws([&] { ff6::saveFromJson(ff6::saveToJson(broken)); }));
    broken = state;
    broken.party.inventory[0].quantity = 100;
    CHECK(throws([&] { ff6::saveFromJson(ff6::saveToJson(broken)); }));
    broken = state;
    broken.party.characters[2].level = 0;
    CHECK(throws([&] { ff6::saveFromJson(ff6::saveToJson(broken)); }));

    // A file is written and read again.
    const std::string path = "ff6-save-test.json";
    ff6::writeSave(path, state);
    CHECK(ff6::readSave(path).eventBits == state.eventBits);
    std::remove(path.c_str());
    CHECK(throws([&] { ff6::readSave(path); }));
}
