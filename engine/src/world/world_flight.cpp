#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "world/world_screen.hpp"

// The airship (src/world/ctrl.asm, move.asm, rotate.asm, liftoff.asm, sprite.asm): the party takes off from the tile where
// the airship is parked, flies with the map turning around it in mode 7 under a sky of the second background, and lands
// where the ground allows it.

namespace ff6 {
namespace {

constexpr int kVramBackdropMap = 0x4400;
constexpr int kVramBackdropGfx = 0x5000;
constexpr int kShadowFirstSprite = 2;
constexpr int kShipFirstSprite = 20;
constexpr int kParkedFirstSprite = 26;
constexpr size_t kArrowFirstSprite = 40;
constexpr int kTweenSteps = 32;
constexpr int kDegrees = 360;
constexpr int kFlightPivot = 0xD0;
constexpr int kMaxSpeed = 0x800;
constexpr int kChocoboSpeed = 0x100;
constexpr unsigned kFlagNoInput = 0x01, kFlagNoTurn = 0x02, kFlagNoRotation = 0x08, kFlagNoHorizon = 0x10, kFlagMoveAngle = 0x20, kFlagNoAltitude = 0x40;  // $1e
constexpr int kMaxRate = 0x200;
constexpr int kMaxAltitude = 0x7E00;
constexpr int kCannotLand = 0x0002;  // in the low byte of the properties of a tile
constexpr int kVeldtTile = 0x2000;
constexpr int kKefkaTowerTile = 0x8000, kPhoenixCaveTile = 0x4000;
constexpr uint32_t kDeckEvent = 0x68, kGroundEvent = 0x59, kKefkaTowerEvent = 0x7F, kPhoenixCaveEvent = 0x88;

// _ee98f1: for each step of the take off, what the camera moves by: the scale, the divisor, the end of the divisor, the
// horizon and the line of the party.
constexpr uint8_t kTween[kTweenSteps][7] = {
    {0x0E, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00}, {0x3A, 0x00, 0x49, 0x00, 0x00, 0x00, 0x00}, {0x81, 0x00, 0xA1, 0x00, 0x01, 0x00, 0x00},
    {0xE1, 0x00, 0x19, 0x01, 0x02, 0x01, 0x00}, {0x55, 0x01, 0xAA, 0x01, 0x03, 0x01, 0x01}, {0xDA, 0x01, 0x50, 0x02, 0x05, 0x02, 0x01},
    {0x6A, 0x02, 0x04, 0x03, 0x06, 0x02, 0x02}, {0x00, 0x03, 0xBF, 0x03, 0x08, 0x03, 0x02}, {0x96, 0x03, 0x7B, 0x04, 0x0A, 0x04, 0x03},
    {0x26, 0x04, 0x2F, 0x05, 0x0B, 0x04, 0x04}, {0xAB, 0x04, 0xD5, 0x05, 0x0D, 0x05, 0x04}, {0x1F, 0x05, 0x66, 0x06, 0x0E, 0x05, 0x05},
    {0x7F, 0x05, 0xDE, 0x06, 0x0F, 0x06, 0x05}, {0xC6, 0x05, 0x36, 0x07, 0x10, 0x06, 0x05}, {0xF1, 0x05, 0x6D, 0x07, 0x10, 0x06, 0x05},
    {0x00, 0x06, 0x80, 0x07, 0x10, 0x06, 0x05}, {0xF1, 0x05, 0x6D, 0x07, 0x10, 0x06, 0x05}, {0xC6, 0x05, 0x36, 0x07, 0x10, 0x06, 0x05},
    {0x7F, 0x05, 0xDE, 0x06, 0x0F, 0x06, 0x05}, {0x1F, 0x05, 0x66, 0x06, 0x0E, 0x05, 0x05}, {0xAB, 0x04, 0xD5, 0x05, 0x0D, 0x05, 0x04},
    {0x26, 0x04, 0x2F, 0x05, 0x0B, 0x04, 0x04}, {0x96, 0x03, 0x7B, 0x04, 0x0A, 0x04, 0x03}, {0x00, 0x03, 0xC0, 0x03, 0x08, 0x03, 0x03},
    {0x6A, 0x02, 0x04, 0x03, 0x06, 0x02, 0x02}, {0xDA, 0x01, 0x50, 0x02, 0x05, 0x02, 0x01}, {0x55, 0x01, 0xAA, 0x01, 0x03, 0x01, 0x01},
    {0xE1, 0x00, 0x19, 0x01, 0x02, 0x01, 0x00}, {0x81, 0x00, 0xA1, 0x00, 0x01, 0x00, 0x00}, {0x3A, 0x00, 0x49, 0x00, 0x00, 0x00, 0x00},
    {0x0E, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00}, {0x02, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00}};

// AirshipDirAnimOffset: the frame of the airship for the way it turns (0, 4 or 8) and climbs (1 straight, 2 up, 3 down).
constexpr int kDirAnimOffset[12] = {0, 1, 7, 13, 0, 5, 11, 17, 0, 3, 9, 15};

// _ee4952: where the parked airship is on the screen and how big, for its distance in front of the party.
constexpr uint8_t kParkedTable[0xB2] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x04, 0x05, 0x06, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0A, 0x0B, 0x0C, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,
    0x12, 0x13, 0x14, 0x15, 0x16, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C,
    0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x54, 0x55, 0x56,
    0x57, 0x58, 0x5A, 0x5B, 0x5C, 0x5E, 0x5F, 0x60, 0x61, 0x63, 0x64, 0x65, 0x66, 0x68, 0x69, 0x6B, 0x6C, 0x6D, 0x6F, 0x70, 0x71, 0x73, 0x74,
    0x76, 0x77, 0x78, 0x7A, 0x7C, 0x7D, 0x7E, 0x80, 0x81, 0x83, 0x85, 0x86, 0x88, 0x89, 0x8B, 0x8C, 0x8E, 0x90, 0x91, 0x93, 0x94, 0x96, 0x98,
    0x9A, 0x9B, 0x9D, 0x9F, 0xA0, 0xA3, 0xA4, 0xA6, 0xA8, 0xA9, 0xAB, 0xAD, 0xAF, 0xB1, 0xB3, 0xB5, 0xB7, 0xB8, 0xBB, 0xBD, 0xBE, 0xC1, 0xC3,
    0xC4, 0xC7, 0xC9, 0xCB, 0xCD, 0xCF, 0xD1, 0xD3, 0xD6, 0xD8, 0xDA, 0xDC, 0xDF, 0xE2, 0xE5, 0xE8, 0xEB};

// WorldSineTbl: the size of the sine of whole degrees, 0 to 255.
int sineMagnitude(int degrees) { return static_cast<int>(std::fabs(std::sin(degrees * 3.14159265358979323846 / 180.0)) * 255.0); }

int absNot(int value) { return value < 0 ? ~value : value; }

constexpr int kRampRows = 31, kRampSize = 32;

// InitOffsetPerTileData: for turns, each row is a ramp from 0 to the number of the row, to tilt the sky.
const std::array<uint8_t, kRampRows * kRampSize>& rollRamp() {
    static const std::array<uint8_t, kRampRows * kRampSize> table = [] {
        std::array<uint8_t, kRampRows * kRampSize> ramp{};
        size_t at = 0;
        for (int row = 1; row <= kRampRows; ++row) {
            int accumulator = -32, count = 0;
            for (int i = 0; i < kRampSize; ++i) {
                accumulator += row * 2;
                if (accumulator >= 32) {
                    ++count;
                    accumulator -= 64;
                }
                ramp[at++] = static_cast<uint8_t>(count);
            }
        }
        return ramp;
    }();
    return table;
}

// StrafeAngleTbl: the heading that the Y button moves along, for the directions that are held (right 1, left 2, down 4, up 8).
constexpr int kStrafeAngles[11] = {0, 270, 90, 0, 180, 225, 135, 0, 0, 315, 45};

}  // namespace

