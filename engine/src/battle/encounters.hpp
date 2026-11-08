#pragma once

#include <cstdint>

#include "data/battle_data.hpp"

namespace ff6 {

// The random battles of the maps (CheckBattleSub): a counter grows at each step by the rate of the map, and a battle
// starts when a number of the game's table gets under its high byte.
class Encounters {
public:
    // One step to a tile of `map`: the battle of the group of the map (a word of the group, which may carry
    // FF6_BATTLE_RANDOM_FLAG), or -1 for none. `enabled` is the flag of the map; the charms that change the rate are not read.
    int step(const BattleData& data, int map, bool enabled);

private:
    int number(const BattleData& data);  // UpdateBattleRng
    int groupNumber(const BattleData& data);  // UpdateBattleGrpRng

    uint16_t counter_ = 0;
    uint8_t pointer_ = 0, increment_ = 0;
    uint8_t groupPointer_ = 0, groupIncrement_ = 0;
};

}  // namespace ff6
