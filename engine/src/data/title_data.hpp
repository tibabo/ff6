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
    std::vector<uint8_t> theEndGfx, theEndStars, theEndPal;  // the screen of THE END
    std::vector<uint8_t> credits;
    // The ground of the flights of the credits: graphics and maps, the palette and boxes tables, six palettes, the scripts of the camera,
    // and the sine in 16 bits.
    std::vector<uint8_t> flightGfx, flightTables, flightPalettes, flightScrolls, sine16, skyGfx, skyMap, skyPal;
    std::vector<uint8_t> endingGfx1;  // the backgrounds of the scenes of the ending
    std::vector<uint8_t> endingAnim2, endingGfx2, endingGfx3, endingGfx4, endingGfx5, endingCoin, endingPhase, endingAirshipPal, falconFrames, creditsSmallFont;  // the animations of CF/F450 on, and the graphics of the sprites of the scenes of the characters
    std::vector<uint8_t> endingAnim, endingFont;  // the sprites of the big names (C2/8A70 on) and the font that they use
    std::vector<uint8_t> ruinGfx, floatingGfx;  // the graphics of the cutscenes of the Ruin and of the floating continent

    // The byte, the word or the bytes at the address that the program has in the game ($7e5000 and up).
    uint8_t byteAt(uint32_t address) const;
    uint16_t wordAt(uint32_t address) const;
    const uint8_t* at(uint32_t address) const;
};

TitleData loadTitleData(const std::string& packDir);

}  // namespace ff6