// The memory of the picture processor for the flights: the airship and its propellers, the sky of the second layer.
void WorldScreen::loadFlightGraphics() {
    loadGraphics();
    if (vehicle_ == Vehicle::Chocobo) {  // the chocobo takes the place of the airship, and a sheet of sprites that goes with it
        for (size_t i = 0; i + 1 < data_.chocoboGfx[0].size(); i += 2) ppu_.vram[0x6400 + i / 2] = static_cast<uint16_t>(data_.chocoboGfx[0][i] | (data_.chocoboGfx[0][i + 1] << 8));
        for (size_t i = 0; i + 1 < data_.chocoboGfx[1].size(); i += 2) ppu_.vram[0x7400 + i / 2] = static_cast<uint16_t>(data_.chocoboGfx[1][i] | (data_.chocoboGfx[1][i + 1] << 8));
    }
    windowX_ = windowY_ = -1;
    ppu_.bgsc = {0x40, 0x45, 0};  // the first layer is the map and has nothing in the sky, the second is the sky: 64 x 32 tiles
    ppu_.nba12 = 0x55;
    if (vehicle_ == Vehicle::Trench) return;  // under the water there is no sky
    for (size_t i = 0; i + 1 < data_.backdropTiles.size(); i += 2) ppu_.vram[static_cast<size_t>(kVramBackdropMap) + i / 2] = static_cast<uint16_t>(data_.backdropTiles[i] | (data_.backdropTiles[i + 1] << 8));
    for (size_t i = 0; i + 1 < data_.backdropGfx.size(); i += 2) ppu_.vram[static_cast<size_t>(kVramBackdropGfx) + i / 2] = static_cast<uint16_t>(data_.backdropGfx[i] | (data_.backdropGfx[i + 1] << 8));
}

void WorldScreen::hideSprites(size_t first, size_t count) {
    for (size_t i = first; i < first + count && i < ppu_.sprites.size(); ++i) ppu_.sprites[i].y = 0xE0;
}

// DrawVehicleSprites: the sprites of a frame of the animations, placed from a point.
void WorldScreen::drawAnimFrame(int frame, int x, int y, size_t firstSprite, size_t count) {
    hideSprites(firstSprite, count);
    if (frame <= 0 || static_cast<size_t>(frame) >= data_.animPointers.size()) return;
    const std::vector<uint8_t>& bytes = data_.animData;
    size_t at = data_.animPointers[static_cast<size_t>(frame)];
    if (at >= bytes.size()) return;
    const int number = bytes[at++];
    for (int i = 0; i < number && i < static_cast<int>(count) && at + 4 <= bytes.size(); ++i, at += 4) {
        PpuSprite& sprite = ppu_.sprites[firstSprite + static_cast<size_t>(i)];
        sprite.x = (static_cast<int8_t>(bytes[at]) + x) & 0xFF;
        sprite.y = (static_cast<int8_t>(bytes[at + 1]) + y) & 0xFF;
        sprite.tile = bytes[at + 2] | ((bytes[at + 3] & 1) << 8);
        sprite.attr = static_cast<uint8_t>(bytes[at + 3] & 0xFE);
        sprite.large = false;
    }
}

