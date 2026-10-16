#include "field/sprite_data.hpp"

namespace ff6 {
namespace {

// ObjMoveTileTbl and ObjStopTileTbl, expressed as indices into the sprite strip.
constexpr int kMoveFrames[FF6_DIRECTION_COUNT][FF6_WALK_POSE_COUNT] = {
    {4, 5, 4, 3}, {10, 11, 10, 9}, {1, 2, 1, 0}, {7, 8, 7, 6}};
constexpr int kStopFrames[FF6_DIRECTION_COUNT] = {4, 10, 1, 7};

}  // namespace

int stopFrame(int facing) { return kStopFrames[facing]; }

int moveFrame(int facing, int position) {
    return kMoveFrames[facing][(position >> FF6_WALK_POSE_SHIFT) & FF6_WALK_POSE_MASK];
}

std::vector<uint8_t> buildSpriteStrip(const CommonData& common, int gfx, int palette) {
    const int stripWidth = FF6_FRAME_WIDTH * FF6_FRAME_COUNT;
    std::vector<uint8_t> strip(static_cast<size_t>(stripWidth) * FF6_FRAME_HEIGHT * FF6_BYTES_PER_PIXEL, 0);
    if (gfx < 0 || static_cast<size_t>(gfx) >= common.spriteGfxCount() || palette < 0 ||
        static_cast<size_t>(palette) >= common.spritePaletteCount()) {
        return strip;
    }
    const uint8_t* sheet = &common.spriteGfx[static_cast<size_t>(gfx) * FF6_SPRITE_SHEET_BYTES];
    const uint16_t* colors = &common.spritePalettes[static_cast<size_t>(palette) * FF6_SPRITE_PAL_COLORS];
    for (int frame = 0; frame < FF6_FRAME_COUNT; ++frame) {
        for (int y = 0; y < FF6_FRAME_HEIGHT; ++y) {
            for (int x = 0; x < FF6_FRAME_WIDTH; ++x) {
                const int index = sheet[(frame * FF6_FRAME_HEIGHT + y) * FF6_FRAME_WIDTH + x];
                if (index == 0) continue;
                colorToRgba(colors[index], &strip[(static_cast<size_t>(y) * stripWidth + frame * FF6_FRAME_WIDTH + x) * FF6_BYTES_PER_PIXEL]);
            }
        }
    }
    return strip;
}

void drawSpriteRows(uint8_t* screen, const std::vector<uint8_t>& strip, int frame, int firstRow, int height, int x,
                    int y) {
    const int stripWidth = FF6_FRAME_WIDTH * FF6_FRAME_COUNT;
    for (int row = 0; row < height; ++row) {
        const int screenY = y + row;
        if (screenY < 0 || screenY >= FF6_SCREEN_HEIGHT) continue;
        for (int column = 0; column < FF6_FRAME_WIDTH; ++column) {
            const int screenX = x + column;
            if (screenX < 0 || screenX >= FF6_SCREEN_WIDTH) continue;
            const uint8_t* source =
                &strip[(static_cast<size_t>(firstRow + row) * stripWidth + frame * FF6_FRAME_WIDTH + column) * FF6_BYTES_PER_PIXEL];
            if (source[FF6_ALPHA_OFFSET] == 0) continue;
            uint8_t* target = screen + (static_cast<size_t>(screenY) * FF6_SCREEN_WIDTH + screenX) * FF6_BYTES_PER_PIXEL;
            for (int channel = 0; channel < FF6_BYTES_PER_PIXEL; ++channel) target[channel] = source[channel];
        }
    }
}

}  // namespace ff6
