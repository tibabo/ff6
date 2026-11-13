#include "field/sprite_data.hpp"

#include "field/map.hpp"

namespace ff6 {
namespace {

// ObjMoveTileTbl and ObjStopTileTbl, expressed as indices into the sprite strip.
constexpr int kMoveFrames[FF6_DIRECTION_COUNT][FF6_WALK_POSE_COUNT] = {
    {4, 5, 4, 3}, {10, 11, 10, 9}, {1, 2, 1, 0}, {7, 8, 7, 6}};
constexpr int kStopFrames[FF6_DIRECTION_COUNT] = {4, 10, 1, 7};

// The sprites that make the Magitek armor, each as the word of the object memory (tile, then the attributes with the flip bit 0x4000):
// top left, top right, bottom left and bottom right, for the facings up, right, down and left and four steps of each.
constexpr uint16_t kMagitekTiles[4][FF6_VEHICLE_MAGITEK_VARIANTS] = {
    {0x2FAC, 0x2FC0, 0x2FAC, 0x2FC4, 0x6FCA, 0x6FE2, 0x6FCA, 0x6FEA, 0x2FA0, 0x2FA4, 0x2FA0, 0x2FA8, 0x2FC8, 0x2FE0, 0x2FC8, 0x2FE8},
    {0x6FAC, 0x6FC4, 0x6FAC, 0x6FC0, 0x6FC8, 0x6FE0, 0x6FC8, 0x6FE8, 0x6FA0, 0x6FA8, 0x6FA0, 0x6FA4, 0x2FCA, 0x2FE2, 0x2FCA, 0x2FEA},
    {0x2FAE, 0x2FC2, 0x2FAE, 0x2FC6, 0x6FCE, 0x6FE6, 0x6FCE, 0x6FEE, 0x2FA2, 0x2FA6, 0x2FA2, 0x2FAA, 0x2FCC, 0x2FE4, 0x2FCC, 0x2FEC},
    {0x6FAE, 0x6FC6, 0x6FAE, 0x6FC2, 0x6FCC, 0x6FE4, 0x6FCC, 0x6FEC, 0x6FA2, 0x6FAA, 0x6FA2, 0x6FA6, 0x2FCE, 0x2FE6, 0x2FCE, 0x2FEE}};

// RaftTiles: top left, top right, bottom left and bottom right, for each facing.
constexpr uint16_t kRaftTiles[4][FF6_DIRECTION_COUNT] = {
    {0x2F20, 0x2F28, 0x2F20, 0x2F28}, {0x2F24, 0x2F2C, 0x2F24, 0x2F2C},
    {0x2F22, 0x2F2A, 0x2F22, 0x2F2A}, {0x2F26, 0x2F2E, 0x2F26, 0x2F2E}};

// The chocobo (DrawChocoUp and the others), by step. Up and down are 16 pixels wide, with a tail or a head that moves.
constexpr int kChocoUpTailX[FF6_WALK_POSE_COUNT] = {0, 1, 0, -1};
constexpr int kChocoUpTailY[FF6_WALK_POSE_COUNT] = {9, 10, 9, 10};
constexpr int kChocoUpRiderY[FF6_WALK_POSE_COUNT] = {0, 1, 0, 1};
constexpr uint16_t kChocoUpTail[FF6_WALK_POSE_COUNT] = {0x2F4A, 0x6F4A, 0x2F4A, 0x6F4A};
constexpr uint16_t kChocoUpBodyTop[FF6_WALK_POSE_COUNT] = {0x2F4C, 0x2F60, 0x2F4C, 0x6F60};
constexpr uint16_t kChocoUpBodyBottom[FF6_WALK_POSE_COUNT] = {0x2F4E, 0x2F62, 0x2F4E, 0x6F62};
constexpr int kChocoDownHeadX[FF6_WALK_POSE_COUNT] = {0, 1, 0, -1};
constexpr int kChocoDownHeadY[FF6_WALK_POSE_COUNT] = {7, 8, 7, 8};
constexpr int kChocoDownRiderY[FF6_WALK_POSE_COUNT] = {-1, 1, -1, 1};
constexpr uint16_t kChocoDownHead = 0x2F40;
constexpr uint16_t kChocoDownBodyTop[FF6_WALK_POSE_COUNT] = {0x2F42, 0x2F46, 0x2F42, 0x6F46};
constexpr uint16_t kChocoDownBodyBottom[FF6_WALK_POSE_COUNT] = {0x2F44, 0x2F48, 0x2F44, 0x6F48};
// Right and left are 32 pixels wide: the sprites that go at the front, then at the back, of the half on the right and of the half
// on the left (the ones facing right are the ones facing left with the flip bit).
constexpr int kChocoSideY[FF6_WALK_POSE_COUNT] = {0, -1, 0, -1};
constexpr uint16_t kChocoSideTiles[2][4][FF6_WALK_POSE_COUNT] = {
    {{0x6F64, 0x6F6C, 0x6F64, 0x6F84}, {0x6F66, 0x6F6E, 0x6F66, 0x6F86}, {0x6F6A, 0x6F82, 0x6F6A, 0x6F8A}, {0x6F68, 0x6F80, 0x6F68, 0x6F88}},
    {{0x2F68, 0x2F80, 0x2F68, 0x2F88}, {0x2F6A, 0x2F82, 0x2F6A, 0x2F8A}, {0x2F66, 0x2F6E, 0x2F66, 0x2F86}, {0x2F64, 0x2F6C, 0x2F64, 0x2F84}}};
constexpr int kObjectCharacters = 0x6000;  // the sprite characters in the video memory: the first table, then the second
constexpr int kObjectCharactersSecond = 0x7000;
constexpr int kTileWords = 16;
constexpr int kTilePixels = 8;

}  // namespace

// The frames of the sprite sheet, by the number that the game gives them (tools/field/obj.mjs SPRITE_FRAMES has the same list).
constexpr uint8_t kFrameIds[FF6_FRAME_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0x46, 0x47, 0x48, 0x2e, 0x2f, 0x6e, 0x6f,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x55, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x62, 0x63, 0x64, 0x65, 0x68, 0x6a, 0x72};

int actionFrame(int original) {
    for (int i = 0; i < FF6_FRAME_COUNT; ++i) {
        if (kFrameIds[i] == original) return i;
    }
    return kStopFrames[Down];
}

int stopFrame(int facing) { return kStopFrames[facing]; }

int moveFrame(int facing, int position) {
    return kMoveFrames[facing][(position >> FF6_WALK_POSE_SHIFT) & FF6_WALK_POSE_MASK];
}

std::vector<uint8_t> buildSpriteStrip(const CommonData& common, int gfx, int palette) {
    if (palette < 0 || static_cast<size_t>(palette) >= common.spritePaletteCount()) {
        return std::vector<uint8_t>(static_cast<size_t>(FF6_FRAME_WIDTH * FF6_FRAME_COUNT) * FF6_FRAME_HEIGHT * FF6_BYTES_PER_PIXEL, 0);
    }
    return buildSpriteStripColors(common, gfx, &common.spritePalettes[static_cast<size_t>(palette) * FF6_SPRITE_PAL_COLORS]);
}

std::vector<uint8_t> buildSpriteStripColors(const CommonData& common, int gfx, const uint16_t* colors) {
    const int stripWidth = FF6_FRAME_WIDTH * FF6_FRAME_COUNT;
    std::vector<uint8_t> strip(static_cast<size_t>(stripWidth) * FF6_FRAME_HEIGHT * FF6_BYTES_PER_PIXEL, 0);
    if (gfx < 0 || static_cast<size_t>(gfx) >= common.spriteGfxCount()) return strip;
    const uint8_t* sheet = &common.spriteGfx[static_cast<size_t>(gfx) * FF6_SPRITE_SHEET_BYTES];
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

int animatedFrame(int animFrame, int animType, int speed, uint32_t counter, bool& flip) {
    static constexpr int kBase[FF6_NPC_ANIM_FRAMES] = FF6_NPC_ANIM_FRAME_BASE;  // ObjSpecialTileTbl
    const uint32_t step = (counter >> 2) >> speed;
    int offset = 0;
    flip = false;
    switch (animType & FF6_NPC_DIR_MASK) {
        case FF6_NPC_ANIM_H_FLIP: flip = (step & 1) != 0; break;
        case FF6_NPC_ANIM_TWO_FRAMES: offset = static_cast<int>(step & 1); break;
        case FF6_NPC_ANIM_FOUR_FRAMES: offset = static_cast<int>(step & 3); break;
        default: break;
    }
    return actionFrame(kBase[animFrame & (FF6_NPC_ANIM_FRAMES - 1)] + offset);
}

void drawSpriteRows(uint8_t* screen, const std::vector<uint8_t>& strip, int frame, int firstRow, int height, int x,
                    int y, bool flip) {
    const int stripWidth = FF6_FRAME_WIDTH * FF6_FRAME_COUNT;
    for (int row = 0; row < height; ++row) {
        const int screenY = y + row;
        if (screenY < 0 || screenY >= FF6_SCREEN_HEIGHT) continue;
        for (int column = 0; column < FF6_FRAME_WIDTH; ++column) {
            const int screenX = x + column;
            if (screenX < 0 || screenX >= FF6_SCREEN_WIDTH) continue;
            const uint8_t* source =
                &strip[(static_cast<size_t>(firstRow + row) * stripWidth + frame * FF6_FRAME_WIDTH + (flip ? FF6_FRAME_WIDTH - 1 - column : column)) * FF6_BYTES_PER_PIXEL];
            if (source[FF6_ALPHA_OFFSET] == 0) continue;
            uint8_t* target = screen + (static_cast<size_t>(screenY) * FF6_SCREEN_WIDTH + screenX) * FF6_BYTES_PER_PIXEL;
            for (int channel = 0; channel < FF6_BYTES_PER_PIXEL; ++channel) target[channel] = source[channel];
        }
    }
}

std::vector<uint8_t> buildVehicleSprite(const CommonData& common, int palette, uint16_t word) {
    std::vector<uint8_t> sprite(static_cast<size_t>(FF6_VEHICLE_SPRITE_SIZE) * FF6_VEHICLE_SPRITE_SIZE * FF6_BYTES_PER_PIXEL, 0);
    if (common.vehicleGfx.size() < FF6_VEHICLE_GFX_BYTES || palette < 0 ||
        static_cast<size_t>(palette) >= common.spritePaletteCount()) {
        return sprite;
    }
    const uint16_t* colors = &common.spritePalettes[static_cast<size_t>(palette) * FF6_SPRITE_PAL_COLORS];
    const int tile = (word & 0xFF) | ((word >> 8 & 1) << 8);
    const bool flip = (word & 0x4000) != 0;
    const int base = tile & 0x100 ? kObjectCharactersSecond : kObjectCharacters;
    // A sprite of 16 x 16 is four characters of the table, in two rows of 16.
    for (int part = 0; part < 4; ++part) {
        const int low = tile & 0xFF;
        const int number = ((((low >> 4) + part / 2) & 0xF) << 4) | (((low & 0xF) + part % 2) & 0xF);
        const int first = (base + number * kTileWords - FF6_VEHICLE_GFX_VRAM) * 2;
        if (first < 0 || first + kTileWords * 2 > FF6_VEHICLE_GFX_BYTES) continue;
        const uint8_t* planes = &common.vehicleGfx[static_cast<size_t>(first)];
        for (int row = 0; row < kTilePixels; ++row) {
            for (int pixel = 0; pixel < kTilePixels; ++pixel) {
                const int bit = kTilePixels - 1 - pixel;
                const int index = (planes[row * 2] >> bit & 1) | ((planes[row * 2 + 1] >> bit & 1) << 1) |
                                  ((planes[16 + row * 2] >> bit & 1) << 2) | ((planes[17 + row * 2] >> bit & 1) << 3);
                if (index == 0) continue;
                const int column = (part % 2) * kTilePixels + pixel;
                const int x = flip ? FF6_VEHICLE_SPRITE_SIZE - 1 - column : column;
                const int y = (part / 2) * kTilePixels + row;
                colorToRgba(colors[index], &sprite[(static_cast<size_t>(y) * FF6_VEHICLE_SPRITE_SIZE + x) * FF6_BYTES_PER_PIXEL]);
            }
        }
    }
    return sprite;
}

void drawVehicleSprite(uint8_t* screen, const std::vector<uint8_t>& sprite, int x, int y) {
    for (int row = 0; row < FF6_VEHICLE_SPRITE_SIZE; ++row) {
        const int screenY = y + row;
        if (screenY < 0 || screenY >= FF6_SCREEN_HEIGHT) continue;
        for (int column = 0; column < FF6_VEHICLE_SPRITE_SIZE; ++column) {
            const int screenX = x + column;
            if (screenX < 0 || screenX >= FF6_SCREEN_WIDTH) continue;
            const uint8_t* source = &sprite[(static_cast<size_t>(row) * FF6_VEHICLE_SPRITE_SIZE + column) * FF6_BYTES_PER_PIXEL];
            if (source[FF6_ALPHA_OFFSET] == 0) continue;
            uint8_t* target = screen + (static_cast<size_t>(screenY) * FF6_SCREEN_WIDTH + screenX) * FF6_BYTES_PER_PIXEL;
            for (int channel = 0; channel < FF6_BYTES_PER_PIXEL; ++channel) target[channel] = source[channel];
        }
    }
}

VehicleLook vehicleLook(int vehicle, int facing, int position, bool moving) {
    VehicleLook look;
    const int step = (position >> FF6_WALK_POSE_SHIFT) & FF6_WALK_POSE_MASK;
    const int type = vehicle & FF6_VEHICLE_MASK;
    const bool rider = (vehicle & FF6_VEHICLE_RIDER) != 0;
    const auto addRider = [&](int x, int y, int firstRow, int rows) {
        if (rider) look.pieces.push_back({0, x, y, firstRow, rows, true});
    };
    const auto add = [&](uint16_t tile, int x, int y, bool top) { look.pieces.push_back({tile, x, y, 0, 0, top}); };
    // ObjVehicleTileTbl: up and down walk like the others, left and right have their own riding poses.
    const int riderStep = moving ? step : 0;
    const int ridingFrame = facing == Right ? FF6_FRAME_RIDING_RIGHT + (riderStep & 1)
                            : facing == Left ? FF6_FRAME_RIDING_LEFT + (riderStep & 1)
                                             : kMoveFrames[facing][riderStep];
    const int s = step;
    if (type == FF6_VEHICLE_MAGITEK) {
        look.riderFrame = ridingFrame;
        const int variant = facing * FF6_WALK_POSE_COUNT + s;
        addRider(0, -15 + (s & 1), 0, FF6_SPRITE_TOP_HEIGHT);
        add(kMagitekTiles[0][variant], -8, -9, true);
        add(kMagitekTiles[1][variant], 8, -9, true);
        add(kMagitekTiles[2][variant], -8, 7, false);
        add(kMagitekTiles[3][variant], 8, 7, false);
    } else if (type == FF6_VEHICLE_RAFT) {
        look.riderFrame = kMoveFrames[facing][0];
        addRider(0, -17, 0, FF6_SPRITE_TOP_HEIGHT);
        addRider(0, -1, FF6_SPRITE_TOP_HEIGHT, FF6_SPRITE_BOTTOM_HEIGHT);
        add(kRaftTiles[0][facing], -8, -9, true);
        add(kRaftTiles[1][facing], 8, -9, false);
        add(kRaftTiles[2][facing], -8, 7, false);
        add(kRaftTiles[3][facing], 8, 7, false);
    } else if (type == FF6_VEHICLE_CHOCOBO) {
        look.riderFrame = ridingFrame;
        if (facing == Up) {
            add(kChocoUpTail[s], kChocoUpTailX[s], -9 + kChocoUpTailY[s], true);
            addRider(0, -13 + kChocoUpRiderY[s], 0, FF6_SPRITE_TOP_HEIGHT);
            add(kChocoUpBodyTop[s], 0, -9, true);
            add(kChocoUpBodyBottom[s], 0, 7, false);
        } else if (facing == Down) {
            add(kChocoDownHead, kChocoDownHeadX[s], -9 + kChocoDownHeadY[s], true);
            addRider(0, -13 + kChocoDownRiderY[s], 0, FF6_SPRITE_TOP_HEIGHT);
            add(kChocoDownBodyTop[s], 0, -9, true);
            add(kChocoDownBodyBottom[s], 0, 7, false);
        } else {
            const bool right = facing == Right;
            const int riderX = right ? -3 : 3;
            const int riderY = -13 + kChocoSideY[s];
            addRider(riderX, riderY, 0, FF6_SPRITE_TOP_HEIGHT);
            addRider(riderX, riderY + FF6_SPRITE_TOP_HEIGHT, FF6_SPRITE_TOP_HEIGHT, FF6_SPRITE_BOTTOM_HEIGHT);
            const auto& tiles = kChocoSideTiles[right ? 0 : 1];
            add(tiles[0][s], 8, -9, true);
            add(tiles[1][s], 8, 7, false);
            add(tiles[2][s], -8, 7, false);
            add(tiles[3][s], -8, -9, false);
        }
    }
    return look;
}

}  // namespace ff6
