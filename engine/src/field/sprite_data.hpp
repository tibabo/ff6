#pragma once

#include <cstdint>
#include <vector>

#include "data/pack.hpp"

namespace ff6 {

// ObjStopTileTbl: the sprite frame of an object standing still, for a facing direction.
int stopFrame(int facing);

// ObjMoveTileTbl: the sprite frame while walking, from the pixel position along the walking axis.
int moveFrame(int facing, int position);

// All FF6_FRAME_COUNT frames of a map sprite side by side as one RGBA image, painted with a sprite palette.
std::vector<uint8_t> buildSpriteStrip(const CommonData& common, int gfx, int palette);

// Copies `height` rows of a frame from the strip to the screen at (x, y), skipping transparent pixels and
// anything outside the screen. The screen is RGBA, FF6_SCREEN_WIDTH x FF6_SCREEN_HEIGHT.
void drawSpriteRows(uint8_t* screen, const std::vector<uint8_t>& strip, int frame, int firstRow, int height, int x,
                    int y);

}  // namespace ff6
