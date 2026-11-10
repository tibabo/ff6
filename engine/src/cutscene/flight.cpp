#include "cutscene/flight.hpp"

#include <algorithm>
#include <cstdlib>

namespace ff6 {
namespace {

// Where the tables are in the data of the flights (their addresses in the ROM minus the start of each block).
constexpr size_t kCloudsAssign = 0, kLandAssign = 0x78, kBoxesClouds = 0x140, kBoxesSeaBoat = 0x160, kBoxesLand = 0x180;
constexpr size_t kCloudsGfx = 0x2D3A, kCloudsMap = 0x3C1A, kLandSeaGfx = 0x541A, kLandMap = 0x1D3A, kSeaMap = 0x6CFA;
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

FlightBackdrop::FlightBackdrop(const TitleData& data, int state) : data_(data) {
    Ground ground = Ground::Clouds;
    switch (state) {
        case 2:  // clouds 1
            tilt_ = 0x0200;
            zoom_ = 0xF000;
            script_ = kScrollClouds1;
            break;
        case 4:  // clouds 2
            tilt_ = 0x8000;
            zoom_ = 0x8000;
            script_ = kScrollClouds2;
            break;
        case 9:  // the tiny airship
            script_ = kScrollScene2;
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
    ppu_.tm = 0x13;
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
    light_ = ground == Ground::Clouds;
    build(ground);
    if (sky_ != Sky::None) buildSky();
    duration_ = script_ >= 0 ? data.flightScrolls[static_cast<size_t>(script_) + 2] : 0xFE;
    updateMatrix();
    applyLines();
}

// The pixels of 256 tiles in the high bytes of the words of the video memory, and the map in the low bytes: the tiles take
// the palette that the table gives them, and the map is the boxes of 32 x 32 that the table lists, twice (one for each half).
void FlightBackdrop::build(Ground ground) {
    const size_t gfxAt = ground == Ground::Clouds ? kCloudsGfx : kLandSeaGfx;
    const size_t gfxBytes = ground == Ground::Clouds ? kCloudsGfxBytes : kLandSeaGfxBytes;
    const size_t assignAt = ground == Ground::Clouds ? kCloudsAssign : kLandAssign;
    const size_t mapAt = ground == Ground::Clouds ? kCloudsMap : ground == Ground::Land ? kLandMap : kSeaMap;
    const size_t boxesAt = ground == Ground::Clouds || ground == Ground::Sea ? kBoxesClouds : ground == Ground::SeaBoat ? kBoxesSeaBoat : kBoxesLand;
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
    for (int line = 0; line < kLines; ++line) {
        const size_t entry = static_cast<size_t>((line < 123 ? line : line - 1) / 2);
        PpuLine& out = ppu_.lines[static_cast<size_t>(line)];
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
            out.cgwsel = 0x82;
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
    ++frame_;
    applyLines();
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
    if (state != 2 && state != 4 && state != 9 && state != 12 && state != 16 && state != 20 && state != 24 && state != 28) return nullptr;
    if (data.flightGfx.size() < FF6_TITLE_FLIGHT_GFX_BYTES) return nullptr;
    return std::make_unique<FlightBackdrop>(data, state);
}

}  // namespace ff6
