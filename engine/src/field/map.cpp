#include "field/map.hpp"

#include <cstdio>
#include <fstream>
#include <stdexcept>

#include "data/pack.hpp"

namespace ff6 {
namespace {

std::string mapPath(const std::string& packDir, uint32_t mapId) {
    char name[16];
    std::snprintf(name, sizeof(name), "%03u", static_cast<unsigned>(mapId));
    return packDir + "/" FF6_MAP_DIR "/" + name + FF6_MAP_FILE_EXTENSION;
}

int layerSize(int bits) { return FF6_LAYER_BASE_TILES << bits; }

std::vector<uint8_t> readCounted(ByteReader& reader, size_t itemSize) {
    const uint32_t count = reader.u32();
    if (count > FF6_MAX_PACK_ITEMS) throw std::runtime_error("invalid item count");
    return reader.bytes(static_cast<size_t>(count) * itemSize);
}

}  // namespace

int MapData::layerOffsetX(int layer) const {
    return prop[layer == FF6_BG2 ? FF6_MAP_PROP_BG2_OFFSET_X : FF6_MAP_PROP_BG3_OFFSET_X];
}

int MapData::layerOffsetY(int layer) const {
    return prop[layer == FF6_BG2 ? FF6_MAP_PROP_BG2_OFFSET_Y : FF6_MAP_PROP_BG3_OFFSET_Y];
}

uint16_t MapData::propAt(int x, int y) const {
    const MapLayout& layout = layers[FF6_BG1];
    x = ((x % layout.width) + layout.width) % layout.width;
    y = ((y % layout.height) + layout.height) % layout.height;
    const uint8_t tile = layout.cells[static_cast<size_t>(y) * layout.width + x];
    return static_cast<uint16_t>(tileProps[tile] | (tileProps[FF6_TILE_PROPS_HIGH + tile] << 8));
}

bool mapExists(const std::string& packDir, uint32_t mapId) {
    return std::ifstream(mapPath(packDir, mapId), std::ios::binary).good();
}

MapData loadMap(const std::string& packDir, uint32_t mapId) {
    const std::vector<uint8_t> data = readFile(mapPath(packDir, mapId));
    ByteReader reader(data);
    reader.expectMagic(FF6_MAP_MAGIC, "map");
    MapData map;
    map.id = reader.u32();
    if (map.id != mapId) throw std::runtime_error("map file has the wrong map number");
    reader.readInto(map.prop.data(), map.prop.size());
    const uint8_t sizes12 = map.prop[FF6_MAP_PROP_SIZES_12];
    const uint8_t sizes3 = map.prop[FF6_MAP_PROP_SIZES_3];
    const int widthBits[FF6_LAYER_COUNT] = {sizes12 >> 6, (sizes12 >> 2) & 3, sizes3 >> 6};
    const int heightBits[FF6_LAYER_COUNT] = {(sizes12 >> 4) & 3, sizes12 & 3, (sizes3 >> 4) & 3};
    map.startX = reader.u8();
    map.startY = reader.u8();
    map.startFacing = reader.u8() % FF6_DIRECTION_COUNT;
    reader.readInto(map.tileProps.data(), map.tileProps.size());
    for (int layer = 0; layer < FF6_LAYER_COUNT; ++layer) {
        MapLayout& layout = map.layers[static_cast<size_t>(layer)];
        layout.width = layerSize(widthBits[layer]);
        layout.height = layerSize(heightBits[layer]);
        layout.cells = reader.bytes(static_cast<size_t>(layout.width) * layout.height);
    }
    for (auto& tileset : map.tilesets) tileset = reader.words(FF6_TILESET_WORDS);
    map.vram = reader.bytes(FF6_VRAM_BYTES);
    reader.readInto(map.bg3Palette.data(), map.bg3Palette.size());
    map.bg3Gfx = reader.bytes(FF6_BG3_GFX_BYTES);
    for (uint16_t& color : map.palette) color = reader.u16();
    for (BgAnimTile& tile : map.bgAnim) {
        tile.speed = reader.u16();
        for (auto& frame : tile.frames) reader.readInto(frame.data(), frame.size());
    }
    map.bg3Anim.speed = reader.u16();
    map.bg3Anim.size = reader.u16();
    for (uint16_t& frame : map.bg3Anim.frames) frame = reader.u16();
    map.bg3Anim.data = reader.bytes(reader.u32());
    reader.readInto(map.parallax.data(), map.parallax.size());
    reader.readInto(map.colorMath.data(), map.colorMath.size());
    map.npcProps = readCounted(reader, FF6_NPC_PROP_SIZE);
    map.eventTriggers = readCounted(reader, FF6_EVENT_TRIGGER_SIZE);
    map.shortEntrances = readCounted(reader, FF6_SHORT_ENTRANCE_SIZE);
    map.longEntrances = readCounted(reader, FF6_LONG_ENTRANCE_SIZE);
    map.startupEvent = reader.u32();
    if (reader.remaining() != 0) throw std::runtime_error("map file has trailing bytes");
    return map;
}

}  // namespace ff6
