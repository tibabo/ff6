#include "cutscene/falcon.hpp"

#include <algorithm>
#include <cmath>

#include "cutscene/flight.hpp"

namespace ff6 {
namespace {

constexpr int kPivotX = 0x80, kPivotY = 0xD0;  // $7b and $7d: where the center of the plane is on the screen
constexpr int kScale = 0x5370, kDivisorStart = 0x22CD, kDivisorEnd = 0xFEFF;  // $8b, $8d and $8f
constexpr int kLines = FF6_SCREEN_HEIGHT;
constexpr int kVramAnimGfx = 0x6000, kVramAirshipGfx = 0x6400, kVramMisc = 0x7400;
constexpr int kFalconFrame = 0x64;
constexpr int kExitX = 0x3800;  // the scene ends once the Falcon is to the left of this
constexpr int kStartX = 0x2A0, kStartY = 0x2A0;  // the plane has no place that the game gives it, so the camera starts over the land

// WorldSineTbl: the size of the sine of whole degrees.
int sineMagnitude(int degrees) { return static_cast<int>(std::fabs(std::sin(degrees * 3.14159265358979323846 / 180.0)) * 255.0); }

uint16_t word(const std::vector<uint8_t>& bytes, size_t at) { return static_cast<uint16_t>(bytes[at] | bytes[at + 1] << 8); }

}  // namespace

FalconScene::FalconScene(const TitleData& data, const WorldData& world) : world_(world), random_(data.random), frameTable_(data.falconFrames) {
    // The ground is the one of the credits over land (EndingAirshipScene), in the memory of the picture processor.
    ppu_ = FlightBackdrop(data, 24).ppu();
    std::fill(ppu_.vram.begin() + 0x4000, ppu_.vram.end(), 0);
    const auto load = [&](const std::vector<uint8_t>& bytes, size_t to) {
        for (size_t i = 0; i + 1 < bytes.size(); i += 2) ppu_.vram[to + i / 2] = word(bytes, i);
    };
    load(world.animGfx, kVramAnimGfx);
    load(world.airshipGfx[1], kVramAirshipGfx);
    load(world.miscSprites, kVramMisc);
    // The colors of the ground, then the ones of the sprites of the world of balance, with the copies that the game makes.
    for (size_t i = 0; i < 128 && i * 2 + 1 < data.endingAirshipPal.size(); ++i) ppu_.cgram[i] = word(data.endingAirshipPal, i * 2);
    for (size_t i = 0; i < 128 && i < world.worlds[1].spritePalette.size(); ++i) ppu_.cgram[128 + i] = world.worlds[1].spritePalette[i];
    for (size_t i = 0; i < 16; ++i) {
        ppu_.cgram[144 + i] = ppu_.cgram[128 + i];
        ppu_.cgram[160 + i] = ppu_.cgram[224 + i];
    }
    ppu_.objsel = 3;
    ppu_.tm = 0x13;
    ppu_.ts = 0;
    ppu_.m7sel = 0;
    ppu_.forceBlank = false;
    ppu_.brightness = 0;
    positionX_ = kStartX << 8;
    positionY_ = kStartY << 8;
    applyLines();
}

// The matrix of each line is the scale of the perspective, with no turn (the view looks north), and the haze that _ee1930
// builds: blue and red go down by a step every few lines to nothing at the line 144.
void FalconScene::applyLines() {
    int divisor = kDivisorStart;
    const int step = (kDivisorEnd - kDivisorStart) / kLines;
    for (int y = 0; y < kLines; ++y) {
        PpuLine& line = ppu_.lines[static_cast<size_t>(y)];
        const int quotient = kScale / std::max(1, divisor >> 8);
        divisor = (divisor + step) & 0xFFFF;
        line.bgMode = 7;
        line.m7Own = false;
        line.m7a = line.m7d = static_cast<int16_t>((quotient * sineMagnitude(90)) >> 8);
        line.m7b = line.m7c = 0;
        line.cgwsel = 0;
        line.cgadsub = 0x23;
        const uint8_t blue = y < 144 ? static_cast<uint8_t>((0x1200 - 0x20 * y) >> 8) : 0, red = y < 144 ? static_cast<uint8_t>((0x900 - 0x10 * y) >> 8) : 0;
        line.fixed = {red, red, blue};
        line.w12sel = 0x33;
        line.wobjsel = 0x33;
        line.tmw = 0x13;
        line.window = {0x08, 0xF7, 0x08, 0xF7};
    }
    ppu_.m7x = static_cast<uint16_t>((positionX_ >> 8) & 0xFFF);
    ppu_.m7y = static_cast<uint16_t>((positionY_ >> 8) & 0xFFF);
    ppu_.m7hofs = static_cast<uint16_t>((ppu_.m7x - kPivotX) & 0x1FFF);
    ppu_.m7vofs = static_cast<uint16_t>((ppu_.m7y - kPivotY) & 0x1FFF);
}

// The Falcon: its place and its steps, and the speed of the flight.
void FalconScene::updateFalcon() {
    ++frames_;
    if (frames_ >= 0x98 && frames_ < 0xB8) speed_ = ((frames_ - 0x98) << 5) + 0x400;  // it goes faster for half a second
    if (--wingCounter_ == 0) {  // the wings, two frames of the animation, every four frames and then every two
        wing_ = (wing_ + 2) & 2;
        wingCounter_ = frames_ < 0x98 ? 4 : 2;
    }
    shownFrame_ = kFalconFrame + wing_;
    shownX_ = falconX_ >> 8 & 0xFF;
    shownY_ = falconY_ >> 8 & 0xFF;
    falconX_ = (falconX_ + stepX_) & 0xFFFF;
    falconY_ = (falconY_ + stepY_) & 0xFFFF;
    stepX_ = (stepX_ - 1) & 0xFFFF;
    if (frames_ < 0x78) {
        stepY_ = (stepY_ + (falconY_ < 0x8000 ? 5 : -2)) & 0xFFFF;
    } else {
        stepX_ = (stepX_ - 3) & 0xFFFF;
        stepY_ = (stepY_ - 6) & 0xFFFF;
    }
}

// The rest of UpdateSpriteAnim_18: the particles of the set of this frame live (their state goes round, their frame comes from the
// table, they go sideways), and one is born. The game indexes the table by the state alone, so the frames are all nothing, except
// for the first particle, whose index has the high byte of the vertical step in front, which reads past the table.
void FalconScene::updateParticles() {
    const int high = stepY_ >> 8 & 0xFF;
    for (int i = 0; i < 5; ++i) {
        uint8_t* p = &particles_[static_cast<size_t>(particleBase_ + i * 8)];
        if (p[0] != 0) {
            int counter = (p[2] - 1) & 0xFF;
            if (counter == 0) {
                p[0] = static_cast<uint8_t>(p[0] + 1 >= 7 ? 0 : p[0] + 1);
                counter = p[1];
            }
            p[2] = static_cast<uint8_t>(counter);
        }
        const size_t at = static_cast<size_t>(high - 0xFD) * 0x100 + p[0];
        entries_[static_cast<size_t>(i)].frame = i == 0 && high >= 0xFD && at < frameTable_.size() ? frameTable_[at] : 0;
        entries_[static_cast<size_t>(i)].y = p[3];
        const int x = ((p[4] | p[5] << 8) + particleStep_) & 0xFFFF;
        p[4] = static_cast<uint8_t>(x & 0xFF);
        p[5] = static_cast<uint8_t>(x >> 8);
        entries_[static_cast<size_t>(i)].x = x;
    }
    randomIndex_ = (randomIndex_ + 1) & 0xFF;  // $1f6d, then the table of random numbers
    int index = randomIndex_;
    const auto next = [&] {
        index = (index + 1) & 0xFF;
        return random_.empty() ? 0 : random_[static_cast<size_t>(index)];
    };
    int slot = particleBase_;
    while (particles_[static_cast<size_t>(slot)] != 0) {  // the first free one, the search goes on past the set until it comes to 0x28
        slot = (slot + 8) & 0xFF;
        if (slot == 0x28) break;
    }
    if (particles_[static_cast<size_t>(slot)] == 0) {
        particleStep_ = 6 + (next() & 3);
        particles_[static_cast<size_t>(slot + 1)] = particles_[static_cast<size_t>(slot + 2)] = static_cast<uint8_t>((next() & 1) + 1);
        particles_[static_cast<size_t>(slot + 3)] = static_cast<uint8_t>(0x8C + (next() & 7));
        particles_[static_cast<size_t>(slot + 4)] = static_cast<uint8_t>(falconX_ >> 8 & 0xFF);
        particles_[static_cast<size_t>(slot)] = 1;
    }
    particleBase_ = 0x28 - particleBase_;
}

// DrawWorldSprites: the sprites of each frame of the table go where it says; one that would be past the edges goes to the first column.
void FalconScene::drawSprites() {
    ppu_.sprites.clear();
    entries_[6] = {shownFrame_, shownX_, shownY_};
    const std::vector<uint8_t>& bytes = world_.animData;
    for (const Entry& entry : entries_) {
        if (entry.frame <= 0 || static_cast<size_t>(entry.frame) >= world_.animPointers.size() || (entry.x >> 8) != 0) continue;
        size_t at = world_.animPointers[static_cast<size_t>(entry.frame)];
        if (at >= bytes.size()) continue;
        const int number = bytes[at++];
        for (int i = 0; i < number && at + 4 <= bytes.size(); ++i, at += 4) {
            const int x = entry.x + static_cast<int8_t>(bytes[at]);
            PpuSprite sprite;
            sprite.x = x < 0 || x > 255 ? 0 : x;
            sprite.y = (static_cast<int8_t>(bytes[at + 1]) + entry.y + 1) & 0xFF;
            sprite.tile = bytes[at + 2] | ((bytes[at + 3] & 1) << 8);
            sprite.attr = static_cast<uint8_t>(bytes[at + 3] & 0xFE);
            sprite.large = false;
            ppu_.sprites.push_back(sprite);
        }
    }
}

// UpdateVehiclePos: the camera goes along the heading by the speed, a distance that the sine and the cosine of the heading share.
void FalconScene::movePosition() {
    const int base = heading_ >= 180 ? heading_ - 180 : heading_;
    const int sine = sineMagnitude(base), cosine = sineMagnitude(base + 90);
    const bool left = heading_ < 180, north = heading_ < 180 ? heading_ < 90 : heading_ >= 270;
    const auto distance = [&](int factor) { return factor * (speed_ >> 8) + ((factor * (speed_ & 0xFF)) >> 8); };
    positionY_ += north ? -distance(cosine) : distance(cosine);
    positionX_ += left ? -distance(sine) : distance(sine);
    positionX_ = (((positionX_ >> 8) & 0xFFF) << 8) | (positionX_ & 0xFF);
    positionY_ = (((positionY_ >> 8) & 0xFFF) << 8) | (positionY_ & 0xFF);
}

void FalconScene::tick(const InputState&) {
    if (finished_) return;
    updateFalcon();
    updateParticles();
    drawSprites();
    movePosition();
    if (falconX_ < kExitX) target_ = 0;
    applyLines();
    // The screen is blank at first, and the brightness goes to its target a step each frame.
    ppu_.brightness = static_cast<uint8_t>(brightness_);
    if (brightness_ < target_) ++brightness_;
    if (brightness_ > target_) --brightness_;
    if (brightness_ == 0) finished_ = true;
}

}  // namespace ff6
