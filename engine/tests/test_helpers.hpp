#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "audio/audio_data.hpp"
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

// Sound double: the commands that the game sent, and the ports and previous song that the tests set.
struct RecordingSound : ff6::Sound {
    struct Command {
        uint8_t command, arg1, arg2;
    };
    std::vector<Command> commands;
    uint8_t ports[4] = {0, 0, 0, 0};
    int previous = -1;

    void command(uint8_t command, uint8_t arg1, uint8_t arg2) override { commands.push_back({command, arg1, arg2}); }
    uint8_t port(int index) const override { return ports[index & 3]; }
    int previousSong() const override { return previous; }
};

// Event host double.
struct FakeHost : ff6::EventHost {
    std::vector<uint32_t> opened;
    bool busy = false;
    int count = 0, choice = 0;
    struct MapChange {
        uint32_t map;
        int x, y, facing;
        bool startupEvent, fadeIn;
    };
    std::vector<MapChange> mapChanges;
    bool fadeBusy = false;
    RecordingSound recording;
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
    void changeMap(uint32_t map, int x, int y, int facing, bool startupEvent, bool fadeIn) override {
        mapChanges.push_back({map, x, y, facing, startupEvent, fadeIn});
    }
    int vehicle = 0;  // what the last map change said that the party is on
    void setVehicle(int value) override { vehicle = value; }
    bool fading() const override { return fadeBusy; }
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
    ff6::Sound& sound() override { return recording; }
};

// One instrument with a pitch multiplier of 0x0100, and songs whose first channel runs `body`.
inline ff6::AudioData makeAudio(const std::vector<uint8_t>& body, int songCount = 1) {
    ff6::AudioData audio;
    ff6::Sample sample;
    sample.freqMult = 0x0100;
    sample.adsr1 = 0xFF;
    sample.adsr2 = 0xE0;
    sample.brr = {0xC3, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};  // constant, looping
    audio.samples.push_back(sample);
    ff6::Song song;
    song.samples[0] = 1;
    const uint16_t base = 0x0100;
    const uint16_t end = static_cast<uint16_t>(base + body.size());
    auto put = [&](uint16_t value) {
        song.script.push_back(static_cast<uint8_t>(value));
        song.script.push_back(static_cast<uint8_t>(value >> 8));
    };
    put(base);
    put(end);
    for (int i = 0; i < 16; ++i) put(i % 8 == 0 ? base : end);  // channel 0 starts the body; the others are unused
    song.script.insert(song.script.end(), body.begin(), body.end());
    for (int i = 0; i < songCount; ++i) audio.songs.push_back(song);
    // Sound effects: instrument 0 is the same sample; game effect 0 and system effect 0 play a quarter note.
    audio.sfxSamples.push_back(sample);
    audio.sfxMemory.assign(FF6_SPC_MEMORY_BYTES, 0);
    const auto poke = [&](size_t at, const std::vector<uint8_t>& bytes) {
        std::copy(bytes.begin(), bytes.end(), audio.sfxMemory.begin() + static_cast<std::ptrdiff_t>(at));
    };
    const std::vector<uint8_t> effect = {0xC4, 0x7F, 0xDC, 0x00, 0xD6, 0x05, 0x04, 0xEB};
    poke(FF6_SFX_POINTERS, {0x00, 0x30, 0x00, 0x00});
    poke(0x3000, effect);
    poke(FF6_SYSTEM_SFX_TABLE, {0x00, 0x31});
    poke(0x3100, effect);
    return audio;
}

// The script that every test starts with: tempo 120, full volume, centered, instrument 0x20, octave 5.
inline std::vector<uint8_t> prologue() { return {0xF0, 0x78, 0xC4, 0x7F, 0xC6, 0x40, 0xDC, 0x20, 0xD6, 0x05}; }

}  // namespace test
