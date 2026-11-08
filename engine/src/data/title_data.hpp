#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "constants.hpp"

namespace ff6 {

// What the title screen and the opening take from the ROM: the program of the cutscenes, with the palettes, sprite
// frames and texts that it holds, the graphics that it decompresses, the map graphics that it uses and two tables.
struct TitleData {
    std::vector<uint8_t> program;
    std::vector<uint8_t> gfx;
    std::array<std::vector<uint8_t>, FF6_TITLE_MAP_GFX_COUNT> mapGfx;
    std::vector<uint8_t> random;  // RNGTbl
    std::vector<uint8_t> sine;  // SineTbl8, signed bytes

    // The byte, the word or the bytes at the address that the program has in the game ($7e5000 and up).
    uint8_t byteAt(uint32_t address) const;
    uint16_t wordAt(uint32_t address) const;
    const uint8_t* at(uint32_t address) const;
};

TitleData loadTitleData(const std::string& packDir);

}  // namespace ff6
