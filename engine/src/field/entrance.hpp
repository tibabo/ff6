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

// Port of CheckLongEntrance and CheckShortEntrance: the entrance that the party standing on a tile triggers.
bool checkEntrances(const MapData& map, int tileX, int tileY, EntranceTarget* target);

}  // namespace ff6
