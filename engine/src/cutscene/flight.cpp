#include "cutscene/flight.hpp"

#include <algorithm>
#include <cstdlib>

namespace ff6 {
namespace {

// Where the tables are in the data of the flights (their addresses in the ROM minus the start of each block).
constexpr size_t kCloudsAssign = 0, kLandAssign = 0x78, kBoxesClouds = 0x140, kBoxesSeaBoat = 0x160, kBoxesLand = 0x180;
constexpr size_t kCloudsGfx = 0x2D3A, kCloudsMap = 0x3C1A, kLandSeaGfx = 0x541A, kLandMap = 0x1D3A, kSeaMap = 0x6CFA, kJetMap = 0x441A;
constexpr size_t kBoxesAirship = 0x1A0;
constexpr size_t kCloudsGfxBytes = 0xEE0, kLandSeaGfxBytes = 0x18E0;
// The scripts of the camera: the ones of the scenes 2, 3 and 6, and of the clouds.
constexpr int kScrollScene2 = 0, kScrollScene3 = 0x69, kScrollScene6 = 0x8A, kScrollScene4 = 0xC6, kScrollScene5 = 0xC9, kScrollClouds1 = 0xCC,
              kScrollClouds2 = 0xD2;
constexpr int kHorizon = 71;  // the lines above it show the sky, in mode 1
constexpr int kLines = FF6_SCREEN_HEIGHT;
// The buttons, as the game keeps them in a word.
constexpr unsigned kR = 0x0010, kL = 0x0020, kX = 0x0040, kA = 0x0080, kRight = 0x0100, kLeft = 0x0200, kDown = 0x0400, kUp = 0x0800,
                   kY = 0x4000, kB = 0x8000, kDirections = 0x0F00;

}  // namespace

FlightBackdrop::FlightBackdrop(const TitleData& data, int state) : data_(data), tasks_(data, ppu_.cgram), state_(state) {
    Ground ground = Ground::Clouds;
    switch (state) {
        case 2:  // clouds 1
            tilt_ = 0x0200;
            zoom_ = 0xF000;
            script_ = kScrollClouds1;
            break;
        case 4:  // clouds 2: the game's tilt of 0x8000 makes the pairs of lines alternate between two zooms, which the original does not show
            tilt_ = 0;
            zoom_ = 0x8000;
            script_ = kScrollClouds2;
            break;
        case 5:  // the airship above the clouds
            ground = Ground::Airship;
            tilt_ = 0x005E;
            zoom_ = 0x2F00;
            centerX_ = 0x100;
            centerY_ = 0x311;
            scrollX_ = 0x80;
            scrollY_ = 0x291;
            break;
        case 9:  // the tiny airship
            script_ = kScrollScene2;
            break;
        case 32:  // the big airship with its jet trails
            ground = Ground::Jet;
            scale_ = 1;
            centerX_ = 8;
            centerY_ = 0x18;
            scrollX_ = 0xFFD8;
            scrollY_ = 0xFFD0;
            break;
        case 12:  // the sea with the boat
            ground = Ground::SeaBoat;
            zoom_ = 0x1000;
            script_ = kScrollScene3;
            break;
        case 16:  // the sea with the airship
            ground = Ground::Sea;
            angle_ = 0xFF6D;
            tilt_ = 0x034C;
            zoom_ = 0xFD00;
            skyY_ = 0x28;
            sky_ = Sky::Left;
            script_ = kScrollScene4;
            break;
        case 20:  // the land with the birds
            ground = Ground::Land;
            angle_ = 0xFFCD;
            tilt_ = 0x030B;
            zoom_ = 0xF000;
            skyY_ = 0x28;
            sky_ = Sky::Right;
            script_ = kScrollScene5;
            break;
        case 28:  // the big airship
            ground = Ground::Land;
            tilt_ = 0x02DC;
            zoom_ = 0x0100;
            skyY_ = 0x10;
            sky_ = Sky::Rising;
            break;
        default:  // the land
            ground = Ground::Land;
            zoom_ = 0x1800;
            script_ = kScrollScene6;
            break;
    }
    ppu_.objsel = 0x03;
    ppu_.tm = state == 32 ? 0x11 : 0x13;
    ppu_.m7sel = state == 32 ? 0x80 : 0;
    ppu_.ts = 0x10;
    ppu_.brightness = 15;
    ppu_.forceBlank = false;
    for (size_t pal = 0; pal < 8; ++pal) {  // the palettes that the game loads: 1 to 4, 6 and 7
        static constexpr int kSource[8] = {-1, 0, 1, 2, 3, -1, 4, 5};
        if (kSource[pal] < 0) continue;
        for (size_t i = 0; i < 16; ++i) {
            const size_t at = static_cast<size_t>(kSource[pal]) * 32 + i * 2;
            ppu_.cgram[pal * 16 + i] = static_cast<uint16_t>(data.flightPalettes[at] | data.flightPalettes[at + 1] << 8);
        }
    }
    if (state == 5 || state == 32) {  // _c3d018: the first two palettes of the background are the pale one
        const std::array<uint16_t, 16> pale = tasks_.palette(0x959C);
        for (size_t i = 0; i < pale.size(); ++i) ppu_.cgram[i] = ppu_.cgram[16 + i] = pale[i];
    }
    loadSprites();
    pages_ = creditsPages(creditsScene(state), data.credits);
    tasks_.creditsNormal = state == 5 || state == 9;  // _c3c5dc: the palette of the clouds, not its inverse
    if (!pages_.empty()) loadCreditsFont();
    build(ground);
    if (sky_ != Sky::None) buildSky();
    duration_ = script_ >= 0 ? data.flightScrolls[static_cast<size_t>(script_) + 2] : 0xFE;
    updateMatrix();
    applyLines();
}

// _c3d6d6 and LoadCreditsSpritePal: the first characters of the credits graphics are the ones of the sprites (and for the big
// airship, the ones after them), with their palettes.
void FlightBackdrop::loadSprites() {
    const auto transfer = [&](size_t from, size_t bytes, size_t to) {
        for (size_t i = 0; i + 1 < bytes && from + i + 1 < data_.flightGfx.size(); i += 2) {
            ppu_.vram[to + i / 2] = static_cast<uint16_t>(data_.flightGfx[from + i] | data_.flightGfx[from + i + 1] << 8);
        }
    };
    transfer(0, 0x17BA, 0x6000);
    if (state_ == 28) transfer(0x17BA, 0x580, 0x6C00);
    static constexpr int kPalettes[][2] = {{0, 0x97F4}, {1, 0x9794}, {2, 0x97B4}, {3, 0x97D4}, {6, 0x9834}, {7, 0x9814}};
    for (const auto& [number, address] : kPalettes) {
        const std::array<uint16_t, 16> colors = tasks_.palette(address);
        for (size_t i = 0; i < colors.size(); ++i) ppu_.cgram[128 + static_cast<size_t>(number) * 16 + i] = colors[i];
    }
}

// LoadCreditsFontGfx: the big letters are 8x16 (two tiles of the font in a 16x16 sprite, the right half blank), which the
// game puts in a sheet by a table (_c3d497); the 2bpp tiles go to the second table of the sprites, with the small letters at
// its end (a to z, then the period and the space).
void FlightBackdrop::loadCreditsFont() {
    constexpr size_t kTable = 0x9854 - 0x8A70, kEntries = 64, kSheet = 0xC00, kFirst = 0x400, kVram = 0x7000;
    std::vector<uint8_t> sheet(kSheet, 0);
    std::copy(data_.endingFont.begin(), data_.endingFont.begin() + static_cast<long>(std::min(data_.endingFont.size(), kSheet)), sheet.begin());
    for (size_t entry = 0; entry < kEntries && kTable + entry < data_.endingAnim.size(); ++entry) {
        const size_t from = data_.endingAnim[kTable + entry] * 16u, to = kFirst + entry * 32;
        for (size_t i = 0; i < 16; ++i) sheet[to + i] = sheet[from + i];
        std::fill(sheet.begin() + static_cast<long>(to) + 16, sheet.begin() + static_cast<long>(to) + 32, uint8_t{0});
    }
    const auto tile = [&](const uint8_t* bytes, size_t to) {  // a tile of two bits per pixel, as four
        for (size_t row = 0; row < 8; ++row) ppu_.vram[to + row] = static_cast<uint16_t>(bytes[row * 2] | bytes[row * 2 + 1] << 8);
        for (size_t row = 8; row < 16; ++row) ppu_.vram[to + row] = 0;
    };
    for (size_t i = 0; i < kSheet / 16; ++i) tile(&sheet[i * 16], kVram + i * 16);
    const std::vector<uint8_t>& small = data_.creditsSmallFont;
    if (small.size() < FF6_TITLE_CREDITS_SMALL_FONT_BYTES) return;
    for (size_t i = 0; i < 0x200 / 16; ++i) tile(&small[i * 16], 0x7E00 + i * 16);
    tile(&small[0x200], 0x7FA0);
    tile(&small[0x210], 0x7FB0);
}

// The birds of a scene: six sprites with their place in a table (the address of the animation, the position across and the
// height), each one in a task that makes it sway.
void FlightBackdrop::createBirds(int table, bool follows) {
    for (int i = 0; i < 6; ++i) {
        const int at = table + i * 4;
        EndingTasks::Task& bird = tasks_.create(2, &EndingTasks::oscillatingBirdTask);
        tasks_.setAnimation(bird, EndingTasks::kBankEnding2, tasks_.word(EndingTasks::kBankEnding2, at));
        bird.speedX = tasks_.byte(EndingTasks::kBankEnding2, at + 2) << 8;
        bird.posY = tasks_.byte(EndingTasks::kBankEnding2, at + 3) << 8;
        bird.flags = follows ? 1 : 0;
    }
}

EndingTasks::Task& FlightBackdrop::createAirship(EndingTasks::Code code, int priority, int address, int x, int y) {
    EndingTasks::Task& task = tasks_.create(priority, code);
    tasks_.setAnimation(task, EndingTasks::kBankEnding1, address);
    task.posX = x << 8;
    task.posY = y << 8;
    return task;
}

// The states of the game that create the sprites of a flight: the ones at its start, and the ones that a wait brings later (the
// tick is the frame of the scene, counted from one).
void FlightBackdrop::startSprites() {
    static constexpr int kShipBirds = 0xF7B5, kSeaBirds = 0xF7CD, kLandBirds = 0xF7E5;
    static constexpr int kAirshipLeft = 0x8BB1, kAirshipShadow = 0x8BEC, kAirshipRight = 0x8C25, kBigAirship = 0x8C60;
    static constexpr int kPropellerLeft = 0x8D3E, kPropellerRight = 0x8D91, kTinyAirship = 0x8CDC, kTinyShadow = 0x8CF0;
    static constexpr int kBoatLeft = 0xF809, kBoatRight = 0xF7FD;
    const auto moving = [&](EndingTasks::Task& task, int speedX, int speedY, int counter) {
        task.speedX = speedX & 0xFFFF;
        task.speedY = speedY & 0xFFFF;
        task.counter = counter;
    };
    switch (state_) {
        case 9:
            if (ticks_ == 16 * 60 + 1) {  // InitTinyAirshipTasks: the airship and its shadow cross the screen slowly
                moving(createAirship(&EndingTasks::animCounterTask, 1, kTinyAirship, 0xF8, 0xA0), -64, -32, 0x400);
                moving(createAirship(&EndingTasks::animCounterTask, 3, kTinyShadow, 0xF8, 0xB0), -64, -32, 0x400);
            }
            break;
        case 12:
            if (ticks_ == 1) createBirds(kShipBirds, true);
            if (ticks_ == 18 * 60 + 1) {  // _c3c94e: the boat, whose right half starts a second later
                EndingTasks::Task& left = tasks_.create(3, &EndingTasks::animTask);
                tasks_.setAnimation(left, EndingTasks::kBankEnding2, kBoatLeft);
                left.speedX = 0xFFC0;
                left.posX = 0xF800;
                left.posY = 0x5000;
                EndingTasks::Task& right = tasks_.create(1, &EndingTasks::boatTask);
                tasks_.setAnimation(right, EndingTasks::kBankEnding2, kBoatRight);
                right.speedX = 0xFFC0;
                right.posX = 0xF800;
                right.posY = 0x5000;
            }
            break;
        case 16:
            if (ticks_ == 1) {
                createBirds(kSeaBirds, false);
                createAirship(&EndingTasks::splashAirshipTask, 2, kAirshipLeft, 0x78, 0x58);
                createAirship(&EndingTasks::animTask, 3, kAirshipShadow, 0x80, 0x80);
            }
            break;
        case 20:
            if (ticks_ == 1) {
                createBirds(kLandBirds, false);
                EndingTasks::Task& ship = createAirship(&EndingTasks::airshipTask, 2, kAirshipRight, 0x78, 0);
                ship.speedY = 0x5800;
            }
            if (ticks_ == 20 * 60 + 1) {  // _c3cd97: four birds that cross it
                static constexpr int kPlaces[4][2] = {{0x00, 0x50}, {0x18, 0x40}, {0x40, 0x68}, {0x10, 0x48}};
                for (const auto& place : kPlaces) {
                    EndingTasks::Task& bird = tasks_.create(2, &EndingTasks::flyingBirdTask);
                    bird.posX = place[0] << 8;
                    bird.posY = place[1] << 8;
                }
            }
            break;
        case 28:
            if (ticks_ == 1) {  // the four propellers, then the airship
                struct Propeller {
                    int address, speedY, x;
                };
                for (const Propeller& p : {Propeller{kPropellerLeft, 0x14, 0x44}, Propeller{kPropellerRight, 0x14, 0xAC},
                                           Propeller{kPropellerLeft, 0xE8, 0x60}, Propeller{kPropellerRight, 0xE8, 0x90}}) {
                    EndingTasks::Task& task = createAirship(&EndingTasks::airshipTask, 2, p.address, p.x, 0);
                    task.speedY = p.speedY << 8;
                }
                EndingTasks::Task& ship = createAirship(&EndingTasks::airshipTask, 2, kBigAirship, 0x48, 0);
                ship.speedY = 0xF800;
            }
            break;
        default: break;
    }
}

// _c3d0f2 and _c3d0a8: one tile of the map takes one of two numbers each frame, so the propellers and the jet trails move.
void FlightBackdrop::animateTiles() {
    const bool odd = (frame_ & 1) != 0;
    const auto set = [&](size_t word, int tile) { ppu_.vram[word] = static_cast<uint16_t>((ppu_.vram[word] & 0xFF00) | tile); };
    if (state_ == 5) set(0x892, odd ? 0x58 : 0x34);
    if (state_ == 32) {
        set(0x283, odd ? 0x51 : 0x59);
        set(0x206, odd ? 0x4F : 0x5A);
    }
    if (state_ == 32 && ticks_ == 2 * 60 + 30 + 1) {  // the jet fades away
        tasks_.blackPalette(16, 4);
        tasks_.blackPalette(32, 4);
        tasks_.blackPalette(0, 4);
    }
}

// The pixels of 256 tiles in the high bytes of the words of the video memory, and the map in the low bytes: the tiles take
// the palette that the table gives them, and the map is the boxes of 32 x 32 that the table lists, twice (one for each half).
void FlightBackdrop::build(Ground ground) {
    const bool clouds = ground == Ground::Clouds || ground == Ground::Airship || ground == Ground::Jet;
    const size_t gfxAt = clouds ? kCloudsGfx : kLandSeaGfx;
    const size_t gfxBytes = clouds ? kCloudsGfxBytes : kLandSeaGfxBytes;
    const size_t assignAt = clouds ? kCloudsAssign : kLandAssign;
    const size_t mapAt = ground == Ground::Clouds || ground == Ground::Airship ? kCloudsMap : ground == Ground::Jet ? kJetMap : ground == Ground::Land ? kLandMap : kSeaMap;
    const size_t boxesAt = ground == Ground::Clouds || ground == Ground::Sea ? kBoxesClouds
                           : ground == Ground::SeaBoat ? kBoxesSeaBoat
                           : ground == Ground::Airship ? kBoxesAirship
                                                       : kBoxesLand;
    for (size_t tile = 0; tile < 256; ++tile) {
        if (tile * 32 + 32 > gfxBytes) break;
        const uint8_t* planes = &data_.flightGfx[gfxAt + tile * 32];
        const int palette = data_.flightTables[assignAt + tile];
        for (size_t row = 0; row < 8; ++row) {
            for (size_t pixel = 0; pixel < 8; ++pixel) {
                const int bit = 7 - static_cast<int>(pixel);
                int color = (planes[row * 2] >> bit & 1) | ((planes[row * 2 + 1] >> bit & 1) << 1) | ((planes[16 + row * 2] >> bit & 1) << 2) |
                            ((planes[17 + row * 2] >> bit & 1) << 3);
                if (color != 0) color |= (palette << 4) & 0xF0;
                ppu_.vram[tile * 64 + row * 8 + pixel] = static_cast<uint16_t>(color << 8);
            }
        }
    }
    // The boxes: where each one starts in the map, and where it goes in the plane (a byte of the video memory for each
    // pair of bytes of the buffer, so half of it is the tile where it lands).
    for (size_t half = 0; half < 2; ++half) {
        for (size_t box = 0; box < 8; ++box) {
            const uint8_t* entry = &data_.flightTables[boxesAt + box * 4];
            const size_t source = static_cast<size_t>(entry[0] | entry[1] << 8), destination = static_cast<size_t>(entry[2] | entry[3] << 8) / 2;
            for (size_t row = 0; row < 32; ++row) {
                for (size_t column = 0; column < 32; ++column) {
                    const size_t word = half * 0x2000 + destination + row * 128 + column;
                    const size_t from = mapAt + source + row * 32 + column;
                    if (word < ppu_.vram.size() && from < data_.flightGfx.size()) ppu_.vram[word] = static_cast<uint16_t>((ppu_.vram[word] & 0xFF00) | data_.flightGfx[from]);
                }
            }
        }
    }
}

// LoadCloudsBackdropGfx: the characters of the sky at $4000, its map at $5000 on bg2 (with the high priority bit), and its palette.
void FlightBackdrop::buildSky() {
    for (size_t i = 0; i + 1 < data_.skyGfx.size(); i += 2) ppu_.vram[0x4000 + i / 2] = static_cast<uint16_t>(data_.skyGfx[i] | data_.skyGfx[i + 1] << 8);
    for (size_t i = 0; i + 1 < data_.skyMap.size(); i += 2) ppu_.vram[0x5000 + i / 2] = static_cast<uint16_t>((data_.skyMap[i] | data_.skyMap[i + 1] << 8) + 0x2000);
    for (size_t i = 0; i < 16; ++i) ppu_.cgram[i] = static_cast<uint16_t>(data_.skyPal[i * 2] | data_.skyPal[i * 2 + 1] << 8);
    ppu_.bgsc = {0x58, 0x50, 0x7C};
    ppu_.nba12 = 0x44;
}

// UpdateMode7HDMA: for each pair of lines, the scale grows toward the horizon by the tilt, and the angle gives the matrix.
void FlightBackdrop::updateMatrix() {
    const auto sine = [&](int index) {
        const size_t at = static_cast<size_t>(index & 0xFF) * 2;
        return static_cast<int16_t>(data_.sine16[at] | data_.sine16[at + 1] << 8);
    };
    const int cosine = sine(angle_ + 0x40), sinus = sine(angle_);
    const int cosineHalf = std::abs(cosine) >> 1, sinusHalf = std::abs(sinus) >> 1;
    int level = zoom_ & 0xFFFF;
    for (int entry = 111; entry >= 0; --entry) {
        const int divisor = level >> 8 & 0xFF;
        const int quotientA = divisor ? cosineHalf / divisor : 0xFFFF, quotientB = divisor ? sinusHalf / divisor : 0xFFFF;
        a_[static_cast<size_t>(entry)] = static_cast<int16_t>(cosine >= 0 ? quotientA : -quotientA);
        b_[static_cast<size_t>(entry)] = static_cast<int16_t>(sinus >= 0 ? quotientB : -quotientB);
        c_[static_cast<size_t>(entry)] = static_cast<int16_t>(-b_[static_cast<size_t>(entry)]);
        level = (level - tilt_) & 0xFFFF;
    }
}

// The tables of the lines: 123 lines from the first word, then 101 from the one before the next (the game's tables overlap).
void FlightBackdrop::applyLines() {
    if (state_ == 32) {  // no table: one matrix for the whole screen, that grows two steps each frame
        for (PpuLine& out : ppu_.lines) {
            out.bgMode = 7;
            out.m7a = out.m7d = static_cast<int16_t>(scale_);
            out.m7b = out.m7c = 0;
            out.cgwsel = 0x82;
            out.cgadsub = 0x01;
        }
    }
    for (int line = 0; line < kLines && state_ != 32; ++line) {
        const size_t entry = static_cast<size_t>((state_ == 5 && line >= 124 ? line - 2 : line < 123 ? line : line - 1) / 2);
        PpuLine& out = ppu_.lines[static_cast<size_t>(line)];
        if (state_ == 5 && line < 124) {  // the airship: its own scale, and its own place on the plane
            out.bgMode = 7;
            out.m7a = out.m7d = static_cast<int16_t>(scale_);
            out.m7b = out.m7c = 0;
            out.m7Own = true;
            out.m7x = 0x71;
            out.m7y = 0x8D;
            out.m7hofs = 0xFFF1;
            out.m7vofs = 0x2E;
            continue;
        }
        out.m7Own = false;
        out.bgMode = sky_ != Sky::None && line < kHorizon ? 1 : 7;
        out.m7a = out.m7d = a_[entry];
        out.m7b = b_[entry];
        out.m7c = c_[entry];
    }
    if (sky_ != Sky::None) {
        // The tables of the color math and of the fixed color: nothing above the horizon, then a haze that fades down the screen.
        static constexpr int kHaze[][2] = {{1, 13}, {1, 11}, {1, 10}, {2, 9}, {3, 8}, {4, 7}, {5, 6}, {6, 5}, {7, 4}, {8, 3}, {12, 2}, {15, 1}};
        int line = kHorizon;
        for (PpuLine& out : ppu_.lines) {
            out.cgwsel = 0x82;  // CreditsColorMathHDMATbl: from the horizon on, the sub screen and the eight pixels blacked out
            out.cgadsub = 0x01;
            out.fixed = {0, 0, 0};
        }
        for (const auto& step : kHaze) {
            for (int i = 0; i < step[0] && line < kLines; ++i, ++line) {
                const uint8_t level = static_cast<uint8_t>(step[1]);
                ppu_.lines[static_cast<size_t>(line)].fixed = {level, level, level};
            }
        }
        for (int y = 0; y < kHorizon; ++y) {
            PpuLine& out = ppu_.lines[static_cast<size_t>(y)];
            out.cgwsel = 0x80;
            out.cgadsub = 0x41;
            out.hofs[1] = static_cast<uint16_t>(skyX_);
            out.vofs[1] = static_cast<uint16_t>(skyY_);
        }
    }
    if (state_ == 5) {  // FixedColorHDMATbl: a haze that peaks at the horizon of the clouds, then the last line's registers
        static constexpr int kHaze[][2] = {{99, 0}, {1, 1}, {1, 2},  {1, 3},  {1, 4},  {1, 5},  {1, 6},  {1, 7},  {1, 8},  {1, 9},
                                           {1, 10}, {1, 11}, {1, 12}, {2, 14}, {3, 16}, {3, 19}, {2, 22}, {1, 26}, {1, 27}, {1, 28},
                                           {1, 15}, {1, 12}, {1, 10}, {1, 6},  {1, 3},  {2, 2},  {3, 1}};
        int line = 0;
        for (const auto& step : kHaze) {
            for (int i = 0; i < step[0] && line < kLines; ++i, ++line) {
                PpuLine& out = ppu_.lines[static_cast<size_t>(line)];
                const uint8_t level = static_cast<uint8_t>(step[1]);
                out.cgwsel = 0x80;
                out.cgadsub = 0x01;
                out.fixed = {level, level, level};
            }
        }
        for (; line < kLines; ++line) {
            PpuLine& out = ppu_.lines[static_cast<size_t>(line)];
            out.cgwsel = 0x82;
            out.cgadsub = 0x81;
            out.fixed = {0, 0, 0};
        }
    }
    if (sky_ == Sky::None && state_ != 5 && state_ != 32) {  // InitHWRegsCredits: the sub screen, and BG1 over it (the clouds take it away, the others add it)
        for (PpuLine& out : ppu_.lines) {
            out.cgwsel = 0x82;
            out.cgadsub = state_ == 2 || state_ == 4 || state_ == 9 ? 0x81 : 0x01;
        }
    }
    // InitEndingWindowMask: eight pixels at each side of the screen, hidden from the layers and blacked out from the rest.
    for (PpuLine& out : ppu_.lines) {
        out.w12sel = 0x33;
        out.wobjsel = 0x33;
        out.tmw = 0x0F;
        out.window = {0x08, 0xF7, 0x08, 0xF7};
    }
    ppu_.m7x = static_cast<uint16_t>(centerX_);
    ppu_.m7y = static_cast<uint16_t>(centerY_);
    ppu_.m7hofs = static_cast<uint16_t>(scrollX_);
    ppu_.m7vofs = static_cast<uint16_t>(scrollY_);
}

uint16_t FlightBackdrop::scriptWord(int at) const {
    return static_cast<uint16_t>(data_.flightScrolls[static_cast<size_t>(at)] | data_.flightScrolls[static_cast<size_t>(at) + 1] << 8);
}

// The camera task, then the script that presses the buttons for the next frame (the same order as the threads).
void FlightBackdrop::tick() {
    const unsigned held = buttons_;
    if (held & kR) angle_ = (angle_ + 1) & 0xFFFF;
    if (held & kL) angle_ = (angle_ - 1) & 0xFFFF;
    if (held & kX) tilt_ = (tilt_ + 1) & 0xFFFF;
    if (held & kY) tilt_ = (tilt_ - 1) & 0xFFFF;
    if (held & kA) zoom_ = (zoom_ & 0x00FF) | (((zoom_ >> 8) + 1) & 0xFF) << 8;
    if (held & kB) zoom_ = (zoom_ & 0x00FF) | (((zoom_ >> 8) - 1) & 0xFF) << 8;
    if (held & kUp) {
        --centerY_;
        --scrollY_;
    }
    if (held & kDown) {
        ++centerY_;
        ++scrollY_;
    }
    if (held & kLeft) {
        --centerX_;
        --scrollX_;
    }
    if (held & kRight) {
        ++centerX_;
        ++scrollX_;
    }
    centerX_ &= 0x1FFF;
    centerY_ &= 0x1FFF;
    if (held & ~kDirections & 0xFFFF) updateMatrix();
    // The tasks that move the sky: every other frame to a side, or a line every 128 frames.
    if (sky_ == Sky::Left && (frame_ & 1) == 0) --skyX_;
    if (sky_ == Sky::Right && (frame_ & 1) == 0) ++skyX_;
    if (sky_ == Sky::Rising && (frame_ & 0x7F) == 0) ++skyY_;
    if (state_ == 28 && (frame_ & 1) == 0) {  // _c3cf9a: the ground creeps toward the horizon
        --centerY_;
        --scrollY_;
    }
    if (state_ == 5) {  // _c3c69a and _c3c6bf: the airship goes away (one step every other frame), the clouds go by
        if ((frame_ & 1) == 0) ++scale_;
        --scrollX_;
        --centerX_;
    }
    if (state_ == 32) {  // _c3d127: the jet gets smaller, and the plane drifts
        if ((frame_ & 7) == 0) {
            --centerY_;
            ++centerX_;
        }
        scale_ += 2;
    }
    ++frame_;
    applyLines();
    ++ticks_;
    animateTiles();
    startSprites();
    for (const CreditsPage& page : pages_) {
        if (page.start != tasks_.frameCounter) continue;
        for (const CreditsWord& word : page.words) tasks_.createCreditsWord(word.codes, word.big, word.x, word.y);
    }
    tasks_.bg1HScroll = scrollX_;
    tasks_.exec(ppu_.sprites);
    ++tasks_.frameCounter;
    // UpdateAnimData over the entries of three bytes: a duration is a count of frames, 0xFE stays, 0xFF starts again.
    if (script_ < 0) return;
    for (;;) {
        if (duration_ == 0xFE) break;
        if (duration_ == 0xFF) {
            index_ = 0;
        } else if (duration_ == 0) {
            index_ += 3;
        } else {
            --duration_;
            break;
        }
        duration_ = data_.flightScrolls[static_cast<size_t>(script_ + index_) + 2];
    }
    buttons_ = scriptWord(script_ + index_);
}

std::unique_ptr<FlightBackdrop> makeFlightBackdrop(const TitleData& data, int state) {
    if (state != 2 && state != 4 && state != 5 && state != 9 && state != 12 && state != 16 && state != 20 && state != 24 && state != 28 && state != 32) return nullptr;
    if (data.flightGfx.size() < FF6_TITLE_FLIGHT_GFX_BYTES) return nullptr;
    return std::make_unique<FlightBackdrop>(data, state);
}

}  // namespace ff6
