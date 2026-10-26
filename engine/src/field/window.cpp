#include "field/window.hpp"

#include <algorithm>

#include "constants.hpp"

namespace ff6 {

void blendPixel(std::vector<uint8_t>& image, int x, int y, const uint8_t* rgba) {
    if (x < 0 || y < 0 || x >= FF6_SCREEN_WIDTH || y >= FF6_SCREEN_HEIGHT || rgba[FF6_ALPHA_OFFSET] == 0) return;
    std::copy(rgba, rgba + FF6_BYTES_PER_PIXEL,
              image.begin() + (static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL);
}

int windowTile(int col, int row, int cols, int rows) {
    const bool top = row == 0, bottom = row == rows - 1;
    const bool left = col == 0, right = col == cols - 1;
    const bool oddColumn = (col & 1) != 0, oddRow = (row & 1) != 0;
    if (top) {
        return left ? FF6_WINDOW_TILE_TOP_LEFT : right ? FF6_WINDOW_TILE_TOP_RIGHT
               : oddColumn ? FF6_WINDOW_TILE_TOP_ODD : FF6_WINDOW_TILE_TOP_EVEN;
    }
    if (bottom) {
        return left ? FF6_WINDOW_TILE_BOTTOM_LEFT : right ? FF6_WINDOW_TILE_BOTTOM_RIGHT
               : oddColumn ? FF6_WINDOW_TILE_BOTTOM_ODD : FF6_WINDOW_TILE_BOTTOM_EVEN;
    }
    if (left) return oddRow ? FF6_WINDOW_TILE_LEFT_ODD : FF6_WINDOW_TILE_LEFT_EVEN;
    if (right) return oddRow ? FF6_WINDOW_TILE_RIGHT_ODD : FF6_WINDOW_TILE_RIGHT_EVEN;
    return ((row - 1) % FF6_WINDOW_FILL_SIZE) * FF6_WINDOW_FILL_SIZE + (col - 1) % FF6_WINDOW_FILL_SIZE;
}

void drawWindow(std::vector<uint8_t>& image, const std::vector<uint8_t>& windowTiles, int x, int y, int cols, int rows) {
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const uint8_t* tile = &windowTiles[static_cast<size_t>(windowTile(col, row, cols, rows)) * FF6_WINDOW_TILE_BYTES];
            for (int ty = 0; ty < FF6_WINDOW_TILE_SIZE; ++ty) {
                for (int tx = 0; tx < FF6_WINDOW_TILE_SIZE; ++tx) {
                    blendPixel(image, x + col * FF6_WINDOW_TILE_SIZE + tx, y + row * FF6_WINDOW_TILE_SIZE + ty,
                               tile + (ty * FF6_WINDOW_TILE_SIZE + tx) * FF6_BYTES_PER_PIXEL);
                }
            }
        }
    }
}

}  // namespace ff6
