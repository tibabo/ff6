#pragma once

#include <cstdint>

#include "data/battle_data.hpp"
#include "data/world_data.hpp"
#include "state/party_state.hpp"

namespace ff6 {

// The random battles of the maps (CheckBattleSub): a counter grows at each step by the rate of the map, and a battle
// starts when a number of the game's table gets under its high byte.
class Encounters {
public:
    // One step to a tile of `map`: the battle of the group of the map (a word of the group, which may carry
    // FF6_BATTLE_RANDOM_FLAG), or -1 for none. `enabled` is the flag of the map; the charms that change the rate are not read.
    int step(const BattleData& data, int map, bool enabled);

    // CheckBattleWorld, one step on a tile of a world map with random battles: the tile has a battle background number
    // (the high byte of its properties), and the tables are read at the sector of the map that the position is in. The
    // group of the table, as with step, or -1; `background` is the battle background of the tile.
    int stepWorld(const BattleData& data, const WorldData& world, int worldIndex, int tileBackground, int sectorX,
                  int sectorY, int* background, PartyState* party = nullptr);

    // Whether the last battle that stepWorld gave comes from the veldt, where Gau can leap.
    bool lastOnVeldt() const { return onVeldt_; }

private:
    // GetVeldtBattle: the next of the battles that the party has fought and that the veldt can bring back.
    int veldtBattle(const BattleData& data, PartyState* party);

    int number(const BattleData& data);  // UpdateBattleRng
    int groupNumber(const BattleData& data);  // UpdateBattleGrpRng

    bool onVeldt_ = false;
    uint16_t counter_ = 0;
    uint8_t pointer_ = 0, increment_ = 0;
    uint8_t groupPointer_ = 0, groupIncrement_ = 0;
};

}  // namespace ff6
