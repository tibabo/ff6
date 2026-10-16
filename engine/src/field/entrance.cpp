#include "field/entrance.hpp"

namespace ff6 {
namespace {

void fillTarget(uint16_t mapWord, int destX, int destY, EntranceTarget* target) {
    target->map = mapWord & FF6_ENTRANCE_MAP_MASK;
    target->toParent = target->map == FF6_ENTRANCE_PARENT_MAP;
    target->setParent = (mapWord & FF6_ENTRANCE_SET_PARENT) != 0;
    target->facing = (mapWord >> FF6_ENTRANCE_FACING_SHIFT) & FF6_NPC_DIR_MASK;
    target->x = destX;
    target->y = destY;
}

}  // namespace

bool checkEntrances(const MapData& map, int tileX, int tileY, EntranceTarget* target) {
    const std::vector<uint8_t>& longs = map.longEntrances;
    for (size_t i = 0; i + FF6_LONG_ENTRANCE_SIZE <= longs.size(); i += FF6_LONG_ENTRANCE_SIZE) {
        const uint8_t* item = &longs[i];
        const int sourceX = item[0], sourceY = item[1];
        const bool vertical = (item[2] & FF6_LONG_ENTRANCE_VERTICAL) != 0;
        const int length = item[2] & ~FF6_LONG_ENTRANCE_VERTICAL;
        const bool hit = vertical ? tileX == sourceX && tileY >= sourceY && tileY <= sourceY + length
                                  : tileY == sourceY && tileX >= sourceX && tileX <= sourceX + length;
        if (hit) {
            fillTarget(static_cast<uint16_t>(item[3] | (item[4] << 8)), item[5], item[6], target);
            return true;
        }
    }
    const std::vector<uint8_t>& shorts = map.shortEntrances;
    for (size_t i = 0; i + FF6_SHORT_ENTRANCE_SIZE <= shorts.size(); i += FF6_SHORT_ENTRANCE_SIZE) {
        const uint8_t* item = &shorts[i];
        if (item[0] == tileX && item[1] == tileY) {
            fillTarget(static_cast<uint16_t>(item[2] | (item[3] << 8)), item[4], item[5], target);
            return true;
        }
    }
    return false;
}

}  // namespace ff6
