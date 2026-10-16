#pragma once

#include <cstdint>
#include <vector>

#include "check.hpp"
#include "data/pack.hpp"
#include "field/event.hpp"
#include "field/map.hpp"
#include "field/obj.hpp"
#include "renderer.hpp"

namespace test {

// An open map: every tile of the first layer can be walked on from any side, on the upper level.
inline ff6::MapData makeMap(int width = 16, int height = 16) {
    ff6::MapData map;
    for (ff6::MapLayout& layout : map.layers) {
        layout.width = width;
        layout.height = height;
        layout.cells.assign(static_cast<size_t>(width) * height, 0);
    }
    map.layers[FF6_BG1].cells.assign(static_cast<size_t>(width) * height, 1);
    map.tileProps[1] = 0x01;
    map.tileProps[FF6_TILE_PROPS_HIGH + 1] = 0x0F;
    for (auto& tileset : map.tilesets) tileset.assign(FF6_TILESET_WORDS, 0);
    map.vram.assign(FF6_VRAM_BYTES, 0);
    map.bg3Gfx.assign(FF6_BG3_GFX_BYTES, 0);
    return map;
}

// Gives one cell its own tile property word (high byte: passability, low byte: level flags).
inline void setProp(ff6::MapData& map, int x, int y, uint16_t prop) {
    size_t tile = 2;
    while (map.tileProps[tile] != 0 || map.tileProps[FF6_TILE_PROPS_HIGH + tile] != 0) ++tile;
    map.tileProps[tile] = prop & 0xFF;
    map.tileProps[FF6_TILE_PROPS_HIGH + tile] = static_cast<uint8_t>(prop >> 8);
    map.layers[FF6_BG1].cells[static_cast<size_t>(y) * map.layers[FF6_BG1].width + x] = static_cast<uint8_t>(tile);
}

inline ff6::CommonData makeCommon(std::vector<uint8_t> script = {}) {
    ff6::CommonData common;
    common.fontWidths.fill(8);
    common.fontWidths[FF6_CODE_SPACE] = 4;
    common.fontWidths[FF6_CODE_WIDE_SPACE] = 6;
    common.npcSwitchDefaults.assign(FF6_NPC_SWITCH_BYTES, 0);
    common.eventScript = std::move(script);
    common.dialogueOffsets = {0};
    return common;
}

// A 9-byte npc property item: the other fields keep their defaults.
inline std::vector<uint8_t> npcBytes(int x, int y, int switchId, int gfx, int palette, int facing, int movement = 0,
                                     int speed = FF6_OBJ_DEFAULT_SPEED) {
    std::vector<uint8_t> bytes(FF6_NPC_PROP_SIZE, 0);
    bytes[0] = 0xA7;
    bytes[1] = 0xD2;
    bytes[2] = static_cast<uint8_t>(0x02 | (palette << FF6_NPC_PAL_SHIFT) | ((switchId & 3) << FF6_NPC_SWITCH_SHIFT));
    bytes[3] = static_cast<uint8_t>(switchId >> 2);
    bytes[4] = static_cast<uint8_t>(x);
    bytes[5] = static_cast<uint8_t>(y | (speed << 6));
    bytes[6] = static_cast<uint8_t>(gfx);
    bytes[7] = static_cast<uint8_t>(movement);
    bytes[8] = static_cast<uint8_t>(facing);
    return bytes;
}

// An open map where npcs may wander, with npcs 0, 1, ... (switch ids 0, 1, ...) all visible, and an event script.
struct ObjScene {
    ff6::MapData map = makeMap();
    ff6::CommonData common;
    ff6::EventBits bits;
    ff6::Objects objects;
    uint32_t frame = 0;