// UpdateSpriteAnim_03: when the airship is parked in front of the party, it shows on the ground, smaller the farther it is.
void WorldScreen::drawParkedAirship() {
    hideSprites(kParkedFirstSprite, ppu_.sprites.size() - kParkedFirstSprite);
    if (!(hooks_.bit && hooks_.bit(FF6_EVENT_BIT_AIRSHIP_VISIBLE))) return;
    const int dy = airshipY_ * 16 - pixelY();
    if (dy < -122 || dy >= 0x38) return;
    const int scale = kParkedTable[dy + 0x7A];
    int dx = airshipX_ * 16 - pixelX() - 8;
    const bool left = dx < 0;
    if (left) dx = -dx;
    const int product = (dx & 0xFF) * 0xC7;
    const int stretched = ((scale * (product & 0xFF)) >> 8) + scale * (product >> 8);
    const int offset = ((stretched / 0xEB) & 0xFF) + dx;
    if (offset >= 0xA0) return;
    const int x = (left ? -offset : offset) + 0x80;
    const int frame = scale / 0x3C + (world_ == 0 ? 0x45 : 0x62);
    drawAnimFrame(frame, x, scale - 8, kParkedFirstSprite, ppu_.sprites.size() - kParkedFirstSprite);
}

// The airship that flies: it hovers at the middle of the screen, higher when the altitude is, with its shadow below.
void WorldScreen::drawAirship() {
    const Flight& flight = flight_;
    const int pitch = flight.climb >= 0xAA ? 1 : flight.climb < -0xAA ? 2 : 0;
    const int turn = flight.rate >= 0xAA ? 9 : flight.rate < -0xAA ? 5 : 1;
    const int frame = kDirAnimOffset[turn + pitch] + (shipFrame_ & 1);
    const int altitudeHigh = flight.altitude >> 8;
    drawAnimFrame(frame, 128, 128 - ((altitudeHigh * 0x51) >> 8), kShipFirstSprite, ppu_.sprites.size() - kShipFirstSprite);
    const int groundProp = static_cast<int>(propAt(flight.x >> 12, flight.y >> 12));
    const int shadow = (((altitudeHigh * 0x10) >> 8) & 0xFE) + 0x28 - ((groundProp & 0x0C) >> 1);
    for (int half = 0; half < 2; ++half) {
        PpuSprite& sprite = ppu_.sprites[static_cast<size_t>(kShadowFirstSprite + half)];
        sprite.x = 0x74 + 8 * half;
        sprite.y = 0xC8;
        sprite.tile = shadow;
        sprite.attr = static_cast<uint8_t>(half == 0 ? 0x10 : 0x50);
        sprite.large = false;
    }
}

// UpdateSpriteAnim_02: the chocobo runs on the spot at the middle of the screen, faster as it goes faster.
void WorldScreen::drawChocobo() {
    const Flight& f = flight_;
    const int turn = f.rate >= 0xAA ? 6 : f.rate < -0xAA ? 12 : 0;
    drawAnimFrame(chocoboFrame_ + turn, 128, 184, kShipFirstSprite, ppu_.sprites.size() - kShipFirstSprite);
    hideSprites(kShadowFirstSprite, 2);
}

// The arrows of the serpent trench blink at the side that the party has chosen: left or right.
void WorldScreen::drawArrows() {
    hideSprites(kArrowFirstSprite, ppu_.sprites.size() - kArrowFirstSprite);
    if (!arrowsShown_) return;
    const bool left = hooks_.bit && hooks_.bit(FF6_EVENT_BIT_TRENCH_LEFT);
    const int frame = arrowCounter_ == 0 ? 0 : arrowCounter_ + (left ? 0x41 : 0x3E);
    drawAnimFrame(frame, left ? 0x60 : 0x98, (0xFF - flight_.horizon) & 0xFF, kArrowFirstSprite, ppu_.sprites.size() - kArrowFirstSprite);
}

// B: the chocobo goes, and the party stands on the tile; on the veldt, the battles come back.
void WorldScreen::dismount() {
    const int prop = static_cast<int>(propAt(flight_.x >> 12, flight_.y >> 12));
    if ((prop & kVeldtTile) && hooks_.setBit) hooks_.setBit(FF6_EVENT_BIT_VELDT, true);
    flight_.speed = flight_.rate = 0;
    mode_ = Mode::Dismount;
    targetBrightness_ = 0;
}

// StartFlight: InitAirship, with the party over the tile that it was on.
void WorldScreen::startFlight(int angle, int altitude, bool tween) {
    flight_ = Flight();
    flight_.angle = (angle % kDegrees) << 8;
    flight_.moveAngle = angle % kDegrees;
    flight_.altitude = altitude;
    // The chocobo stands in the middle of its tile, the airship and the boat at its corner.
    const int center = vehicle_ == Vehicle::Chocobo ? 8 : 0;
    flight_.x = (pixelX() + center) << 8;
    flight_.y = (pixelY() + center) << 8;
    hidden_ = true;
    loadFlightGraphics();
    if (tween) {  // AirshipLiftOffAnim2
        mode_ = Mode::TweenIn;
        camera_ = Camera();
        tween_ = 0;
        brightness_ = 0;
    } else {
        mode_ = Mode::Fly;
        brightness_ = 15;
        moveFlight(InputState());
    }
    targetBrightness_ = 15;
    shipFrame_ = 0;
    shipCounter_ = shipRate_ = 1;
    chocoboFrame_ = 0x25;
    buildWindow();
    playVehicleMusic();
}

