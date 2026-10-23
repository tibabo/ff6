#include "data/save.hpp"

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
    out << ",\n  \"spritePalettes\": ";
    appendList(out, std::vector<int>(state.spritePalettes.begin(), state.spritePalettes.end()));
    out << ",\n  \"parent\": ";
    if (state.hasParent) {
        out << "{\"map\": " << state.parentMap << ", \"x\": " << state.parentX << ", \"y\": " << state.parentY
            << ", \"facing\": " << state.parentFacing << "}";
    } else {
        out << "null";
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
