#include "battle/encounters.hpp"

namespace ff6 {

int Encounters::number(const BattleData& data) {
    if (++pointer_ == 0) increment_ = static_cast<uint8_t>(increment_ + FF6_RNG_STEP_ENCOUNTER);
    return static_cast<uint8_t>(data.rngTable[pointer_] + increment_);
}

int Encounters::groupNumber(const BattleData& data) {
    if (++groupPointer_ == 0) groupIncrement_ = static_cast<uint8_t>(groupIncrement_ + FF6_RNG_STEP_GROUP);
    return static_cast<uint8_t>(data.rngTable[groupPointer_] + groupIncrement_);
}

int Encounters::veldtBattle(const BattleData& data, PartyState* party) {
    if (!party) return -1;
    const auto byteAt = [&](int index) {
        int value = 0;
        for (int bit = 0; bit < 8; ++bit) value |= party->veldtBattles[static_cast<size_t>(index * 8 + bit)] << bit;
        return value;
    };
    int index = (party->veldtPointer + 1) & (FF6_VELDT_BATTLE_COUNT / 8 - 1);
    for (int tries = 0; byteAt(index) == 0; ++tries) {
        if (tries == FF6_VELDT_BATTLE_COUNT / 8) return -1;  // none has been fought yet
        index = (index + 1) & (FF6_VELDT_BATTLE_COUNT / 8 - 1);
    }
    party->veldtPointer = index;
    int pick = groupNumber(data) & 7;
    while (!(byteAt(index) & (1 << pick))) pick = (pick + 1) & 7;
    return index * 8 + pick;
}

int Encounters::step(const BattleData& data, int map, bool enabled) {
    if (!enabled || map < FF6_FIRST_SUB_MAP) return -1;
    static constexpr int kRates[] = FF6_SUB_RATES;
    static constexpr int kPicks[] = FF6_ENCOUNTER_PICKS;
    const int sum = counter_ + kRates[data.subRate(map)];
    counter_ = static_cast<uint16_t>(sum > 0xFFFF ? FF6_ENCOUNTER_COUNTER_MAX : sum);
    if (number(data) >= (counter_ >> 8)) return -1;
    counter_ = 0;
    const int draw = groupNumber(data);
    int pick = 0;
    while (pick < 3 && draw >= kPicks[pick]) ++pick;
    return data.randBattle(data.subGroup(map), pick);
}

int Encounters::stepWorld(const BattleData& data, const WorldData& world, int worldIndex, int tileBackground, int sectorX,
                          int sectorY, int* background, PartyState* party) {
    static constexpr int kBackgrounds[] = FF6_WORLD_BATTLE_BACKGROUNDS;
    static constexpr int kRateShifts[] = FF6_WORLD_BACKGROUND_RATE_SHIFTS;
    static constexpr int kGroupColumns[] = FF6_WORLD_BACKGROUND_GROUPS;
    static constexpr int kRates[] = FF6_WORLD_RATES;
    static constexpr int kPicks[] = FF6_ENCOUNTER_PICKS;
    const int tile = tileBackground & 7, map = worldIndex & 1;
    *background = kBackgrounds[map * 8 + tile];
    const int sector = ((sectorY >> 5) & 7) * 8 + ((sectorX >> 5) & 7);  // 32 x 32 tiles each
    const size_t groupAt = static_cast<size_t>(map * 256 + sector * 4 + kGroupColumns[tile]);
    const size_t rateAt = static_cast<size_t>(map * 64 + sector);
    if (groupAt >= world.battleGroups.size() || rateAt >= world.battleRates.size()) return -1;
    const int rate = (world.battleRates[rateAt] >> (2 * kRateShifts[tile])) & 3;
    if (rate == FF6_WORLD_NO_BATTLE_RATE || kRates[rate] == 0) return -1;
    const int sum = counter_ + kRates[rate];
    counter_ = static_cast<uint16_t>(sum > 0xFFFF ? FF6_ENCOUNTER_COUNTER_MAX : sum);
    if (number(data) >= (counter_ >> 8)) return -1;
    counter_ = 0;
    const int group = world.battleGroups[groupAt];
    if (group == FF6_WORLD_VELDT_GROUP) return veldtBattle(data, party);
    const int draw = groupNumber(data);
    int pick = 0;
    while (pick < 3 && draw >= kPicks[pick]) ++pick;
    return data.randBattle(group, pick);
}

}  // namespace ff6
