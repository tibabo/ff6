#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "constants.hpp"

namespace ff6 {

// A sprite as the object attribute memory holds it: the position of its top left corner (y wraps at 256),
// the tile with the name table bit, and the attributes (bits 1-3 palette, 4-5 priority, 6 and 7 flips).
struct PpuSprite {
    int x = 0, y = 0;
    int tile = 0;
    uint8_t attr = 0;
    bool large = false;
};

// What the registers hold while one line is drawn; the HDMA tables of a cutscene change them from line to line.
struct PpuLine {
    uint8_t bgMode = 1;  // BGMODE: bits 0-2 the mode (1, 2 or 7), bit 3 gives bg3 the highest priority
    std::array<uint16_t, 3> hofs{}, vofs{};  // bg1 to bg3
    int16_t m7a = 0, m7b = 0, m7c = 0, m7d = 0;
    uint8_t cgwsel = 0, cgadsub = 0;  // color math: bit 1 of CGWSEL picks the sub screen, then CGADSUB as the console has it
    std::array<uint8_t, 3> fixed{};  // the fixed color, 5 bits for red, green and blue
    bool bg2Columns = false;  // offset per tile in mode 2: bg2 has its own vertical scroll in each column of 8 pixels
    std::array<uint8_t, 33> bg2ColumnVofs{};  // the column 0 is the one that the scroll of the register gives
};

// A software version of the picture processor, as far as the title and the opening need it: modes 1 and 7, the
// sprites, color math with the sub screen, and what changes from line to line. Everything is drawn from the video
// memory and the palette like the console does, so the cutscene code can fill them the way the game does.
class Ppu {
public:
    static constexpr size_t kVramWords = 0x8000;

    Ppu();
    void clearVram();
    // Draws the screen as FF6_SCREEN_WIDTH x FF6_SCREEN_HEIGHT RGBA pixels.
    void render(uint8_t* rgba) const;

    std::vector<uint16_t> vram;
    std::array<uint16_t, 256> cgram{};
    std::vector<PpuSprite> sprites;  // in the order of the object memory: the first one is in front
    std::array<PpuLine, FF6_SCREEN_HEIGHT> lines;
    std::array<uint8_t, 3> bgsc{};  // BG1SC to BG3SC
    uint8_t nba12 = 0, nba34 = 0;  // where the characters of the bg layers are, in units of 0x1000 words
    uint8_t objsel = 0;
    uint8_t tm = 0, ts = 0;  // the layers of the main screen and of the sub screen: bg1 to bg4 in bits 0-3, objects in 4
    uint16_t m7x = 0, m7y = 0;  // the center of mode 7
    uint16_t m7hofs = 0, m7vofs = 0;
    uint8_t brightness = 0;  // 0 to 15
    bool forceBlank = true;
};

}  // namespace ff6