void WorldScreen::enterFlying(int world, int tileX, int tileY, int facing, const Lead& lead, int angle, int altitude) {
    enter(world, tileX, tileY, facing, lead);
    vehicle_ = Vehicle::Airship;
    startFlight(angle, altitude, false);
}

// InitChoco: it starts facing north or south, whichever side the party looked at.
void WorldScreen::enterRiding(int world, int tileX, int tileY, int facing, const Lead& lead) {
    enter(world, tileX, tileY, facing, lead);
    vehicle_ = Vehicle::Chocobo;
    startFlight(facing >= 2 ? 180 : 0, 0, false);
}

// InitSnakeRoad.
void WorldScreen::enterTrench(int tileX, int tileY, int facing, const Lead& lead) {
    enter(FF6_TRENCH_MAP, tileX, tileY, facing, lead);
    vehicle_ = Vehicle::Trench;
    minimapShown_ = false;
    startFlight(0, 0, false);
}

// The song of the vehicle, unless the story asked for the one that plays to go on.
void WorldScreen::playVehicleMusic() {
    if (hooks_.bit && hooks_.bit(FF6_EVENT_BIT_KEEP_SONG)) return;
    int song = world_ == 0 ? FF6_SONG_BLACKJACK : FF6_SONG_SEARCHING_FOR_FRIENDS;
    if (vehicle_ == Vehicle::Chocobo) song = FF6_SONG_TECHNO_DE_CHOCOBO;
    if (vehicle_ == Vehicle::Trench) song = FF6_SONG_SERPENT_TRENCH;
    if (vehicle_ != Vehicle::Trench && hooks_.bit && hooks_.bit(FF6_EVENT_BIT_VELDT)) song = FF6_SONG_VELDT;
    sound_.command(FF6_SOUND_PLAY_SONG, static_cast<uint8_t>(song), FF6_SOUND_FULL_VOLUME);
}

// The party gets on board when it faces the airship that stands where it is: in the story it is a scene, else it takes off.
void WorldScreen::boardAirship() {
    if (hooks_.bit && hooks_.bit(FF6_EVENT_BIT_AIRSHIP_GROUND)) {
        startEvent(kGroundEvent);
        menuLocked_ = true;
        return;
    }
    mode_ = Mode::TakeoffFade;
    hidden_ = true;
    targetBrightness_ = 0;
    flight_ = Flight();
    airshipX_ = tileX();
    airshipY_ = tileY();
    menuLocked_ = true;
}

// LandAirship: where the ground is not water or mountains the airship goes down.
void WorldScreen::landAirship() {
    const int prop = static_cast<int>(propAt(flight_.x >> 12, flight_.y >> 12));
    if (prop & kCannotLand) return;
    if (world_ == 1 && (prop & kKefkaTowerTile)) {
        startEvent(kKefkaTowerEvent);
        return;
    }
    if (world_ == 1 && (prop & kPhoenixCaveTile)) {
        startEvent(kPhoenixCaveEvent);
        return;
    }
    if ((prop & kVeldtTile) && hooks_.setBit) hooks_.setBit(FF6_EVENT_BIT_VELDT, true);
    flight_.landing = true;
    flight_.speed = flight_.rate = 0;
}

// The airship is on the ground: the party stands where it landed, the airship stays there.
void WorldScreen::finishLanding() {
    const bool airship = vehicle_ == Vehicle::Airship;
    mode_ = Mode::Walk;
    vehicle_ = Vehicle::Airship;
    hidden_ = false;
    const int tileX = flight_.x >> 12, tileY = flight_.y >> 12;
    if (airship) {
        airshipX_ = tileX;
        airshipY_ = tileY;
    }
    dfx_ = static_cast<uint16_t>(tileX << 8);
    dfy_ = static_cast<uint16_t>(tileY << 8);
    speedX_ = speedY_ = 0;
    sectorX_ = tileX;
    sectorY_ = tileY;
    pivot_ = kFlightPivot;
    playMusic();
    battleChecked_ = true;
    menuLocked_ = false;
    loadGraphics();
    windowX_ = windowY_ = -1;
    buildWindow();
    setCharacterSprites();
    brightness_ = 0;
    targetBrightness_ = 15;
}

// UpdateVehiclePos: one frame of movement along a heading, at the speed of the vehicle.
void WorldScreen::advance(int degrees) {
    Flight& f = flight_;
    const int base = degrees >= 180 ? degrees - 180 : degrees;
    const int sine = sineMagnitude(base), cosine = sineMagnitude(base + 90);
    const int north = (degrees < 90 || degrees >= 270) ? -1 : 1, west = degrees < 180 ? -1 : 1;
    const int mask = mapSize() * 16 * 256 - 1;
    f.y = (f.y + north * ((cosine * f.speed) >> 8)) & mask;
    f.x = (f.x + west * ((sine * f.speed) >> 8)) & mask;
}

