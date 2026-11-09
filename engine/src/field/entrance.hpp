#pragma once

#include <cstdint>

#include "field/map.hpp"

namespace ff6 {

struct EntranceTarget {
    uint32_t map = 0;
    int x = 0, y = 0;
    int facing = Down;
    bool setParent = false;  // remember the current map as the parent of the sub-map
    bool toParent = false;  // go back to the parent map instead
};

// What the map word of an entrance, and the tile that it leads to, ask for.
EntranceTarget makeEntranceTarget(uint16_t mapWord, int destX, int destY);

// Port of CheckLongEntrance and CheckShortEntrance: the entrance that the party standing on a tile triggers.
bool checkEntrances(const MapData& map, int tileX, int tileY, EntranceTarget* target);

}  // namespace ff6
