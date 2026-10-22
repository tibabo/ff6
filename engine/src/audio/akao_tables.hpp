#pragma once

#include <cstdint>

#include "constants.hpp"

// Constant tables of the sound program (src/spc/spc_main.asm).
namespace ff6 {
namespace akao {

// Pitch multiplier of each note of the octave: 0x1000 * 2 ^ ((note - 11) / 12).
inline constexpr uint16_t kFreqMult[12] = {0x0879, 0x08fa, 0x0983, 0x0a14, 0x0aad, 0x0b50,
                                           0x0bfc, 0x0cb2, 0x0d74, 0x0e41, 0x0f1a, 0x1000};

// Duration in ticks of each note length: whole, half, third, dotted quarter, quarter, ...
inline constexpr uint8_t kNoteDuration[FF6_AKAO_NOTE_DIVISOR] = {0xc0, 0x60, 0x40, 0x48, 0x30, 0x20, 0x24,
                                                                 0x18, 0x10, 0x0c, 0x08, 0x06, 0x04, 0x03};

// Echo filters that the script can select.
inline constexpr uint8_t kFilter[4][FF6_AKAO_FIR_TAPS] = {{0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                                                          {0x0c, 0x21, 0x2b, 0x2b, 0x13, 0xfe, 0xf3, 0xf9},
                                                          {0x58, 0xbf, 0xdb, 0xf0, 0xfe, 0x07, 0x0c, 0x0c},
                                                          {0x34, 0x33, 0x00, 0xd9, 0xe5, 0x01, 0xfc, 0xeb}};

// Bytes that follow each command, from FF6_AKAO_FIRST_COMMAND to 0xFF.
inline constexpr uint8_t kCommandBytes[60] = {
    1, 2, 1, 2, 2, 3, 0, 3, 0, 2, 0, 1,  // c4-cf
    0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1,  // d0-df
    1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,  // e0-ef
    1, 2, 1, 2, 1, 3, 2, 2, 2, 0, 0, 0, 2, 0, 0, 0};  // f0-ff

// The change per tick that takes `current` to `target` in `ticks` ticks (CalcEnvRate).
inline uint16_t envelopeRate(int target, int current, int ticks) {
    const int difference = target - current;
    const int magnitude = (difference < 0 ? -difference : difference) * 256 / ticks;
    return static_cast<uint16_t>(difference < 0 ? -magnitude : magnitude);
}

}  // namespace akao
}  // namespace ff6
