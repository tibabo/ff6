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

}  // namespace ff6
