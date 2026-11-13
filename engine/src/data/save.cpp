#include "data/save.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "data/json.hpp"

namespace ff6 {
namespace {

void appendList(std::ostringstream& out, const std::vector<int>& values) {
    out << "[";
    for (size_t i = 0; i < values.size(); ++i) out << (i ? ", " : "") << values[i];
    out << "]";
}

// A whole number between two limits, from a member of an object.
int number(const Json& object, const char* key, int low, int high) {
    const Json* value = object.find(key);
    if (!value || value->type != Json::Type::Number) throw std::runtime_error(std::string("save: missing number ") + key);
    if (value->number < low || value->number > high) throw std::runtime_error(std::string("save: ") + key + " is out of range");
    return static_cast<int>(value->number);
}

bool flag(const Json& object, const char* key) {
    const Json* value = object.find(key);
    if (!value || value->type != Json::Type::Bool) throw std::runtime_error(std::string("save: missing flag ") + key);
    return value->boolean;
}

// A whole number between two limits, from a list.
int element(const Json& array, size_t index, int low, int high) {
    const Json& value = array.items[index];
    if (value.type != Json::Type::Number || value.number < low || value.number > high) throw std::runtime_error("save: bad item");
    return static_cast<int>(value.number);
}

const Json& list(const Json& object, const char* key) {
    const Json* value = object.find(key);
    if (!value || value->type != Json::Type::Array) throw std::runtime_error(std::string("save: missing list ") + key);
    return *value;
}

std::vector<int> ids(const Json& object, const char* key, int count) {
    std::vector<int> result;
    for (const Json& item : list(object, key).items) {
        if (item.type != Json::Type::Number || item.number < 0 || item.number >= count) {
            throw std::runtime_error(std::string("save: bad id in ") + key);
        }
        result.push_back(static_cast<int>(item.number));
    }
    return result;
}

}  // namespace

std::string saveToJson(const SaveState& state) {
    std::ostringstream out;
    out << "{\n  \"version\": " << FF6_SAVE_VERSION << ",\n";
    out << "  \"map\": " << state.map << ", \"x\": " << state.x << ", \"y\": " << state.y << ", \"facing\": " << state.facing << ",\n";
    out << "  \"activeParty\": " << state.activeParty << ",\n  \"characters\": [\n";
    for (size_t i = 0; i < state.characters.size(); ++i) {
        const SaveCharacter& c = state.characters[i];
        out << "    {\"enabled\": " << (c.enabled ? "true" : "false") << ", \"visible\": " << (c.visible ? "true" : "false")
            << ", \"party\": " << c.party << ", \"slot\": " << c.slot << ", \"map\": " << c.map << ", \"gfx\": " << c.gfx
            << ", \"palette\": " << c.palette << ", \"x\": " << c.x << ", \"y\": " << c.y << ", \"facing\": " << c.facing << "}"
            << (i + 1 < state.characters.size() ? "," : "") << "\n";
    }
    out << "  ],\n  \"eventBits\": ";
    appendList(out, state.eventBits);
    out << ",\n  \"battleFlags\": ";
    appendList(out, state.battleFlags);
    out << ",\n  \"variables\": ";
    appendList(out, state.variables);
    out << ", \"timers\": ";
    appendList(out, state.timers);
    out << ",\n  \"spritePalettes\": ";
    appendList(out, std::vector<int>(state.spritePalettes.begin(), state.spritePalettes.end()));
    out << ",\n  \"parent\": ";
    if (state.hasParent) {
        out << "{\"map\": " << state.parentMap << ", \"x\": " << state.parentX << ", \"y\": " << state.parentY
            << ", \"facing\": " << state.parentFacing << "}";
    } else {
        out << "null";
    }
    out << ",\n  \"airship\": {\"x\": " << state.airshipX << ", \"y\": " << state.airshipY << ", \"doomGaze\": " << state.doomGazeCounter << "}";
    out << ",\n  \"playFrames\": " << state.playFrames;
    out << ",\n  \"config\": ";
    appendList(out, std::vector<int>(state.config.begin(), state.config.end()));
    if (!state.names.empty()) {
        out << ",\n  \"names\": [";
        for (size_t i = 0; i < state.names.size(); ++i) out << (i ? ", " : "") << jsonQuote(state.names[i]);
        out << "]";
    }
    if (state.hasColors) {
        out << ",\n  \"fontColor\": " << state.fontColor << ",\n  \"windowColors\": ";
        appendList(out, std::vector<int>(state.windowColors.begin(), state.windowColors.end()));
    }
    if (state.hasParty) {
        const PartyState& party = state.party;
        out << ",\n  \"gil\": " << party.gil << ", \"steps\": " << party.steps << ",\n  \"stats\": [\n";
        for (size_t i = 0; i < party.characters.size(); ++i) {
            const CharacterState& c = party.characters[i];
            out << "    {\"actor\": " << c.actor << ", \"level\": " << c.level << ", \"hp\": " << c.hp << ", \"maxHp\": " << c.maxHp
                << ", \"mp\": " << c.mp << ", \"maxMp\": " << c.maxMp << ", \"experience\": " << c.experience
                << ", \"vigor\": " << c.vigor << ", \"speed\": " << c.speed << ", \"stamina\": " << c.stamina
                << ", \"magicPower\": " << c.magicPower << ", \"esper\": " << c.esper << ", \"backRow\": " << (c.backRow ? 1 : 0) << ", \"status\": " << c.status << ", \"equipment\": ";
            appendList(out, std::vector<int>(c.equipment.begin(), c.equipment.end()));
            std::vector<int> known;
            for (size_t spell = 0; spell < party.spells[i].size(); ++spell) {
                if (party.spells[i][spell]) known.push_back(static_cast<int>(spell));
            }
            out << ", \"spells\": ";
            appendList(out, known);
            out << ", \"learn\": [";
            bool firstLearn = true;
            for (size_t spell = 0; spell < party.spellLearn[i].size(); ++spell) {
                if (party.spellLearn[i][spell] == 0 || party.spells[i][spell]) continue;
                out << (firstLearn ? "" : ", ") << "[" << spell << ", " << static_cast<int>(party.spellLearn[i][spell]) << "]";
                firstLearn = false;
            }
            out << "]}" << (i + 1 < party.characters.size() ? "," : "") << "\n";
        }
        out << "  ],\n  \"items\": [";
        bool first = true;
        for (const InventorySlot& slot : party.inventory) {
            if (slot.item == FF6_ITEM_EMPTY) continue;
            out << (first ? "" : ", ") << "[" << slot.item << ", " << slot.quantity << "]";
            first = false;
        }
        out << "]";
        std::vector<int> opened;
        for (size_t i = 0; i < party.treasures.size(); ++i) {
            if (party.treasures[i]) opened.push_back(static_cast<int>(i));
        }
        out << ",\n  \"treasures\": ";
        appendList(out, opened);
        std::vector<int> espers;
        for (size_t i = 0; i < party.espers.size(); ++i) {
            if (party.espers[i]) espers.push_back(static_cast<int>(i));
        }
        out << ",\n  \"espers\": ";
        appendList(out, espers);
        out << ", \"swordtechs\": " << party.swordtechs << ", \"blitzes\": " << party.blitzes << ", \"lores\": " << party.lores
            << ", \"dances\": " << party.dances << ", \"morph\": " << party.morphCounter;
        std::vector<int> rages;
        for (size_t i = 0; i < party.rages.size(); ++i) {
            if (party.rages[i]) rages.push_back(static_cast<int>(i));
        }
        out << ", \"rages\": ";
        appendList(out, rages);
        std::vector<int> veldt;
        for (size_t i = 0; i < party.veldtBattles.size(); ++i) {
            if (party.veldtBattles[i]) veldt.push_back(static_cast<int>(i));
        }
        out << ",\n  \"veldt\": ";
        appendList(out, veldt);
        out << ", \"veldtPointer\": " << party.veldtPointer;
    }
    out << "\n}\n";
    return out.str();
}

SaveState saveFromJson(const std::string& text) {
    const Json root = parseJson(text);
    if (root.type != Json::Type::Object) throw std::runtime_error("save: the file is not an object");
    if (number(root, "version", 0, 0x7FFFFFFF) != FF6_SAVE_VERSION) throw std::runtime_error("save: unsupported version");
    SaveState state;
    state.map = number(root, "map", 0, FF6_ENTRANCE_MAP_MASK);
    state.x = number(root, "x", 0, FF6_MAX_TILE);
    state.y = number(root, "y", 0, FF6_MAX_TILE);
    state.facing = number(root, "facing", 0, FF6_DIRECTION_COUNT - 1);
    state.activeParty = number(root, "activeParty", 0, FF6_PARTY_COUNT - 1);
    const Json& characters = list(root, "characters");
    if (characters.items.size() != state.characters.size()) throw std::runtime_error("save: there must be 16 characters");
    for (size_t i = 0; i < state.characters.size(); ++i) {
        const Json& item = characters.items[i];
        SaveCharacter& c = state.characters[i];
        c.enabled = flag(item, "enabled");
        c.visible = flag(item, "visible");
        c.party = number(item, "party", 0, FF6_PARTY_COUNT - 1);
        c.slot = number(item, "slot", 0, FF6_PARTY_SIZE - 1);
        c.map = number(item, "map", -1, FF6_ENTRANCE_MAP_MASK);
        c.gfx = number(item, "gfx", 0, FF6_SPRITE_GFX_TOTAL - 1);
        c.palette = number(item, "palette", 0, FF6_SPRITE_PALETTE_SLOTS - 1);
        c.x = number(item, "x", 0, FF6_MAX_TILE);
        c.y = number(item, "y", 0, FF6_MAX_TILE);
        c.facing = number(item, "facing", 0, FF6_DIRECTION_COUNT - 1);
    }
    state.eventBits = ids(root, "eventBits", FF6_EVENT_BIT_COUNT);
    state.battleFlags = ids(root, "battleFlags", FF6_BATTLE_FLAG_BYTES * 8);
    if (root.find("variables")) {  // saves from before the variables and the timers do not have them
        state.variables = ids(root, "variables", FF6_BYTE_VALUES * FF6_BYTE_VALUES);
        state.timers = ids(root, "timers", FF6_EVENT_SCRIPT_LIMIT);
        if (state.variables.size() != 0 && state.variables.size() != FF6_EVENT_VAR_COUNT) throw std::runtime_error("save: there must be 256 variables");
        if (state.timers.size() != 0 && state.timers.size() != FF6_EVENT_TIMER_COUNT * 3) throw std::runtime_error("save: there must be 4 timers");
    }
    const std::vector<int> palettes = ids(root, "spritePalettes", FF6_SPRITE_PALETTE_TOTAL);
    if (palettes.size() != state.spritePalettes.size()) throw std::runtime_error("save: there must be 8 sprite palettes");
    std::copy(palettes.begin(), palettes.end(), state.spritePalettes.begin());
    const Json* parent = root.find("parent");
    if (parent && parent->type == Json::Type::Object) {
        state.hasParent = true;
        state.parentMap = number(*parent, "map", 0, FF6_ENTRANCE_MAP_MASK);
        state.parentX = number(*parent, "x", 0, FF6_MAX_TILE);
        state.parentY = number(*parent, "y", 0, FF6_MAX_TILE);
        state.parentFacing = number(*parent, "facing", 0, FF6_DIRECTION_COUNT - 1);
    }
    const Json* airship = root.find("airship");
    if (airship && airship->type == Json::Type::Object) {
        state.airshipX = number(*airship, "x", 0, FF6_WORLD_SIZE - 1);
        state.airshipY = number(*airship, "y", 0, FF6_WORLD_SIZE - 1);
        state.doomGazeCounter = number(*airship, "doomGaze", 0, 255);
    }
    if (root.find("playFrames")) state.playFrames = number(root, "playFrames", 0, FF6_SAVE_MAX_PLAY_FRAMES);
    if (root.find("config")) {
        const std::vector<int> config = ids(root, "config", FF6_BYTE_VALUES);
        if (config.size() != state.config.size()) throw std::runtime_error("save: there must be 3 config bytes");
        std::copy(config.begin(), config.end(), state.config.begin());
    }
    if (root.find("windowColors")) {
        state.fontColor = number(root, "fontColor", 0, FF6_MAX_COLOR);
        const std::vector<int> colors = ids(root, "windowColors", FF6_MAX_COLOR + 1);
        if (colors.size() != state.windowColors.size()) throw std::runtime_error("save: there must be 56 window colors");
        std::copy(colors.begin(), colors.end(), state.windowColors.begin());
        state.hasColors = true;
    }
    if (const Json* names = root.find("names")) {
        if (names->type != Json::Type::Array) throw std::runtime_error("save: bad names");
        for (const Json& item : names->items) {
            if (item.type != Json::Type::String) throw std::runtime_error("save: bad name");
            state.names.push_back(item.text.substr(0, 6));
        }
    }
    if (root.find("stats")) {
        PartyState& party = state.party;
        party.gil = number(root, "gil", 0, FF6_MAX_GIL);
        party.steps = number(root, "steps", 0, FF6_MAX_STEPS);
        const Json& stats = list(root, "stats");
        if (stats.items.size() != party.characters.size()) throw std::runtime_error("save: there must be 16 sets of statistics");
        for (size_t i = 0; i < party.characters.size(); ++i) {
            const Json& item = stats.items[i];
            CharacterState& c = party.characters[i];
            c.actor = number(item, "actor", 0, FF6_CHAR_PROP_COUNT - 1);
            c.level = number(item, "level", 1, FF6_MAX_LEVEL);
            c.maxHp = number(item, "maxHp", 0, FF6_MAX_HP);
            c.maxMp = number(item, "maxMp", 0, FF6_MAX_MP);
            c.hp = number(item, "hp", 0, FF6_MAX_HP);
            c.mp = number(item, "mp", 0, FF6_MAX_MP);
            c.experience = number(item, "experience", 0, FF6_MAX_EXPERIENCE);
            c.vigor = number(item, "vigor", 0, FF6_STAT_MAX);
            c.speed = number(item, "speed", 0, FF6_STAT_MAX);
            c.stamina = number(item, "stamina", 0, FF6_STAT_MAX);
            c.magicPower = number(item, "magicPower", 0, FF6_STAT_MAX);
            const std::vector<int> equipment = ids(item, "equipment", FF6_ITEM_COUNT);
            if (equipment.size() != c.equipment.size()) throw std::runtime_error("save: there must be 6 pieces of equipment");
            std::copy(equipment.begin(), equipment.end(), c.equipment.begin());
            if (item.find("status")) c.status = static_cast<unsigned>(number(item, "status", 0, 0x7FFFFFFF)) & FF6_ST_PERSISTENT;
            if (item.find("esper")) c.esper = number(item, "esper", -1, FF6_ESPER_COUNT - 1);
            if (item.find("backRow")) c.backRow = number(item, "backRow", 0, 1) != 0;
            if (item.find("spells")) {
                for (const int spell : ids(item, "spells", FF6_SPELL_COUNT)) party.spells[i][static_cast<size_t>(spell)] = true;
            }
            if (item.find("learn")) {
                for (const Json& pair : list(item, "learn").items) {
                    if (pair.type != Json::Type::Array || pair.items.size() != 2) throw std::runtime_error("save: bad spell progress");
                    party.spellLearn[i][static_cast<size_t>(element(pair, 0, 0, FF6_SPELL_COUNT - 1))] =
                        static_cast<uint8_t>(element(pair, 1, 0, 99));
                }
            }
        }
        size_t used = 0;
        for (const Json& pair : list(root, "items").items) {
            if (pair.type != Json::Type::Array || pair.items.size() != 2 || used >= party.inventory.size()) {
                throw std::runtime_error("save: bad item");
            }
            InventorySlot& slot = party.inventory[used++];
            slot.item = element(pair, 0, 0, FF6_ITEM_COUNT - 2);
            slot.quantity = element(pair, 1, 1, FF6_MAX_ITEM_QUANTITY);
        }
        if (root.find("treasures")) {
            for (const int id : ids(root, "treasures", FF6_TREASURE_BIT_COUNT)) party.treasures[static_cast<size_t>(id)] = true;
        }
        if (root.find("veldt")) {
            for (const int id : ids(root, "veldt", FF6_VELDT_BATTLE_COUNT)) party.veldtBattles[static_cast<size_t>(id)] = true;
            party.veldtPointer = number(root, "veldtPointer", 0, FF6_VELDT_BATTLE_COUNT / 8 - 1);
        }
        if (root.find("espers")) {
            for (const int id : ids(root, "espers", FF6_ESPER_COUNT)) party.espers[static_cast<size_t>(id)] = true;
            party.swordtechs = static_cast<unsigned>(number(root, "swordtechs", 0, (1 << FF6_TECH_COUNT) - 1));
            party.blitzes = static_cast<unsigned>(number(root, "blitzes", 0, (1 << FF6_TECH_COUNT) - 1));
            if (root.find("lores")) party.lores = static_cast<unsigned>(number(root, "lores", 0, (1 << FF6_LORE_COUNT) - 1));
            if (root.find("dances")) party.dances = static_cast<unsigned>(number(root, "dances", 0, (1 << FF6_DANCE_COUNT) - 1));
            if (root.find("morph")) party.morphCounter = number(root, "morph", 0, 255);
            if (root.find("rages")) {
                for (const int id : ids(root, "rages", FF6_RAGE_MONSTERS)) party.rages[static_cast<size_t>(id)] = true;
            }
        }
        state.hasParty = true;
    }
    return state;
}

SaveState readSave(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path);
    std::ostringstream text;
    text << in.rdbuf();
    return saveFromJson(text.str());
}

void writeSave(const std::string& path, const SaveState& state) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + path);
    out << saveToJson(state);
    if (!out) throw std::runtime_error("cannot write " + path);
}

}  // namespace ff6
