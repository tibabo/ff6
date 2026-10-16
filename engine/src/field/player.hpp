#pragma once

#include <cstdint>

#include "field/map.hpp"
#include "field/obj.hpp"

namespace ff6 {

// Port of the field movement rules (CheckPlayerMove and party priority updates).
class Player {
public:
    void place(const MapData& map, int tileX, int tileY, int facing);
    // direction: FF6_NO_DIRECTION for none. Returns true if a step started.
    bool update(const MapData& map, int direction, const Objects& objects);
    // The party is led by a script: the player stands where the lead object is.
    void follow(int pixelX, int pixelY, int facing);
    bool moving() const { return remaining_ > 0; }
    int pixelX() const { return pixelX_; }
    int pixelY() const { return pixelY_; }
    int tileX() const { return tileX_; }
    int tileY() const { return tileY_; }
    int facing() const { return facing_; }
    int zLevel() const { return zLevel_; }
    bool topAbove() const { return topAbove_; }
    bool bottomAbove() const { return bottomAbove_; }
    // Index into the sprite strip.
    int frameIndex() const;
    static bool canMove(const MapData& map, int fromX, int fromY, int direction, int zLevel);

private:
    bool tryStep(const MapData& map, int direction, const Objects& objects);
    void applyCurrentTilePriority(const MapData& map);
    int tileX_ = 0, tileY_ = 0, pixelX_ = 0, pixelY_ = 0;
    int facing_ = Down, direction_ = FF6_NO_DIRECTION, remaining_ = 0, zLevel_ = FF6_Z_LEVEL_1;
    bool topAbove_ = false, bottomAbove_ = false;
};

}  // namespace ff6
