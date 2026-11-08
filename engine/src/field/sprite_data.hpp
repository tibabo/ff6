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

// A sprite of the vehicle graphics, as the word of the object memory gives it (tile, palette, flip bit 0x4000): an RGBA image of
// FF6_VEHICLE_SPRITE_SIZE square pixels, painted with a sprite palette.
std::vector<uint8_t> buildVehicleSprite(const CommonData& common, int palette, uint16_t word);

// Copies the image of buildVehicleSprite to the screen at (x, y), skipping transparent pixels and anything outside the screen.
void drawVehicleSprite(uint8_t* screen, const std::vector<uint8_t>& sprite, int x, int y);

// A piece of what an object rides on or of the rider: the position is from the corner of the sprite of a character.
struct VehiclePiece {
    uint16_t tile = 0;  // word for buildVehicleSprite, 0 for the rider
    int x = 0, y = 0;
    int firstRow = 0, rows = 0;  // the rows of the frame of the rider that show
    bool top = false;  // drawn with the upper half of the object: over the layers or under them like it
};

struct VehicleLook {
    std::vector<VehiclePiece> pieces;  // from the front of the object to its back
    int riderFrame = 0;  // index into the sprite strip
};

// DrawChoco, DrawMagitek and DrawRaft: what the object looks like in a FF6_VEHICLE_* type of vehicle. The position is the one on
// the axis that it moves on, and `moving` is whether it does.
VehicleLook vehicleLook(int vehicle, int facing, int position, bool moving);

}  // namespace ff6
