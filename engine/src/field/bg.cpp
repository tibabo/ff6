#include "field/bg.hpp"

#include "data/pack.hpp"

namespace ff6 {
namespace {

int wrap(int value, int size) { return ((value % size) + size) % size; }

// Palette index of pixel (x, y) of a 4bpp tile: bit planes 0-1 in the first 16 bytes, 2-3 in the next 16.
int pixel4bpp(const uint8_t* tile, int x, int y) {
    const int shift = 7 - x;
    return ((tile[y * 2] >> shift) & 1) | (((tile[y * 2 + 1] >> shift) & 1) << 1) |
           (((tile[FF6_TILE2_BYTES + y * 2] >> shift) & 1) << 2) |
           (((tile[FF6_TILE2_BYTES + y * 2 + 1] >> shift) & 1) << 3);
}

int pixel2bpp(const uint8_t* tile, int x, int y) {
    const int shift = 7 - x;
    return ((tile[y * 2] >> shift) & 1) | (((tile[y * 2 + 1] >> shift) & 1) << 1);
}

}  // namespace

void renderBg(const MapData& map, int layer, int scrollX, int scrollY, uint8_t* low, uint8_t* high) {
    const MapLayout& layout = map.layers[static_cast<size_t>(layer)];
    const int pixelWidth = layout.width * FF6_TILE_SIZE;
    const int pixelHeight = layout.height * FF6_TILE_SIZE;
    // Colors of the whole palette, converted once per call.
    uint8_t colors[FF6_MAP_PALETTE_COLORS][FF6_BYTES_PER_PIXEL];
    for (int i = 0; i < FF6_MAP_PALETTE_COLORS; ++i) colorToRgba(map.palette[static_cast<size_t>(i)], colors[i]);

    for (int screenY = 0; screenY < FF6_SCREEN_HEIGHT; ++screenY) {
        const int mapY = wrap(scrollY + screenY, pixelHeight);
        const int cellY = mapY / FF6_TILE_SIZE;
        for (int screenX = 0; screenX < FF6_SCREEN_WIDTH; ++screenX) {
            const int mapX = wrap(scrollX + screenX, pixelWidth);
            const int cellX = mapX / FF6_TILE_SIZE;
            const uint8_t cell = layout.cells[static_cast<size_t>(cellY) * layout.width + cellX];
            int inX = mapX % FF6_TILE_SIZE, inY = mapY % FF6_TILE_SIZE;
            const uint8_t* color = nullptr;
            bool front = false;
            if (layer == FF6_BG3) {
                // A bg3 cell is four consecutive 2bpp tiles; flipping the cell flips its four tiles too.
                if (cell & FF6_BG3_HFLIP) inX = FF6_TILE_SIZE - 1 - inX;
                if (cell & FF6_BG3_VFLIP) inY = FF6_TILE_SIZE - 1 - inY;
                const int tileIndex = cell & FF6_BG3_TILE_MASK;
                const int tile = tileIndex * 4 + (inY / FF6_SUBTILE_SIZE) * 2 + inX / FF6_SUBTILE_SIZE;
                const int index = pixel2bpp(&map.bg3Gfx[static_cast<size_t>(tile) * FF6_TILE2_BYTES],
                                            inX % FF6_SUBTILE_SIZE, inY % FF6_SUBTILE_SIZE);
                if (index == 0) continue;
                const int palette = (map.bg3Palette[static_cast<size_t>(tileIndex)] & FF6_BG3_PAL_MASK) >> FF6_BG3_PAL_SHIFT;
                color = colors[palette * FF6_BG3_COLORS + index];
                front = map.bg3Foreground();
            } else {
                const int quadrant = (inY / FF6_SUBTILE_SIZE) * 2 + inX / FF6_SUBTILE_SIZE;
                const uint16_t word = map.tilesets[static_cast<size_t>(layer)][static_cast<size_t>(quadrant) * FF6_TILESET_TILES + cell];
                int x = inX % FF6_SUBTILE_SIZE, y = inY % FF6_SUBTILE_SIZE;
                if (word & FF6_TILE_WORD_HFLIP) x = FF6_SUBTILE_SIZE - 1 - x;
                if (word & FF6_TILE_WORD_VFLIP) y = FF6_SUBTILE_SIZE - 1 - y;
                const int tile = word & FF6_TILE_WORD_INDEX_MASK;
                const int index = pixel4bpp(&map.vram[static_cast<size_t>(tile) * FF6_TILE4_BYTES], x, y);
                if (index == 0) continue;
                const int palette = (word >> FF6_TILE_WORD_PAL_SHIFT) & FF6_TILE_WORD_PAL_MASK;
                color = colors[palette * FF6_PALETTE_COLORS + index];
                front = (word & FF6_TILE_WORD_PRIORITY) != 0;
            }
            uint8_t* target = (front ? high : low) + (static_cast<size_t>(screenY) * FF6_SCREEN_WIDTH + screenX) * FF6_BYTES_PER_PIXEL;
            for (int c = 0; c < FF6_BYTES_PER_PIXEL; ++c) target[c] = color[c];
        }
    }
}

}  // namespace ff6
