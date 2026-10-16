#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "constants.hpp"

namespace ff6 {

enum Facing : int { Up = 0, Right = 1, Down = 2, Left = 3 };

// DirXTbl and DirYTbl: the step to the next tile for Up, Right, Down and Left.
inline constexpr int kDeltaX[FF6_DIRECTION_COUNT] = {0, 1, 0, -1};
inline constexpr int kDeltaY[FF6_DIRECTION_COUNT] = {-1, 0, 1, 0};

struct MapLayout {
    int width = 0;  // in map tiles
    int height = 0;
    std::vector<uint8_t> cells;
};

struct BgAnimTile {
    uint16_t speed = 0;
    std::array<std::array<uint8_t, FF6_BG_ANIM_TILE_BYTES>, FF6_BG_ANIM_FRAMES> frames{};
};

struct Bg3Anim {
    uint16_t speed = 0;
    uint16_t size = 0;
    std::array<uint16_t, FF6_BG3_ANIM_FRAMES> frames{};
    std::vector<uint8_t> data;
};

// One field map as map.asm sees it after LoadMapProp, LoadTileset, LoadMapTiles, LoadMapGfx and LoadMapPal.
struct MapData {
    uint32_t id = 0;
    std::array<uint8_t, FF6_MAP_PROP_SIZE> prop{};
    int startX = 0, startY = 0, startFacing = Down;
    std::array<uint8_t, FF6_TILE_PROPS_BYTES> tileProps{};
    std::array<MapLayout, FF6_LAYER_COUNT> layers;  // bg1, bg2, bg3
    std::array<std::vector<uint16_t>, 2> tilesets;  // bg1, then bg2 and bg3
    std::vector<uint8_t> vram;  // 4bpp tile graphics, the animated blocks at FF6_BG_ANIM_VRAM
    std::array<uint8_t, FF6_BG3_PALETTE_BYTES> bg3Palette{};
    std::vector<uint8_t> bg3Gfx;  // FF6_BG3_GFX_BYTES of 2bpp tiles
    std::array<uint16_t, FF6_MAP_PALETTE_COLORS> palette{};  // BGR555
    std::array<BgAnimTile, FF6_BG_ANIM_TILES> bgAnim;
    Bg3Anim bg3Anim;
    std::array<uint8_t, FF6_PARALLAX_BYTES> parallax{};
    std::array<uint8_t, FF6_COLOR_MATH_BYTES> colorMath{};
    std::vector<uint8_t> npcProps, eventTriggers, shortEntrances, longEntrances;  // raw ROM items

    bool bg3Foreground() const { return (prop[FF6_MAP_PROP_FLAGS] & FF6_BG3_FOREGROUND_FLAG) != 0; }
    bool bgAnimEnabled() const { return prop[FF6_MAP_PROP_ANIMATION] != 0; }
    int bg3AnimIndex() const { return prop[FF6_MAP_PROP_ANIMATION] >> FF6_BG3_ANIM_SHIFT; }
    int limitWidth() const { return prop[FF6_MAP_PROP_LIMIT_WIDTH]; }
    int limitHeight() const { return prop[FF6_MAP_PROP_LIMIT_HEIGHT]; }
    int layerOffsetX(int layer) const;  // in map tiles, bg2 and bg3 only
    int layerOffsetY(int layer) const;

    // Tile property word of a map cell: passability bits in the high byte, level flags in the low byte.
    uint16_t propAt(int x, int y) const;
};

MapData loadMap(const std::string& packDir, uint32_t mapId);
bool mapExists(const std::string& packDir, uint32_t mapId);

}  // namespace ff6