    ObjScene(std::vector<uint8_t> script, const std::vector<std::vector<uint8_t>>& npcs)
        : common(makeCommon(std::move(script))) {
        map.tileProps[FF6_TILE_PROPS_HIGH + 1] = 0x8F;  // passable from every side, and npcs may wander onto it
        bits.init(common);
        for (size_t i = 0; i < npcs.size(); ++i) {
            map.npcProps.insert(map.npcProps.end(), npcs[i].begin(), npcs[i].end());
            bits.set(FF6_NPC_SWITCH_EVENT_BIT + static_cast<int>(i), true);
        }
        objects.init(map, bits);
    }
    void run(int frames, bool eventBusy = false) {
        for (int i = 0; i < frames; ++i) objects.update(map, common.eventScript, bits, ++frame, eventBusy);
    }
    ff6::Npc& npc(size_t index = 0) { return objects.npcs()[index]; }
    // A character of the party stands on a tile of the map.
    ff6::Npc& placeCharacter(size_t id, int tileX, int tileY) {
        ff6::Npc& character = objects.characters()[id];
        character.enabled = character.visible = true;
        character.map = objects.mapId();
        character.posX = tileX * FF6_OBJ_TILE_SUBPIXELS;
        character.posY = tileY * FF6_OBJ_TILE_SUBPIXELS;
        return character;
    }
};

// Renderer double: records what the game asks for, so the game is tested without any graphics library.
class RecordingRenderer : public ff6::Renderer {
public:
    struct Texture {
        int width, height;
    };
    struct Draw {
        ff6::TextureId texture;
        ff6::Rect source, destination;
    };
    std::vector<Texture> textures;
    std::vector<Draw> draws;  // of the current frame
    std::vector<ff6::TextureId> updates;
    int frames = 0;
    bool inFrame = false, drewOutsideFrame = false;

    ff6::TextureId createTexture(int width, int height, const uint8_t*, ff6::TextureUsage) override {
        textures.push_back({width, height});
        return static_cast<ff6::TextureId>(textures.size() - 1);
    }
    void updateTexture(ff6::TextureId texture, const uint8_t*) override { updates.push_back(texture); }
    void beginFrame() override {
        inFrame = true;
        draws.clear();
    }
    void draw(ff6::TextureId texture, const ff6::Rect& source, const ff6::Rect& destination) override {
        if (!inFrame) drewOutsideFrame = true;
        draws.push_back({texture, source, destination});
    }
    void endFrame() override {
        inFrame = false;
        ++frames;
    }
};

// Event host double.
struct FakeHost : ff6::EventHost {
    std::vector<uint32_t> opened;
    bool busy = false;
    int count = 0, choice = 0;
    struct MapChange {
        uint32_t map;
        int x, y, facing;
    };
    std::vector<MapChange> mapChanges;
    // Object scripts and commands: what the interpreter asked for, and which objects count as busy.
    struct ScriptStart {
        int object;
        uint32_t offset;
    };
    struct ObjectCommand {
        uint8_t op;
        int object;
        uint32_t arg;
    };
    bool supportsScripts = false;
    std::vector<ScriptStart> scripts;
    std::vector<ObjectCommand> objectCommands;
    std::vector<int> busyObjects;
    struct FieldCommand {
        uint8_t op;
        std::vector<uint8_t> bytes;
    };
    std::vector<FieldCommand> fieldCommands;

    void openDialogue(uint32_t id, uint16_t) override {
        opened.push_back(id);
        busy = true;
    }
    bool dialogueBusy() const override { return busy; }
    int dialogueChoiceCount() const override { return count; }
    int dialogueChoice() const override { return choice; }
    void changeMap(uint32_t map, int x, int y, int facing) override { mapChanges.push_back({map, x, y, facing}); }
    bool startObjectScript(int object, uint32_t offset) override {
        scripts.push_back({object, offset});
        return supportsScripts;
    }
    bool objectBusy(int object) const override {
        for (int busyObject : busyObjects) {
            if (busyObject == object) return true;
        }
        return false;
    }
    void objectCommand(uint8_t op, int object, uint32_t arg) override { objectCommands.push_back({op, object, arg}); }
    void fieldCommand(uint8_t op, const uint8_t* bytes, size_t size) override {
        fieldCommands.push_back({op, std::vector<uint8_t>(bytes, bytes + size)});
    }
};

}  // namespace test