// The chocobo cannot go over the tiles that have the first bit of their properties.
bool WorldScreen::chocoboBlocked() const { return (propAt(flight_.x >> 12, flight_.y >> 12) & 1) != 0; }

// FixChocoboCollision: once the chocobo is back where it was, it tries to slide along the wall, to the side that the
// heading is closer to first.
void WorldScreen::fixChocoboCollision(int heading) {
    if (heading % 90 == 0) return;
    const int savedX = flight_.x, savedY = flight_.y;
    const auto wrap = [](int degrees) { return ((degrees % kDegrees) + kDegrees) % kDegrees; };
    const auto slide = [&](int degrees) {
        advance(degrees);
        if (!chocoboBlocked()) return true;
        flight_.x = savedX;
        flight_.y = savedY;
        return false;
    };
    const bool plusFirst = heading < 45 || (heading >= 90 && heading < 135) || (heading >= 180 && heading < 225) || (heading >= 270 && heading < 315);
    if (slide(wrap(heading + (plusFirst ? 90 : -90)))) return;
    slide(wrap(heading + (plusFirst ? -90 : 90)));
}

// GetVehicleInput and MoveVehicle: the controls change the speed, the turn and the climb, which move the airship.
void WorldScreen::moveFlight(const InputState& input) {
    Flight& f = flight_;
    const bool chocobo = vehicle_ == Vehicle::Chocobo;
    if (input.start && !eventRunning_) minimapShown_ = !minimapShown_;
    autoHeld_ = 0;
    if (eventRunning_) execVehicleCmd();
    const unsigned held = eventRunning_ ? autoHeld_ : input.held;
    if (arrowsInput_ && hooks_.setBit) {  // the choice at the forks of the trench
        if (input.held & FF6_HELD_RIGHT) {
            hooks_.setBit(FF6_EVENT_BIT_TRENCH_LEFT, false);
        } else if (input.held & FF6_HELD_LEFT) {
            hooks_.setBit(FF6_EVENT_BIT_TRENCH_LEFT, true);
        }
    }
    if (arrowsShown_ && ++arrowTick_ >= 4) {
        arrowTick_ = 0;
        arrowCounter_ = (arrowCounter_ + 1) & 3;
    }
    const bool controlled = !f.landing && !entranceReady_ && !(f.flags & kFlagNoInput);
    if (controlled && chocobo) {  // GetChocoInput: no climb, and it runs at the pace of a walk
        if (held & FF6_HELD_RIGHT) {
            if (f.rate - 0x20 >= -kMaxRate) f.rate -= 0x20;
        } else if (held & FF6_HELD_LEFT) {
            if (f.rate + 0x20 <= kMaxRate) f.rate += 0x20;
        } else {
            f.rate += f.rate > 0 ? -0x20 : f.rate < 0 ? 0x20 : 0;
        }
        if (held & (FF6_HELD_UP | FF6_HELD_A)) {
            if (f.speed + 0x40 <= kChocoboSpeed) f.speed += 0x40;
        } else {
            f.speed = std::max(0, f.speed - 0x80);
        }
        if ((held & FF6_HELD_L) && (held & FF6_HELD_LEFT)) f.angle += 2 << 8;
        if ((held & FF6_HELD_R) && (held & FF6_HELD_RIGHT)) f.angle -= 2 << 8;
        if (!eventRunning_ && input.cancel) dismount();
    } else if (controlled) {
        if (held & FF6_HELD_Y) {  // the Y button: the directions move the airship sideways while it slows down
            const unsigned directions = ((held & FF6_HELD_RIGHT) ? 1u : 0u) | ((held & FF6_HELD_LEFT) ? 2u : 0u) | ((held & FF6_HELD_DOWN) ? 4u : 0u) |
                                        ((held & FF6_HELD_UP) ? 8u : 0u);
            if (directions >= 1 && directions <= 10) {
                const int heading = (kStrafeAngles[directions] + (f.angle >> 8)) % kDegrees;
                const int part = heading >= 180 ? heading - 180 : heading;
                const int mask = mapSize() * 16 * 256 - 1;
                f.y = (f.y + ((heading < 90 || heading >= 270) ? -1 : 1) * sineMagnitude(part + 90) * 2) & mask;
                f.x = (f.x + (heading < 180 ? -1 : 1) * sineMagnitude(part) * 2) & mask;
            }
            f.rate += f.rate > 0 ? -0x10 : f.rate < 0 ? 0x10 : 0;
            if (f.climb >= 0x10) f.climb -= 0x10;
            else if (f.climb < 0) f.climb += 0x10;
            f.speed = std::max(0, f.speed - 0x80);
        } else {
            if (held & FF6_HELD_RIGHT) {
                if (f.rate - 0x20 >= -kMaxRate) f.rate -= 0x20;
            } else if (held & FF6_HELD_LEFT) {
                if (f.rate + 0x20 <= kMaxRate) f.rate += 0x20;
            } else if (f.rate > 0) {
                f.rate -= 0x10;
            } else if (f.rate < 0) {
                f.rate += 0x10;
            }
            if (held & FF6_HELD_A) {
                if (f.speed + 0x40 <= kMaxSpeed) f.speed += 0x40;
            } else {
                f.speed = std::max(0, f.speed - 0x80);
            }
            if (held & FF6_HELD_UP) {
                if (f.climb - 0x20 >= -kMaxRate) f.climb -= 0x20;
            } else if (held & FF6_HELD_DOWN) {
                if (f.climb + 0x20 <= kMaxRate) f.climb += 0x20;
            } else if (f.climb >= 0x10) {
                f.climb -= 0x10;
            } else if (f.climb < 0) {
                f.climb += 0x10;
            }
            if ((held & FF6_HELD_L) && (held & FF6_HELD_LEFT)) f.angle += 2 << 8;
            if ((held & FF6_HELD_R) && (held & FF6_HELD_RIGHT)) f.angle -= 2 << 8;
            if (!eventRunning_ && input.cancel && vehicle_ == Vehicle::Airship) landAirship();
        }
        if (!eventRunning_ && !f.landing && input.menu && vehicle_ == Vehicle::Airship) {
            deck_ = true;
            startEvent(kDeckEvent);
        }
    }
    // Doom Gaze: holding the engines over one of its places in the world of ruin brings the battle.
    if (vehicle_ == Vehicle::Airship && world_ == 1 && doomGaze_.active && (input.held & FF6_HELD_A) && !eventRunning_ && !f.landing && !doomGazeReady_) {
        const int cellX = (f.x >> 14) & 0x3F, cellY = (f.y >> 14) & 0x3F;
        for (const std::array<int, 2>& cell : doomGaze_.cells) {
            if (cell[0] != cellX || cell[1] != cellY) continue;
            f.speed = f.climb = 0;
            doomGazeReady_ = true;
            break;
        }
    }
    // The turn, then the movement along the heading, which a script can keep apart from the way that the camera looks.
    if (!(f.flags & kFlagNoTurn)) {
        f.angle += f.rate;
        if (f.angle < 0) f.angle += kDegrees << 8;
        if (f.angle >= kDegrees << 8) f.angle -= kDegrees << 8;
    }
    const int heading = (f.flags & kFlagMoveAngle) ? f.moveAngle : f.angle >> 8;
    const int savedX = f.x, savedY = f.y;
    advance(heading);
    if (chocobo && chocoboBlocked()) {
        f.x = savedX;
        f.y = savedY;
        fixChocoboCollision(heading);
    }
    dfx_ = static_cast<uint16_t>((f.x >> 12) << 8);
    dfy_ = static_cast<uint16_t>((f.y >> 12) << 8);
    const int altitude = f.altitude + f.climb;
    if (!(f.flags & kFlagNoAltitude) && altitude >= 0 && altitude <= kMaxAltitude) f.altitude = altitude;
    if (f.landing) {  // _ee1c56: the airship goes down at the pace of the climb, which it slows to a stop
        if (f.altitude + f.climb < 0 || f.altitude == 0) {
            f.climb = 0;
            mode_ = Mode::TweenOut;
            tween_ = kTweenSteps - 1;
            camera_ = Camera();
            camera_.scale = 0x3000;
            camera_.divStart = 0x1800;
            camera_.divEnd = 0xFEFF;
            camera_.horizon = 0x92;
            camera_.pivot = kFlightPivot;
            camera_.lines = 0x92;
            targetBrightness_ = 0;
            minimapShown_ = false;
        } else if (f.climb - 0x20 >= -kMaxRate) {
            f.climb -= 0x20;
        }
    }
    // The camera: the scale and the divisor follow the altitude, the horizon moves with the climb and the turn.
    camera_.scale = f.altitude + 0x3000;
    camera_.divStart = (f.altitude >> 8) * 0x4F + 0x1800;
    camera_.divEnd = 0xFF00;
    camera_.pivot = pivot_;
    if (!(f.flags & kFlagNoHorizon)) {
        const int climbPart = (((((absNot(f.climb) >> 3) * (f.speed >> 4)) >> 8) & 0xFF) * 0xF0) >> 8;
        f.horizon = f.climb >= 0 ? 0x92 - climbPart : 0x92 + climbPart;
    }
    if (!(f.flags & kFlagNoRotation)) {
        const int turnPart = (((((absNot(f.rate) >> 3) * (f.speed >> 4)) >> 8) & 0xFF) * 0xF0) >> 8;
        f.rotation = f.rate >= 0 ? -turnPart : turnPart;
    }
    camera_.lines = (absNot(f.rotation) >> 1) + f.horizon;
    if (mode_ == Mode::TweenOut) camera_.lines = camera_.horizon;
}

