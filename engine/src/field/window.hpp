#pragma once

#include <cstdint>
#include <vector>

namespace ff6 {

// Writes a pixel into a screen image, unless it is out of the screen or transparent.
void blendPixel(std::vector<uint8_t>& image, int x, int y, const uint8_t* rgba);

// The tile of the window graphics at a place of a window of cols x rows tiles: corners, edges and the 4x4 fill.
int windowTile(int col, int row, int cols, int rows);

// Draws a window of cols x rows tiles, borders included, with its upper left corner at a pixel of the screen.
void drawWindow(std::vector<uint8_t>& image, const std::vector<uint8_t>& windowTiles, int x, int y, int cols, int rows);

}  // namespace ff6
