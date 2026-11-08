#include <vector>

#include "../check.hpp"
#include "../tests.hpp"
#include "cutscene/ppu.hpp"

namespace {

std::vector<uint8_t> screen(const ff6::Ppu& ppu) {
    std::vector<uint8_t> pixels(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
    ppu.render(pixels.data());
    return pixels;
}

const uint8_t* at(const std::vector<uint8_t>& pixels, int x, int y) {
    return &pixels[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL];
}

// A screen that is lit, with the first color of the palette black.
ff6::Ppu litPpu() {
    ff6::Ppu ppu;
    ppu.forceBlank = false;
    ppu.brightness = 15;
    return ppu;
}

// The first row of a character of 4 bits per pixel, which starts at the word `base`: all of its pixels have `color`.
void fillRow(ff6::Ppu& ppu, int base, int color) {
    ppu.vram[static_cast<size_t>(base)] = static_cast<uint16_t>((color & 1 ? 0x00FF : 0) | (color & 2 ? 0xFF00 : 0));
    ppu.vram[static_cast<size_t>(base + 8)] = static_cast<uint16_t>((color & 4 ? 0x00FF : 0) | (color & 8 ? 0xFF00 : 0));
}

}  // namespace

void testPpu() {
    {  // a screen under force blank, and the brightness
        ff6::Ppu ppu;
        ppu.cgram[0] = 0x7FFF;
        CHECK(at(screen(ppu), 5, 5)[0] == 0);  // force blank is the state at the start
        ppu.forceBlank = false;
        ppu.brightness = 15;
        CHECK(at(screen(ppu), 5, 5)[0] > 240);
        ppu.brightness = 0;
        CHECK(at(screen(ppu), 5, 5)[0] == 0);
        ppu.brightness = 7;
        const int half = at(screen(ppu), 5, 5)[0];
        CHECK(half > 100 && half < 130);
    }
    {  // bg1 draws the tile of its map with the palette of the tile, and the scroll of the line moves it
        ff6::Ppu ppu = litPpu();
        ppu.tm = 1;
        ppu.bgsc[0] = 0x00;  // the map at 0, 32 x 32
        ppu.nba12 = 0x01;  // the characters at 0x1000
        fillRow(ppu, 0x1000 + 5 * 16, 3);  // the tile 5, color 3, on its first row only
        ppu.vram[0] = 5 | (2 << 10);  // the palette 2
        ppu.cgram[2 * 16 + 3] = 0x001F;  // red
        std::vector<uint8_t> pixels = screen(ppu);
        CHECK(at(pixels, 0, 0)[0] > 240 && at(pixels, 0, 0)[1] == 0 && at(pixels, 7, 0)[0] > 240);
        CHECK(at(pixels, 8, 0)[0] == 0 && at(pixels, 0, 1)[0] == 0);  // the next tile and the second row are empty
        ppu.lines[0].hofs[0] = 4;
        pixels = screen(ppu);
        CHECK(at(pixels, 3, 0)[0] > 240 && at(pixels, 4, 0)[0] == 0);
    }
    {  // flips, and the priority of a tile against a sprite
        ff6::Ppu ppu = litPpu();
        ppu.tm = 0x11;
        ppu.nba12 = 0x01;
        ppu.objsel = 0x00;  // the characters of the sprites at 0, 8 x 8 and 16 x 16
        ppu.cgram[1] = 0x03E0;  // green, for bg1
        ppu.cgram[128 + 1] = 0x7C00;  // blue, for the sprites
        ppu.vram[static_cast<size_t>(0x1000 + 16)] = 0x00FF;  // the tile 1 of bg1 has ink on its first row
        ppu.vram[0] = 1 | 0x8000;  // flipped vertically: the ink is on the last row
        ppu.vram[1] = 1 | 0x8000 | 0x2000;  // the same, with the high priority
        fillRow(ppu, 16, 1);  // the tile 1 of the sprites
        ppu.sprites.push_back({0, 0, 1, 0, false});
        ppu.sprites.push_back({8, 7, 1, 0x10, false});  // the priority 1
        const std::vector<uint8_t> pixels = screen(ppu);
        CHECK(at(pixels, 0, 7)[1] > 200 && at(pixels, 0, 7)[2] == 0);  // the tile of bg1, flipped
        CHECK(at(pixels, 0, 0)[2] > 200 && at(pixels, 0, 0)[1] == 0);  // the sprite, on the empty row of the tile
        CHECK(at(pixels, 8, 7)[1] > 200 && at(pixels, 8, 7)[2] == 0);  // the tile with the high priority hides the sprite
    }
    {  // sprites: the large size, the wrap of y, the first one in front, and the flip
        ff6::Ppu ppu = litPpu();
        ppu.tm = 0x10;
        ppu.cgram[128 + 1] = 0x001F;  // palette 0: red
        ppu.cgram[128 + 16 + 1] = 0x03E0;  // palette 1: green
        fillRow(ppu, 16 * 16, 1);  // the first row of the tile under the first one: the second row of tiles of a large sprite
        ppu.sprites.push_back({20, 250, 0, 0x02, true});  // 16 x 16: the second row of tiles is on the line 2, past the wrap
        ppu.sprites.push_back({20, 250, 0, 0x00, true});  // behind the first one
        const std::vector<uint8_t> pixels = screen(ppu);
        CHECK(at(pixels, 20, 2)[1] > 200 && at(pixels, 20, 2)[0] == 0);  // the first sprite, which is green, is in front
        CHECK(at(pixels, 27, 2)[1] > 200 && at(pixels, 28, 2)[1] == 0 && at(pixels, 19, 2)[1] == 0);
        CHECK(at(pixels, 20, 1)[1] == 0 && at(pixels, 20, 3)[1] == 0);
        ppu.sprites.clear();
        ppu.vram[0] = 0x0001;  // the pixel at the right of the first row of the tile 0
        ppu.vram[8] = 0;
        ppu.sprites.push_back({0, 0, 0, 0x40, false});  // flipped horizontally
        const std::vector<uint8_t> flipped = screen(ppu);
        CHECK(at(flipped, 0, 0)[0] > 200 && at(flipped, 7, 0)[0] == 0);
    }
    {  // mode 7: a matrix of 1 shows the plane as it is, and a matrix of one half shows it twice as large
        ff6::Ppu ppu = litPpu();
        ppu.tm = 1;
        for (ff6::PpuLine& line : ppu.lines) {
            line.bgMode = 7;
            line.m7a = line.m7d = 0x0100;
        }
        // The tile 3 has a pixel of color 5 at 2, 0, and the map puts the tile at 1, 0 of the plane: the pixel is at 10, 0.
        ppu.vram[1] = 3;
        ppu.vram[static_cast<size_t>(3 * 64 + 2)] = 5 << 8;
        ppu.cgram[5] = 0x001F;
        std::vector<uint8_t> pixels = screen(ppu);
        CHECK(at(pixels, 10, 0)[0] > 240 && at(pixels, 9, 0)[0] == 0 && at(pixels, 11, 0)[0] == 0);
        for (ff6::PpuLine& line : ppu.lines) line.m7a = line.m7d = 0x0080;
        pixels = screen(ppu);
        CHECK(at(pixels, 20, 0)[0] > 240 && at(pixels, 21, 0)[0] > 240 && at(pixels, 22, 0)[0] == 0 && at(pixels, 19, 0)[0] == 0);
        ppu.m7hofs = 8;  // the screen is moved by 8 pixels of the plane: the pixel is 8 / 2 = 4 pixels closer
        pixels = screen(ppu);
        CHECK(at(pixels, 12, 0)[0] > 240 && at(pixels, 13, 0)[0] > 240 && at(pixels, 14, 0)[0] == 0 && at(pixels, 11, 0)[0] == 0);
    }
    {  // color math: the fixed color is added, taken away and halved; a backdrop in the sub screen is not halved
        ff6::Ppu ppu = litPpu();
        ppu.tm = 1;
        ppu.nba12 = 0x01;
        ppu.vram[0] = 1;
        ppu.vram[static_cast<size_t>(0x1000 + 16)] = 0x00FF;
        ppu.cgram[1] = 0x0010;  // a dim red
        for (ff6::PpuLine& line : ppu.lines) {
            line.cgwsel = 0;
            line.cgadsub = 0x01;  // added to bg1
            line.fixed = {8, 0, 0};
        }
        std::vector<uint8_t> pixels = screen(ppu);
        CHECK(at(pixels, 0, 0)[0] > 180 && at(pixels, 0, 1)[0] == 0);  // 16 + 8 of 31; the backdrop is not changed
        for (ff6::PpuLine& line : ppu.lines) line.cgadsub = 0x81;
        pixels = screen(ppu);
        CHECK(at(pixels, 0, 0)[0] > 50 && at(pixels, 0, 0)[0] < 90);  // 16 - 8
        for (ff6::PpuLine& line : ppu.lines) line.cgadsub = 0x41;
        pixels = screen(ppu);
        CHECK(at(pixels, 0, 0)[0] > 90 && at(pixels, 0, 0)[0] < 120);  // (16 + 8) / 2
        ppu.ts = 0x04;  // the sub screen is bg3, which has nothing here: its backdrop is the fixed color, not halved
        for (ff6::PpuLine& line : ppu.lines) line.cgwsel = 2;
        pixels = screen(ppu);
        CHECK(at(pixels, 0, 0)[0] > 180);
    }
}