// ExecVehicleCmd: the scripts that run while the airship flies. The first commands fly it by themselves for a number of
// frames, as if the buttons were held; the others are the jumps, the event bits, the position of the airship and the maps.
void WorldScreen::execVehicleCmd() {
    for (int guard = 0; guard < 0x10000 && eventRunning_; ++guard) {
        const int cmd = inCommand_ ? command_ : scriptByte();
        command_ = cmd;
        if (cmd < 0x80) {  // b0: -dulrfkt: down, up, left, right, forward, back, sharp turns
            if (!inCommand_) {
                commandCount_ = scriptByte();
                inCommand_ = true;
            }
            autoHeld_ = 0;
            if (cmd & 0x01) autoHeld_ |= FF6_HELD_L | FF6_HELD_R;
            if (cmd & 0x04) autoHeld_ |= FF6_HELD_A;
            if (cmd & 0x08) autoHeld_ |= FF6_HELD_RIGHT;
            if (cmd & 0x10) autoHeld_ |= FF6_HELD_LEFT;
            if (cmd & 0x20) autoHeld_ |= FF6_HELD_DOWN;  // up in the air is down on the pad
            if (cmd & 0x40) autoHeld_ |= FF6_HELD_UP;
            if (commandCount_ == 0) {
                inCommand_ = false;
            } else {
                --commandCount_;
            }
            return;
        }
        if (cmd < 0xC0) {  // conditional jump
            const int count = (cmd & 7) + 1;
            std::vector<int> conditions;
            for (int i = 0; i < count; ++i) conditions.push_back(scriptWord());
            const uint32_t target = static_cast<uint32_t>(scriptByte() | (scriptByte() << 8) | (scriptByte() << 16));
            bool jump = cmd >= 0xB8;
            for (const int condition : conditions) jump = cmd >= 0xB8 ? jump && conditionMet(condition) : jump || conditionMet(condition);
            if (jump) jumpTo(target);
            continue;
        }
        switch (cmd) {
            case 0xC0: flight_.flags = scriptByte(); break;
            case 0xC1: flight_.angle = (scriptWord() % kDegrees) << 8; break;
            case 0xC2: flight_.moveAngle = scriptWord() % kDegrees; break;
            case 0xC3: flight_.rotation = static_cast<int16_t>(scriptWord()); break;
            case 0xC4: flight_.horizon = scriptWord(); break;
            case 0xC5: flight_.altitude = scriptWord(); break;
            case 0xC6: flight_.speed = scriptWord(); break;
            case 0xC7:
                airshipX_ = scriptByte();
                airshipY_ = scriptByte();
                break;
            case 0xCA:
            case 0xCB:
            case 0xCC:
            case 0xCD:
            case 0xCE:
            case 0xCF:  // a battle: the game picks the formation of the group, and goes on once it is over
                scriptBattleGroup_ = scriptByte();
                scriptBattleBackground_ = scriptByte();
                scriptBattleReady_ = true;
                return;
            case 0xD0: vehicleHidden_ = false; break;
            case 0xD1: vehicleHidden_ = true; break;
            case 0xD4:
            case 0xD5:
            case 0xD6:
            case 0xD7:
            case 0xD8: targetBrightness_ = 15; break;
            case 0xD9: targetBrightness_ = 0; break;
            case 0xDA:
                arrowsShown_ = arrowsInput_ = true;
                arrowCounter_ = 0;
                break;
            case 0xDB: arrowsInput_ = false; break;
            case 0xDC: arrowsShown_ = false; break;
            case 0xDD:
                minimapShown_ = false;
                return;
            case 0xDE:
                pivotX_ = scriptByte();
                pivot_ = scriptByte();
                break;
            case 0xDF: minimapShown_ = true; break;
            case 0xE0: {  // pause: the first call reads the duration, then it counts frames
                if (commandCount_ == 0) {
                    commandCount_ = scriptByte();
                    pauseCounter_ = 15;
                    inCommand_ = true;
                }
                if (--pauseCounter_ == 0) {
                    if (--commandCount_ == 0) {
                        inCommand_ = false;
                    } else {
                        pauseCounter_ = 4;
                    }
                }
                return;
            }
            case 0xC8:
            case 0xC9: {
                const int bit = scriptWord();
                if (hooks_.setBit) hooks_.setBit(bit & 0x7FFF, cmd == 0xC8);
                break;
            }
            case 0xD2:
            case 0xD3: loadMapCommand(); return;
            case 0xFE: break;
            case 0xFF:
                eventRunning_ = false;
                storyScript_ = false;
                menuLocked_ = false;
                return;
            default:
                std::fprintf(stderr, "airship event: unsupported command 0x%02X, event ended\n", cmd);
                eventRunning_ = false;
                storyScript_ = false;
                menuLocked_ = false;
                return;
        }
    }
}

