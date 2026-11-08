#include "data/battle_gfx.hpp"

#include <algorithm>
#include <stdexcept>

#include "data/pack.hpp"

namespace ff6 {

void decodeTile(const uint8_t* tile, bool threePlanes, const uint8_t palette[][FF6_BYTES_PER_PIXEL], uint8_t* rgba,
    int stride, int x, int y) {
    for (int row = 0; row < FF6_MENU_TILE_SIZE; ++row) {
        const int plane0 = tile[row * 2], plane1 = tile[row * 2 + 1];
        const int plane2 = tile[FF6_TILE_PLANE_STRIDE + (threePlanes ? row : row * 2)];
        const int plane3 = threePlanes ? 0 : tile[FF6_TILE_PLANE_STRIDE + row * 2 + 1];
        for (int column = 0; column < FF6_MENU_TILE_SIZE; ++column) {
            const int shift = FF6_MENU_TILE_SIZE - 1 - column;
            const int color = ((plane0 >> shift) & 1) | (((plane1 >> shift) & 1) << 1) | (((plane2 >> shift) & 1) << 2) |
                              (((plane3 >> shift) & 1) << 3);
            if (color == 0) continue;
            uint8_t* pixel = rgba + (static_cast<size_t>(y + row) * stride + static_cast<size_t>(x + column)) * FF6_BYTES_PER_PIXEL;
            std::copy(palette[color], palette[color] + FF6_BYTES_PER_PIXEL, pixel);
        }
    }
}

const BattleImage& BattleGfx::monster(int id) const {
    const auto found = monsters_.find(id);
    if (found != monsters_.end()) return found->second;
    BattleImage& image = monsters_[id];
    if (id < 0 || id >= FF6_MONSTER_GFX_COUNT) return image;
    const uint8_t* prop = &monsterProp[static_cast<size_t>(id) * FF6_MONSTER_GFX_PROP_BYTES];
    const int graphics = prop[0] | (prop[1] << 8), palette = (prop[2] << 8) | prop[3], stencil = prop[4];
    const bool large = (palette & FF6_GFX_LARGE_FLAG) != 0, threePlanes = (graphics & FF6_GFX_3BPP_FLAG) != 0;
    const int cells = large ? FF6_STENCIL_LARGE_CELLS : FF6_STENCIL_SMALL_CELLS;
    const int stencils = large ? FF6_STENCIL_LARGE_COUNT : FF6_STENCIL_SMALL_COUNT;
    if (stencil >= stencils) return image;
    const uint8_t* rows = large ? &stencilLarge[static_cast<size_t>(stencil) * FF6_STENCIL_LARGE_BYTES]
                                : &stencilSmall[static_cast<size_t>(stencil) * FF6_STENCIL_SMALL_BYTES];
    uint8_t colors[16][FF6_BYTES_PER_PIXEL];
    const size_t first = static_cast<size_t>(palette & FF6_GFX_PALETTE_MASK) * FF6_MONSTER_PAL_STEP;
    for (size_t i = 0; i < 16; ++i) {
        const size_t at = first + i * 2;
        if (at + 1 >= monsterPal.size()) break;
        colorToRgba(static_cast<uint16_t>(monsterPal[at] | (monsterPal[at + 1] << 8)), colors[i]);
    }
    const int side = cells * FF6_MENU_TILE_SIZE;
    std::vector<uint8_t> grid(static_cast<size_t>(side) * side * FF6_BYTES_PER_PIXEL, 0);
    size_t at = static_cast<size_t>(graphics & FF6_GFX_ADDRESS_MASK) * FF6_GFX_ADDRESS_UNIT;
    const size_t tileBytes = threePlanes ? FF6_TILE_3BPP_BYTES : FF6_TILE_4BPP_BYTES;
    int columns = 0, lines = 0;
    for (int row = 0; row < cells; ++row) {
        const int bits = large ? (rows[row * 2] << 8) | rows[row * 2 + 1] : rows[row] << 8;
        for (int column = 0; column < cells; ++column) {
            if (!((bits >> (15 - column)) & 1)) continue;
            if (at + tileBytes > monsterGfx.size()) throw std::runtime_error("monster graphics run past their data");
            decodeTile(&monsterGfx[at], threePlanes, colors, grid.data(), side, column * FF6_MENU_TILE_SIZE,
                       row * FF6_MENU_TILE_SIZE);
            at += tileBytes;
            columns = std::max(columns, column + 1);
            lines = row + 1;
        }
    }
    image.width = columns * FF6_MENU_TILE_SIZE;
    image.height = lines * FF6_MENU_TILE_SIZE;
    image.rgba.resize(static_cast<size_t>(image.width) * image.height * FF6_BYTES_PER_PIXEL);
    for (int y = 0; y < image.height; ++y) {
        const uint8_t* source = &grid[static_cast<size_t>(y) * side * FF6_BYTES_PER_PIXEL];
        std::copy(source, source + static_cast<size_t>(image.width) * FF6_BYTES_PER_PIXEL,
                  image.rgba.begin() + static_cast<std::ptrdiff_t>(y) * image.width * FF6_BYTES_PER_PIXEL);
    }
    return image;
}

const BattleImage& BattleGfx::background(int index) const {
    const auto found = backgrounds_.find(index);
    if (found != backgrounds_.end()) return found->second;
    BattleImage& image = backgrounds_[index];
    if (index < 0 || index >= FF6_BG_COUNT) return image;
    const uint8_t* prop = &bgProp[static_cast<size_t>(index) * FF6_BG_PROP_BYTES];
    // The tiles of the three blocks of graphics, by their numbers in the tilemap.
    std::vector<const uint8_t*> tiles(FF6_BG_TILE_COUNT, nullptr);
    const auto load = [&](int block, int first, int trim) {
        if (block == FF6_BG_NONE || (block & FF6_BG_INDEX_MASK) >= FF6_BG_GFX_COUNT) return;
        const int size = (block & FF6_BG_LARGE_FLAG ? 2 * FF6_BG_BLOCK_BYTES : FF6_BG_BLOCK_BYTES) - trim;
        const uint8_t* data = &bgGfx[static_cast<size_t>(block & FF6_BG_INDEX_MASK) * FF6_BG_GFX_BYTES];
        for (int tile = 0; (tile + 1) * FF6_TILE_4BPP_BYTES <= size && first + tile < FF6_BG_TILE_COUNT; ++tile) {
            tiles[static_cast<size_t>(first + tile)] = data + tile * FF6_TILE_4BPP_BYTES;
        }
    };
    load(prop[0], 0, 0);
    if (!(prop[0] & FF6_BG_LARGE_FLAG)) load(prop[1], FF6_BG_SECOND_TILE, 0);
    load(prop[2], FF6_BG_THIRD_TILE, FF6_BG_THIRD_TRIM);
    if (prop[3] >= FF6_BG_TILES_COUNT) return image;
    const uint8_t* map = &bgTiles[static_cast<size_t>(prop[3]) * FF6_BG_TILES_BYTES];
    const uint8_t* palette = &bgPal[static_cast<size_t>(prop[5] & FF6_BG_INDEX_MASK) * FF6_BG_PAL_BYTES];
    uint8_t colors[FF6_BG_PALETTE_COUNT][FF6_BG_PALETTE_COLORS][FF6_BYTES_PER_PIXEL];
    for (int p = 0; p < FF6_BG_PALETTE_COUNT; ++p) {
        for (int c = 0; c < FF6_BG_PALETTE_COLORS; ++c) {
            const size_t at = static_cast<size_t>(p * FF6_BG_PALETTE_COLORS + c) * 2;
            colorToRgba(static_cast<uint16_t>(palette[at] | (palette[at + 1] << 8)), colors[p][c]);
        }
    }
    image.width = FF6_BG_COLUMNS * FF6_MENU_TILE_SIZE;
    image.height = FF6_BG_ROWS * FF6_MENU_TILE_SIZE;
    image.rgba.assign(static_cast<size_t>(image.width) * image.height * FF6_BYTES_PER_PIXEL, 0);
    for (size_t i = FF6_ALPHA_OFFSET; i < image.rgba.size(); i += FF6_BYTES_PER_PIXEL) image.rgba[i] = 255;
    for (int row = 0; row < FF6_BG_ROWS; ++row) {
        for (int column = 0; column < FF6_BG_COLUMNS; ++column) {
            const size_t at = (static_cast<size_t>(row) * FF6_BG_COLUMNS + column) * 2;
            const int entry = map[at] | (map[at + 1] << 8);
            const int p = ((entry >> FF6_BG_PALETTE_SHIFT) & 7) - FF6_BG_FIRST_PALETTE;
            const uint8_t* tile = tiles[static_cast<size_t>(entry & FF6_BG_TILE_MASK)];
            if (!tile || p < 0 || p >= FF6_BG_PALETTE_COUNT) continue;
            for (int y = 0; y < FF6_MENU_TILE_SIZE; ++y) {
                const int sourceY = entry & FF6_BG_VFLIP ? FF6_MENU_TILE_SIZE - 1 - y : y;
                for (int x = 0; x < FF6_MENU_TILE_SIZE; ++x) {
                    const int shift = entry & FF6_BG_HFLIP ? x : FF6_MENU_TILE_SIZE - 1 - x;
                    const int color = ((tile[sourceY * 2] >> shift) & 1) | (((tile[sourceY * 2 + 1] >> shift) & 1) << 1) |
                                      (((tile[FF6_TILE_PLANE_STRIDE + sourceY * 2] >> shift) & 1) << 2) |
                                      (((tile[FF6_TILE_PLANE_STRIDE + sourceY * 2 + 1] >> shift) & 1) << 3);
                    if (color == 0) continue;
                    uint8_t* pixel = &image.rgba[(static_cast<size_t>(row * FF6_MENU_TILE_SIZE + y) * image.width +
                                                 column * FF6_MENU_TILE_SIZE + x) * FF6_BYTES_PER_PIXEL];
                    std::copy(colors[p][color], colors[p][color] + FF6_BYTES_PER_PIXEL, pixel);
                }
            }
        }
    }
    return image;
}

bool BattleGfx::wavy(int index) const {
    return index >= 0 && index < FF6_BG_COUNT && (bgProp[static_cast<size_t>(index) * FF6_BG_PROP_BYTES + FF6_BG_PROP_PALETTE] & FF6_BG_WAVY_FLAG) != 0;
}

const BattleImage& BattleGfx::character(int actor, int pose) const {
    const int key = actor * FF6_CHAR_ACTION_COUNT + pose;
    const auto found = characters_.find(key);
    if (found != characters_.end()) return found->second;
    BattleImage& image = characters_[key];
    if (actor < 0 || actor >= FF6_CHAR_GFX_COUNT || pose < 0 || pose >= FF6_CHAR_ACTION_COUNT) return image;
    const uint8_t* sheet = &charGfx[static_cast<size_t>(actor) * FF6_CHAR_GFX_BYTES];
    const uint8_t* tiles = &charActions[static_cast<size_t>(pose) * FF6_CHAR_ACTION_TILES * 2];
    const uint8_t* palette = &charPal[static_cast<size_t>(charPalTable[static_cast<size_t>(actor)] % FF6_CHAR_PAL_COUNT) * FF6_CHAR_PAL_BYTES];
    uint8_t colors[16][FF6_BYTES_PER_PIXEL];
    for (size_t i = 0; i < 16; ++i) colorToRgba(static_cast<uint16_t>(palette[i * 2] | (palette[i * 2 + 1] << 8)), colors[i]);
    image.width = FF6_CHAR_WIDTH;
    image.height = FF6_CHAR_HEIGHT;
    image.rgba.assign(static_cast<size_t>(image.width) * image.height * FF6_BYTES_PER_PIXEL, 0);
    for (int tile = 0; tile < FF6_CHAR_TILES; ++tile) {
        const size_t at = static_cast<size_t>(FF6_CHAR_ACTION_FIRST_TILE + tile) * 2;
        const int offset = tiles[at] | (tiles[at + 1] << 8);
        if (offset == FF6_CHAR_TILE_NONE || offset + FF6_TILE_4BPP_BYTES > FF6_CHAR_GFX_BYTES) continue;
        decodeTile(sheet + offset, false, colors, image.rgba.data(), image.width, tile % 2 * FF6_MENU_TILE_SIZE,
                   tile / 2 * FF6_MENU_TILE_SIZE);
    }
    return image;
}

BattleGfx loadBattleGfx(const std::string& packDir) {
    const std::vector<uint8_t> file = readFile(packDir + "/" FF6_BATTLE_GFX_FILE);
    ByteReader reader(file);
    reader.expectMagic(FF6_BATTLE_GFX_MAGIC, "battle graphics");
    BattleGfx gfx;
    gfx.monsterProp = reader.bytes(static_cast<size_t>(FF6_MONSTER_GFX_COUNT) * FF6_MONSTER_GFX_PROP_BYTES);
    gfx.monsterPal = reader.bytes(FF6_MONSTER_PAL_BYTES);
    gfx.stencilSmall = reader.bytes(static_cast<size_t>(FF6_STENCIL_SMALL_COUNT) * FF6_STENCIL_SMALL_BYTES);
    gfx.stencilLarge = reader.bytes(static_cast<size_t>(FF6_STENCIL_LARGE_COUNT) * FF6_STENCIL_LARGE_BYTES);
    gfx.monsterGfx = reader.bytes(FF6_MONSTER_GFX_BYTES);
    gfx.bgProp = reader.bytes(static_cast<size_t>(FF6_BG_COUNT) * FF6_BG_PROP_BYTES);
    gfx.bgPal = reader.bytes(static_cast<size_t>(FF6_BG_COUNT) * FF6_BG_PAL_BYTES);
    gfx.bgGfx = reader.bytes(static_cast<size_t>(FF6_BG_GFX_COUNT) * FF6_BG_GFX_BYTES);
    gfx.bgTiles = reader.bytes(static_cast<size_t>(FF6_BG_TILES_COUNT) * FF6_BG_TILES_BYTES);
    gfx.charGfx = reader.bytes(static_cast<size_t>(FF6_CHAR_GFX_COUNT) * FF6_CHAR_GFX_BYTES);
    gfx.charActions = reader.bytes(static_cast<size_t>(FF6_CHAR_ACTION_COUNT) * FF6_CHAR_ACTION_TILES * 2);
    gfx.charPalTable = reader.bytes(FF6_CHAR_PAL_TABLE_BYTES);
    gfx.charPal = reader.bytes(static_cast<size_t>(FF6_CHAR_PAL_COUNT) * FF6_CHAR_PAL_BYTES);
    if (reader.remaining() != 0) throw std::runtime_error("battle graphics have trailing bytes");
    return gfx;
}

}  // namespace ff6
