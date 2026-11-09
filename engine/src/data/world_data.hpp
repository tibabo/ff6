#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "constants.hpp"

namespace ff6 {

// A rectangle of tiles that replaces the map once an event bit is set (ModifyMap).
struct WorldMod {
    int bit = 0;
    int x = 0, y = 0, width = 0, height = 0;
    std::vector<uint8_t> tiles;  // width * height
};

// One of the world maps: the tiles, the graphics that the tiles are made of and what the party can do on each of them.
struct WorldMapData {
    std::vector<uint8_t> tilemap;  // FF6_WORLD_SIZE x FF6_WORLD_SIZE tile numbers
    std::vector<uint8_t> formation;  // for each tile, its four characters: top left, top right, bottom left, bottom right
    std::vector<uint8_t> chr;  // mode 7 characters: 64 bytes each, a color of the palette for each pixel
    std::vector<uint16_t> palette;  // the colors of the map, then the ones of the sprites (BGR555)
    std::vector<uint16_t> spritePalette;
    std::vector<uint16_t> tileProps;  // WorldTileProp
    std::vector<uint8_t> minimap;  // 4 bits per pixel, as the sprites have them
    std::vector<WorldMod> mods;
    std::vector<uint8_t> shortEntrances;  // x, y, map word, x and y of where it leads
    std::vector<uint8_t> eventTriggers;  // x, y, offset of the event in the event script
};

struct WorldData {
    std::array<WorldMapData, FF6_WORLD_COUNT> worlds;
    std::vector<uint8_t> miscSprites;  // 4 bits per pixel: the position mark of the mini-map and the other sprites
    std::vector<uint8_t> battleGroups;  // WorldBattleGroup
    std::vector<uint8_t> battleRates;  // WorldBattleRate
    std::vector<uint16_t> animPointers;  // where the sprites of each animation frame start in animData
    std::vector<uint8_t> animData;  // for each frame: the number of sprites, then x, y, tile and attributes of each
    std::vector<uint8_t> animGfx;  // 4 bits per pixel: the sprites that move by themselves
    std::array<std::vector<uint8_t>, FF6_WORLD_COUNT> airshipGfx;  // the airship, the falcon
    std::vector<uint8_t> backdropGfx, backdropTiles;  // the sky of the flights
};

WorldData loadWorldData(const std::string& packDir);

}  // namespace ff6