// The frames of the flight: the transitions move the camera along a table, the flight itself is the controls.
void WorldScreen::tickFlight(const InputState& input) {
    if (mode_ == Mode::TakeoffFade) {
        if (brightness_ > 0) --brightness_;
        updateWater();
        if (brightness_ == 0) startFlight(0, 0, true);
        return;
    }
    if (mode_ == Mode::Dismount) {
        if (brightness_ > 0) --brightness_;
        flight_.speed = 0;
        if (brightness_ == 0) finishLanding();
        updateWater();
        return;
    }
    if (mode_ == Mode::TweenIn || mode_ == Mode::TweenOut) {
        const bool in = mode_ == Mode::TweenIn;
        const uint8_t* row = kTween[tween_];
        const int scale = row[0] | (row[1] << 8), divisor = row[2] | (row[3] << 8);
        camera_.scale = (camera_.scale + (in ? -scale : scale)) & 0xFFFF;
        camera_.divStart = (camera_.divStart + (in ? -divisor : divisor)) & 0xFFFF;
        camera_.divEnd = (camera_.divEnd + (in ? -row[4] : row[4])) & 0xFFFF;
        camera_.horizon += in ? -row[5] : row[5];
        camera_.pivot += in ? row[5] : -row[6];
        camera_.lines = std::min(camera_.horizon, 0xE0);
        flight_.rotation = 0;
        if (in) {
            if (++tween_ == kTweenSteps) {
                mode_ = Mode::Fly;
                moveFlight(InputState());
            }
        } else if (--tween_ < 0) {
            finishLanding();
        }
    } else {
        moveFlight(input);
    }
    if (mode_ != Mode::Walk && vehicle_ == Vehicle::Chocobo) {  // the steps of the chocobo are quicker the faster it goes
        if (flight_.rate == 0 && flight_.speed == 0) {
            chocoboFrame_ = 0x25;
        } else {
            const int period = 7 - (flight_.rate != 0 ? 0 : (flight_.speed * 4) >> 8);
            if (--shipCounter_ < 0) {
                if (++chocoboFrame_ >= 0x19) chocoboFrame_ = 0x13;
                shipCounter_ = period;
            }
        }
    } else if (mode_ != Mode::Walk) {  // the propellers turn faster the faster the airship goes
        const int rate = ((8 - (flight_.speed >> 8)) >> 1) + 1;
        if (--shipCounter_ <= 0) {
            --shipFrame_;
            shipCounter_ = rate;
        }
    }
    if (mode_ != Mode::Walk) {
        if (vehicle_ != Vehicle::Trench) updateWater();
        buildWindow();
        if (brightness_ < targetBrightness_) ++brightness_;
        if (brightness_ > targetBrightness_) --brightness_;
    }
}

