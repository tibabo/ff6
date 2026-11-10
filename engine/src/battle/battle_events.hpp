// The dialogs of the scripts of the battle events (btlgfx/battle_event_script.asm), in order. The animations of the scenes are not played:
// what the characters say is.
#pragma once

#include <cstdint>
#include <vector>

namespace ff6 {

inline const std::vector<uint8_t>& battleEventDialogs(int event) {
    static const std::vector<std::vector<uint8_t>> kDialogs = {
        {},  // 0 CHAR_ENTRY
        {},  // 1 VICTORY_FANFARE
        {},  // 2 BATTLE_EVENT_SCRIPT_2
        {},  // 3 BATTLE_EVENT_SCRIPT_3
        {},  // 4 TRITOCH_INTRO
        {},  // 5 WHELK_INTRO
        {184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198},  // 6 TERRA_MAGIC
        {253},  // 7 SABIN_INTRO
        {82, 83, 84, 85, 212, 213, 214, 215, 216, 86},  // 8 BLITZ_TUTORIAL
        {},  // 9 VARGAS_DEFEATED
        {},  // 10 ULTROS_RIVER_DEFEATED
        {251, 252},  // 11 SHADOW_LEAVES_PARTY
        {79, 80, 81},  // 12 CYAN_INTRO
        {58, 76, 59, 60, 61, 62, 63, 248, 64, 65, 66, 77, 249, 67, 68, 69, 70, 71, 71, 71, 71, 72, 73, 73, 73, 74, 75, 158, 159, 160, 161, 162, 163, 164, 78, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 183, 178, 179, 180, 181, 181, 182, 157},  // 13 GAU_INTRO
        {},  // 14 STEAL_GREEN_SOLDIER
        {},  // 15 STEAL_MERCHANT
        {200, 201, 202, 203, 204},  // 16 TUNNELARMR_INTRO
        {},  // 17 KEFKA_TERRA_INTRO
        {151, 152, 153, 154, 155, 156},  // 18 TRITOCH_TERRA_TRANSFORMS
        {95, 96, 97, 98, 99, 100, 101, 102},  // 19 KEFKA_SEALED_GATE_1
        {87, 89, 90, 91, 92, 93, 89, 94},  // 20 KEFKA_SEALED_GATE_2
        {103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 50, 115, 51, 116, 117},  // 21 AIRSHIP_GENJU
        {118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 240, 241, 242, 243},  // 22 RELM_ULTROS_INTRO
        {133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149},  // 23 KEFKA_KILLS_LEO
        {208, 209, 210, 211},  // 24 THAMASA_SEALED_GATE
        {212, 213, 214, 215, 216},  // 25 UNUSED_BLITZ_TUTORIAL
        {205, 206, 207},  // 26 KEFKA_GENJU_MAGICITE
        {254},  // 27 GAU_APPEARS
        {57},  // 28 GAU_RUNS_AWAY
        {},  // 29 SABIN_KEFKA_IMPERIAL_CAMP
        {217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239},  // 30 KEFKA_KILLS_GESTAHL
        {244},  // 31 WREXSOUL_INTRO
        {245, 246, 247},  // 32 FINAL_BATTLE_INTRO
    };
    static const std::vector<uint8_t> kNone;
    return event >= 0 && event < static_cast<int>(kDialogs.size()) ? kDialogs[static_cast<size_t>(event)] : kNone;
}

}  // namespace ff6
