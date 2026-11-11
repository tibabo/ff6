#include "cutscene/ppu.hpp"

#include <algorithm>

#include "data/pack.hpp"

namespace ff6 {
namespace {

constexpr int kWidth = FF6_SCREEN_WIDTH, kHeight = FF6_SCREEN_HEIGHT;
constexpr int kVramMask = 0x7FFF;
constexpr int kSpriteSizes[8][2] = {{8, 16}, {8, 32}, {8, 64}, {16, 32}, {16, 64}, {32, 64}, {16, 32}, {16, 32}};

enum Source { kBg1, kBg2, kBg3, kObj, kBackdrop };

// What one layer shows at one pixel of the line.
struct Px {
    uint16_t color = 0;
    bool on = false;
    uint8_t priority = 0;
    bool math = true;  // objects with a palette below 4 are not touched by the color math
};

using Row = std::array<Px, kWidth>;

struct Entry {
    int source, priority;
};

// From front to back; the table of mode 7 puts the layer between the objects of priority 1 and 0.
constexpr Entry kMode1Bg3Front[] = {{kBg3, 1}, {kObj, 3}, {kBg1, 1}, {kBg2, 1}, {kObj, 2},
                                    {kBg1, 0}, {kBg2, 0}, {kObj, 1}, {kObj, 0}, {kBg3, 0}};
constexpr Entry kMode1[] = {{kObj, 3}, {kBg1, 1}, {kBg2, 1}, {kObj, 2}, {kBg1, 0},
                            {kBg2, 0}, {kObj, 1}, {kObj, 0}, {kBg3, 1}, {kBg3, 0}};
constexpr Entry kMode2[] = {{kBg1, 1}, {kObj, 3}, {kBg2, 1}, {kObj, 2}, {kBg1, 0}, {kObj, 1}, {kBg2, 0}, {kObj, 0}};
constexpr Entry kMode7[] = {{kObj, 3}, {kObj, 2}, {kObj, 1}, {kBg1, 0}, {kObj, 0}};

// The color of the pixel (x, y) of a 4 or 2 bits per pixel character, 0 when it is transparent.
int characterPixel(const std::vector<uint16_t>& vram, int base, int fineX, int fineY, bool fourBits) {
    const int bit = 7 - fineX;
    const uint16_t low = vram[static_cast<size_t>((base + fineY) & kVramMask)];
    int color = (low >> bit & 1) | ((low >> (8 + bit) & 1) << 1);
    if (fourBits) {
        const uint16_t high = vram[static_cast<size_t>((base + 8 + fineY) & kVramMask)];
        color |= ((high >> bit & 1) << 2) | ((high >> (8 + bit) & 1) << 3);
    }
    return color;
}

void drawBg(const Ppu& ppu, int bg, int y, bool fourBits, Row& out) {
    const PpuLine& line = ppu.lines[static_cast<size_t>(y)];
    const size_t index = static_cast<size_t>(bg);
    const int sc = ppu.bgsc[index];
    const int mapBase = (sc & 0xFC) << 8;
    const bool wide = (sc & 1) != 0, tall = (sc & 2) != 0;
    const int nibble = bg == kBg1 ? ppu.nba12 & 15 : bg == kBg2 ? ppu.nba12 >> 4 : ppu.nba34 & 15;
    const int chr = nibble * 0x1000, words = fourBits ? 16 : 8;
    const bool columns = bg == kBg2 && line.bg2Columns;
    int py = (y + line.vofs[index]) & (tall ? 511 : 255);
    for (int x = 0; x < kWidth; ++x) {
        const int px = (x + line.hofs[index]) & (wide ? 511 : 255);
        if (columns) {
            const int column = std::min(32, (x + (line.hofs[index] & 7)) >> 3);
            py = (y + (column == 0 ? line.vofs[index] : line.bg2ColumnVofs[static_cast<size_t>(column)])) & (tall ? 511 : 255);
        }
        const int tx = px >> 3, ty = py >> 3;
        int screen = 0;
        if (wide && tx >= 32) screen += 1;
        if (tall && ty >= 32) screen += wide ? 2 : 1;
        const uint16_t entry = ppu.vram[static_cast<size_t>((mapBase + screen * 0x400 + (ty & 31) * 32 + (tx & 31)) & kVramMask)];
        int fineX = px & 7, fineY = py & 7;
        if (entry & 0x4000) fineX = 7 - fineX;
        if (entry & 0x8000) fineY = 7 - fineY;
        const int color = characterPixel(ppu.vram, chr + (entry & 0x3FF) * words, fineX, fineY, fourBits);
        Px& pixel = out[static_cast<size_t>(x)];
        pixel.on = color != 0;
        if (!pixel.on) continue;
        const int palette = entry >> 10 & 7;
        pixel.color = ppu.cgram[static_cast<size_t>((fourBits ? palette * 16 : palette * 4) + color)];
        pixel.priority = static_cast<uint8_t>(entry >> 13 & 1);
    }
}

// Mode 7: the matrix of the line sends each pixel of the screen to a pixel of the 1024 x 1024 plane.
void drawMode7(const Ppu& ppu, int y, Row& out) {
    const PpuLine& line = ppu.lines[static_cast<size_t>(y)];
    const auto signed13 = [](int value) { return (value & 0x1000) ? (value & 0x1FFF) - 0x2000 : value & 0x1FFF; };
    const auto clip = [](int value) { return (value & 0x2000) ? (value | ~1023) : (value & 1023); };
    const int a = line.m7a, b = line.m7b, c = line.m7c, d = line.m7d;
    const int centerX = signed13(line.m7Own ? line.m7x : ppu.m7x), centerY = signed13(line.m7Own ? line.m7y : ppu.m7y);
    const int offsetX = clip(signed13(line.m7Own ? line.m7hofs : ppu.m7hofs) - centerX);
    const int offsetY = clip(signed13(line.m7Own ? line.m7vofs : ppu.m7vofs) - centerY);
    const int originX = ((a * offsetX) & ~63) + ((b * offsetY) & ~63) + ((b * y) & ~63) + (centerX << 8);
    const int originY = ((c * offsetX) & ~63) + ((d * offsetY) & ~63) + ((d * y) & ~63) + (centerY << 8);
    for (int x = 0; x < kWidth; ++x) {
        const int rawX = (originX + a * x) >> 8, rawY = (originY + c * x) >> 8;
        const int px = rawX & 1023, py = rawY & 1023;
        const bool outside = rawX < 0 || rawX > 1023 || rawY < 0 || rawY > 1023;
        Px& pixel = out[static_cast<size_t>(x)];
        if (outside && (ppu.m7sel >> 6) == 2) {
            pixel.on = false;
            continue;
        }
        const int tile = outside && (ppu.m7sel >> 6) == 3 ? 0 : ppu.vram[static_cast<size_t>(((py >> 3) * 128 + (px >> 3)) & kVramMask)] & 0xFF;
        const int color = ppu.vram[static_cast<size_t>((tile * 64 + (py & 7) * 8 + (px & 7)) & kVramMask)] >> 8;
        pixel.on = color != 0;
        if (pixel.on) pixel.color = ppu.cgram[static_cast<size_t>(color)];
        pixel.priority = 0;
    }
}

void drawSprites(const Ppu& ppu, int y, Row& out) {
    out.fill(Px());
    const int nameBase = (ppu.objsel & 7) * 0x2000;
    const int nameSecond = nameBase + (((ppu.objsel >> 3) & 3) + 1) * 0x1000;
    const int* sizes = kSpriteSizes[ppu.objsel >> 5 & 7];
    for (size_t i = ppu.sprites.size(); i-- > 0;) {  // the first sprite is drawn last, so it is in front
        const PpuSprite& sprite = ppu.sprites[i];
        const int size = sprite.large ? sizes[1] : sizes[0];
        const int row = (y - sprite.y) & 0xFF;
        if (row >= size) continue;
        const int sourceRow = (sprite.attr & 0x80) ? size - 1 - row : row;
        const int palette = sprite.attr >> 1 & 7;
        for (int column = 0; column < size; ++column) {
            const int x = sprite.x + column;
            if (x < 0 || x >= kWidth) continue;
            const int sourceColumn = (sprite.attr & 0x40) ? size - 1 - column : column;
            const int low = sprite.tile & 0xFF;
            const int tile = (((low & 0x0F) + (sourceColumn >> 3)) & 0x0F) | ((((low >> 4) + (sourceRow >> 3)) & 0x0F) << 4);
            const int base = (sprite.tile & 0x100 ? nameSecond : nameBase) + tile * 16;
            const int color = characterPixel(ppu.vram, base, sourceColumn & 7, sourceRow & 7, true);
            if (color == 0) continue;
            Px& pixel = out[static_cast<size_t>(x)];
            pixel.on = true;
            pixel.color = ppu.cgram[static_cast<size_t>(128 + palette * 16 + color)];
            pixel.priority = static_cast<uint8_t>(sprite.attr >> 4 & 3);
            pixel.math = palette >= 4;
        }
    }
}

struct Pick {
    uint16_t color = 0;
    int source = kBackdrop;
    bool math = true;
};

// The front pixel of the layers in `mask` (bit n for bg n + 1, bit 4 for the objects).
Pick frontPixel(int mask, int x, int mode, bool bg3Front, const std::array<Row, 3>& bg, const Row& objects, uint16_t backdrop) {
    const Entry* begin = mode == 7 ? kMode7 : mode == 2 ? kMode2 : bg3Front ? kMode1Bg3Front : kMode1;
    const Entry* end = mode == 7   ? kMode7 + sizeof(kMode7) / sizeof(kMode7[0])
                       : mode == 2 ? kMode2 + sizeof(kMode2) / sizeof(kMode2[0])
                                   : begin + (bg3Front ? sizeof(kMode1Bg3Front) / sizeof(kMode1Bg3Front[0]) : sizeof(kMode1) / sizeof(kMode1[0]));
    for (const Entry* entry = begin; entry != end; ++entry) {
        const int bit = entry->source == kObj ? 0x10 : 1 << entry->source;
        if (!(mask & bit)) continue;
        const Px& pixel = entry->source == kObj ? objects[static_cast<size_t>(x)] : bg[static_cast<size_t>(entry->source)][static_cast<size_t>(x)];
        if (pixel.on && pixel.priority == entry->priority) return {pixel.color, entry->source, pixel.math};
    }
    return {backdrop, kBackdrop, true};
}

int channel(uint16_t color, int shift) { return color >> shift & 31; }

// Whether the pixel x is in the area that the two windows of `select` (bit 0 and 2 invert, 1 and 3 turn on) make.
bool insideWindows(const PpuLine& line, int select, int x) {
    bool inside = false;
    for (int window = 0; window < 2; ++window) {
        if (!(select & (2 << window * 2))) continue;
        const bool within = x >= line.window[static_cast<size_t>(window) * 2] && x <= line.window[static_cast<size_t>(window) * 2 + 1];
        inside = inside || (within != ((select & (1 << window * 2)) != 0));
    }
    return inside;
}

// Takes away the pixels that the windows hide on the main screen.
void applyWindows(const PpuLine& line, std::array<Row, 3>& bg, Row& objects, int tm) {
    const auto hide = [&](int select, int layerBit, Row& row) {
        if (!(line.tmw & layerBit) || !(tm & layerBit) || !(select & 0xA)) return;
        for (int x = 0; x < kWidth; ++x) {
            if (insideWindows(line, select, x)) row[static_cast<size_t>(x)].on = false;
        }
    };
    hide(line.w12sel & 15, 1, bg[0]);
    hide(line.w12sel >> 4, 2, bg[1]);
    hide(line.wobjsel & 15, 0x10, objects);
}

}  // namespace

Ppu::Ppu() : vram(kVramWords, 0) {
    lines.fill(PpuLine());
}

void Ppu::clearVram() { std::fill(vram.begin(), vram.end(), 0); }

void Ppu::render(uint8_t* rgba) const {
    std::array<Row, 3> bg;
    Row objects;
    for (int y = 0; y < kHeight; ++y) {
        uint8_t* out = rgba + static_cast<size_t>(y) * kWidth * FF6_BYTES_PER_PIXEL;
        if (forceBlank) {
            for (int x = 0; x < kWidth; ++x) {
                const uint8_t black[FF6_BYTES_PER_PIXEL] = {0, 0, 0, 255};
                std::copy(black, black + FF6_BYTES_PER_PIXEL, out + x * FF6_BYTES_PER_PIXEL);
            }
            continue;
        }
        const PpuLine& line = lines[static_cast<size_t>(y)];
        const int mode = line.bgMode & 7;
        const bool bg3Front = (line.bgMode & 8) != 0;
        for (Row& row : bg) row.fill(Px());
        if (mode == 7) {
            if ((tm | ts) & 1) drawMode7(*this, y, bg[0]);
        } else {
            if ((tm | ts) & 1) drawBg(*this, kBg1, y, true, bg[0]);
            if ((tm | ts) & 2) drawBg(*this, kBg2, y, true, bg[1]);
            if ((tm | ts) & 4) drawBg(*this, kBg3, y, false, bg[2]);
        }
        drawSprites(*this, y, objects);
        applyWindows(line, bg, objects, tm);
        const bool subScreen = (line.cgwsel & 2) != 0;
        const uint16_t fixed = static_cast<uint16_t>(line.fixed[0] | line.fixed[1] << 5 | line.fixed[2] << 10);
        for (int x = 0; x < kWidth; ++x) {
            Pick main = frontPixel(tm, x, mode, bg3Front, bg, objects, cgram[0]);
            const int enabled = main.source == kBackdrop ? 0x20 : main.source == kObj ? (main.math ? 0x10 : 0) : 1 << main.source;
            uint16_t color = main.color;
            // The color window blacks out the main screen (bits 6 and 7 of CGWSEL: never, outside, inside, always) before the math.
            const int clip = line.cgwsel >> 6 & 3;
            if (clip == 3 || (clip != 0 && insideWindows(line, line.wobjsel >> 4, x) == (clip == 2))) color = 0;
            if (line.cgadsub & enabled) {
                bool subIsBackdrop = !subScreen;
                uint16_t other = fixed;
                if (subScreen) {
                    const Pick sub = frontPixel(ts, x, mode, bg3Front, bg, objects, fixed);
                    other = sub.color;
                    subIsBackdrop = sub.source == kBackdrop;
                }
                const bool subtract = (line.cgadsub & 0x80) != 0;
                const bool half = (line.cgadsub & 0x40) != 0 && !(subScreen && subIsBackdrop);
                uint16_t result = 0;
                for (int shift = 0; shift <= 10; shift += 5) {
                    int value = subtract ? channel(color, shift) - channel(other, shift) : channel(color, shift) + channel(other, shift);
                    if (half) value >>= 1;
                    result = static_cast<uint16_t>(result | std::max(0, std::min(31, value)) << shift);
                }
                color = result;
            }
            uint8_t* pixel = out + x * FF6_BYTES_PER_PIXEL;
            colorToRgba(color, pixel);
            for (int c = 0; c < FF6_ALPHA_OFFSET; ++c) pixel[c] = static_cast<uint8_t>(pixel[c] * brightness / 15);
            pixel[FF6_ALPHA_OFFSET] = 255;
        }
    }
}

}  // namespace ff6