// UpdateMode7Rot: the matrix of every line is the scale of the perspective turned by the heading; the lines above the
// horizon are the sky.
void WorldScreen::renderFlight() {
    const int mapLines = std::clamp(camera_.lines, 0, FF6_SCREEN_HEIGHT);
    const int skyLines = FF6_SCREEN_HEIGHT - mapLines;
    const int degrees = flight_.angle >> 8;
    const int base = degrees >= 180 ? degrees - 180 : degrees;
    const int sine = sineMagnitude(base), cosine = sineMagnitude(base + 90);
    const int sineSign = degrees < 180 ? 1 : -1, cosineSign = (degrees >= 90 && degrees < 270) ? -1 : 1;
    const int step = mapLines > 0 ? std::abs(camera_.divEnd - camera_.divStart) / mapLines : 0;
    // $70: the sky turns at the pace of the heading
    const int low = degrees & 0xFF, high = degrees >> 8;
    bg2Scroll_ = 0x1FF - ((((0x6C * low) >> 8) + 0x6C * high + degrees) * 2 & 0x1FF);
    // The sky tilts with the turn: each 8 lines slide sideways and each column of 8 pixels moves up or down.
    const int tilt = std::abs(flight_.rotation), rampStart = std::min(tilt, kRampRows - 1) * kRampSize;
    const std::array<uint8_t, kRampRows * kRampSize>& ramp = rollRamp();
    const int skyScroll = camera_.lines - 0x64 - (flight_.rotation > 0 ? flight_.rotation : 0);
    std::array<uint8_t, 33> columns{};
    for (size_t column = 1; column < columns.size(); ++column) {
        const int offset = ramp[static_cast<size_t>(std::min(rampStart + static_cast<int>(column) - 1, kRampRows * kRampSize - 1))];
        columns[column] = static_cast<uint8_t>(skyScroll + (flight_.rotation > 0 ? offset : -offset));
    }
    int divisor = camera_.divStart;
    for (int y = 0; y < FF6_SCREEN_HEIGHT; ++y) {
        PpuLine& line = ppu_.lines[static_cast<size_t>(y)];
        line.cgwsel = 2;  // the mini-map is on the sub screen, added to the map
        line.cgadsub = 0x23;
        line.fixed = {0, 0, 0};
        if (y < skyLines) {
            line.bgMode = 2;  // the objects are in front of the sky
            const int slide = tilt == 0 ? 0 : ramp[static_cast<size_t>(std::min(rampStart + (y >> 3), kRampRows * kRampSize - 1))];
            line.hofs = {0, static_cast<uint16_t>((bg2Scroll_ + (flight_.rotation > 0 ? -slide : slide)) & 0x1FF), 0};
            line.vofs = {0, static_cast<uint16_t>(skyScroll & 0xFF), 0};
            line.bg2Columns = tilt != 0;
            line.bg2ColumnVofs = columns;
            continue;
        }
        const int quotient = camera_.scale / std::max(1, divisor >> 8);
        divisor = (divisor + step) & 0xFFFF;
        line.bgMode = 7;
        line.bg2Columns = false;
        line.m7a = line.m7d = static_cast<int16_t>(cosineSign * ((quotient * cosine) >> 8));
        line.m7b = static_cast<int16_t>(sineSign * ((quotient * sine) >> 8));
        line.m7c = static_cast<int16_t>(-line.m7b);
    }
    ppu_.m7x = static_cast<uint16_t>((flight_.x >> 8) & (mapSize() * 16 - 1));
    ppu_.m7y = static_cast<uint16_t>((flight_.y >> 8) & (mapSize() * 16 - 1));
    ppu_.m7hofs = static_cast<uint16_t>((ppu_.m7x - pivotX_) & 0x1FFF);
    ppu_.m7vofs = static_cast<uint16_t>((ppu_.m7y - camera_.pivot) & 0x1FFF);
    drawMinimap();
    if (vehicle_ == Vehicle::Chocobo) {
        drawChocobo();
    } else {
        drawAirship();
        if (vehicleHidden_) {
            hideSprites(kShadowFirstSprite, 2);
            hideSprites(kShipFirstSprite, kArrowFirstSprite - kShipFirstSprite);
        }
    }
    drawArrows();
    ppu_.tm = 0x13;
    ppu_.ts = 0x10;
}

}  // namespace ff6
