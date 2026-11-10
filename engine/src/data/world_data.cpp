#include "data/world_data.hpp"

#include <stdexcept>

#include "data/pack.hpp"

namespace ff6 {

WorldData loadWorldData(const std::string& packDir) {
    const std::vector<uint8_t> file = readFile(packDir + "/" FF6_WORLD_FILE);
    ByteReader reader(file);
    reader.expectMagic(FF6_WORLD_MAGIC, "world data");
    WorldData data;
    for (WorldMapData& world : data.worlds) {
        world.tilemap = reader.bytes(FF6_WORLD_TILEMAP_BYTES);
        world.formation = reader.bytes(FF6_WORLD_FORMATION_BYTES);
        world.chr = reader.bytes(FF6_WORLD_CHR_BYTES);
        world.palette = reader.words(FF6_WORLD_PALETTE_COLORS);
        world.spritePalette = reader.words(FF6_WORLD_PALETTE_COLORS);
        world.tileProps = reader.words(FF6_WORLD_TILE_PROPS);
        world.minimap = reader.bytes(FF6_WORLD_MINIMAP_BYTES);
        const uint16_t modCount = reader.u16();
        for (uint16_t i = 0; i < modCount; ++i) {
            WorldMod mod;
            mod.bit = reader.u16();
            mod.x = reader.u8();
            mod.y = reader.u8();
            mod.width = reader.u8();
            mod.height = reader.u8();
            mod.tiles = reader.bytes(static_cast<size_t>(mod.width) * mod.height);
            world.mods.push_back(std::move(mod));
        }
        world.shortEntrances = reader.bytes(static_cast<size_t>(reader.u16()) * FF6_WORLD_SHORT_ENTRANCE_SIZE);
        world.eventTriggers = reader.bytes(static_cast<size_t>(reader.u16()) * FF6_WORLD_EVENT_TRIGGER_SIZE);
    }
    data.miscSprites = reader.bytes(FF6_WORLD_MISC_SPRITE_BYTES);
    data.battleGroups = reader.bytes(FF6_WORLD_BATTLE_GROUP_BYTES);
    data.battleRates = reader.bytes(FF6_WORLD_BATTLE_RATE_BYTES);
    data.animPointers = reader.words(FF6_WORLD_ANIM_FRAMES);
    data.animData = reader.bytes(FF6_WORLD_ANIM_DATA_BYTES);
    data.animGfx = reader.bytes(FF6_WORLD_ANIM_GFX_BYTES);
    for (auto& gfx : data.airshipGfx) gfx = reader.bytes(FF6_WORLD_AIRSHIP_GFX_BYTES);
    data.backdropGfx = reader.bytes(FF6_WORLD_BACKDROP_GFX_BYTES);
    data.backdropTiles = reader.bytes(FF6_WORLD_BACKDROP_TILES_BYTES);
    for (auto& gfx : data.chocoboGfx) gfx = reader.bytes(FF6_WORLD_CHOCOBO_GFX_BYTES);
    data.trench.tilemap = reader.bytes(FF6_TRENCH_TILEMAP_BYTES);
    data.trench.formation = reader.bytes(FF6_WORLD_FORMATION_BYTES);
    data.trench.chr = reader.bytes(FF6_WORLD_CHR_BYTES);
    data.trench.palette = reader.words(FF6_WORLD_PALETTE_COLORS);
    data.trench.spritePalette = reader.words(FF6_WORLD_PALETTE_COLORS);
    data.trench.tileProps.assign(FF6_WORLD_TILE_PROPS, 0);
    data.trench.minimap.assign(FF6_WORLD_MINIMAP_BYTES, 0);
    TrainData& train = data.train;
    train.gfx = reader.bytes(FF6_TRAIN_GFX_BYTES);
    train.tilePointers = reader.words(FF6_TRAIN_TILE_POINTERS);
    train.palette = reader.words(FF6_TRAIN_PALETTE_COLORS);
    train.pitch1 = reader.bytes(0x1A0);
    train.pitch2 = reader.bytes(0x1A0);
    train.reduce = reader.bytes(0x40);
    train.yawMultiplier = reader.bytes(0x20);
    train.yaw = reader.bytes(0x60);
    train.background = reader.bytes(0xA0);
    train.tiles = reader.bytes(0x3C0);
    train.course = reader.bytes(FF6_TRAIN_COURSE_RECORDS * FF6_TRAIN_COURSE_RECORD_BYTES);
    if (reader.remaining() != 0) throw std::runtime_error("world data has trailing bytes");
    return data;
}

}  // namespace ff6
