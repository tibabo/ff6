#include "field/player.hpp"

#include "field/sprite_data.hpp"

namespace ff6 {
namespace {

// DirectionBitTbl: passability bits in the high byte of the current tile.
constexpr uint8_t kDirectionBits[FF6_DIRECTION_COUNT] = {FF6_PASS_UP, FF6_PASS_RIGHT, FF6_PASS_DOWN, FF6_PASS_LEFT};

int wrap(int value, int size) { return ((value % size) + size) % size; }

}  // namespace

bool Player::canMove(const MapData& map, int fromX, int fromY, int direction, int zLevel) {
    const uint16_t current = map.propAt(fromX, fromY);
    const uint8_t currentLow = current & FF6_PROP_LOW_MASK;
    if (((current >> FF6_PROP_HIGH_SHIFT) & FF6_PROP_PASSABILITY_MASK & kDirectionBits[direction]) == 0) return false;
    const uint8_t destination = map.propAt(fromX + kDeltaX[direction], fromY + kDeltaY[direction]) & FF6_PROP_LOW_MASK;
    if ((destination & FF6_TILE_TYPE_MASK) == FF6_TILE_IMPASSABLE) return false;  // counter or wall
    if (currentLow & FF6_TILE_BRIDGE) {
        return (zLevel & FF6_TILE_LEVEL_1) ? (destination & FF6_TILE_LEVEL_2) == 0
                                           : (destination & FF6_TILE_LEVEL_1) == 0;
    }
    if ((destination & FF6_TILE_LEVEL_MASK) == FF6_TILE_LEVEL_MASK) return true;
    if ((currentLow & FF6_TILE_LEVEL_MASK) == FF6_TILE_LEVEL_MASK) return (destination & FF6_TILE_BRIDGE) == 0;
    return (((currentLow & FF6_TILE_LEVEL_MASK) ^ FF6_TILE_LEVEL_MASK) & destination) == 0;
}

void Player::place(const MapData& map, int tileX, int tileY, int facing) {
    tileX_ = tileX;
    tileY_ = tileY;
    pixelX_ = tileX * FF6_TILE_SIZE;
    pixelY_ = tileY * FF6_TILE_SIZE;
    facing_ = facing;
    remaining_ = 0;
    direction_ = FF6_NO_DIRECTION;
    const uint8_t low = map.propAt(tileX, tileY) & FF6_PROP_LOW_MASK;
    zLevel_ = (low & FF6_TILE_BRIDGE) ? FF6_Z_LEVEL_1 : (low & FF6_TILE_LEVEL_MASK);
    topAbove_ = false;
    bottomAbove_ = false;
    applyCurrentTilePriority(map);
}

void Player::applyCurrentTilePriority(const MapData& map) {
    const uint8_t low = map.propAt(tileX_, tileY_) & FF6_PROP_LOW_MASK;
    if (low & FF6_TILE_BRIDGE) {
        if (zLevel_ != FF6_Z_LEVEL_1) return;
        topAbove_ = true;
    } else if (low & FF6_TILE_PRIORITY_TOP) {
        topAbove_ = true;
    }
    if (low & FF6_TILE_PRIORITY_BOTTOM) bottomAbove_ = true;
}

bool Player::update(const MapData& map, int direction, const Objects& objects) {
    bool started = false;
    if (remaining_ == 0) started = tryStep(map, direction, objects);
    if (remaining_ > 0) {
        pixelX_ += kDeltaX[direction_] * FF6_STEP_PIXELS_PER_FRAME;
        pixelY_ += kDeltaY[direction_] * FF6_STEP_PIXELS_PER_FRAME;
        --remaining_;
    }
    return started;
}

bool Player::tryStep(const MapData& map, int direction, const Objects& objects) {
    applyCurrentTilePriority(map);
    if (direction < 0 || direction >= FF6_DIRECTION_COUNT) return false;
    facing_ = direction;
    if (!canMove(map, tileX_, tileY_, direction, zLevel_)) return false;
    const MapLayout& layout = map.layers[FF6_BG1];
    const int nextX = wrap(tileX_ + kDeltaX[direction], layout.width);
    const int nextY = wrap(tileY_ + kDeltaY[direction], layout.height);
    if (objects.occupied(nextX, nextY)) return false;

    const uint8_t currentLow = map.propAt(tileX_, tileY_) & FF6_PROP_LOW_MASK;
    if ((currentLow & FF6_TILE_TYPE_MASK) < FF6_TILE_LEVEL_MASK) zLevel_ = currentLow & FF6_TILE_LEVEL_MASK;
    tileX_ = nextX;
    tileY_ = nextY;
    const uint8_t destination = map.propAt(tileX_, tileY_) & FF6_PROP_LOW_MASK;
    if (destination & FF6_TILE_BRIDGE) {
        if (zLevel_ == FF6_Z_LEVEL_2) topAbove_ = false;
    } else if (!(destination & FF6_TILE_PRIORITY_TOP)) {
        topAbove_ = false;
    }
    if (!(destination & FF6_TILE_PRIORITY_BOTTOM)) bottomAbove_ = false;
    direction_ = direction;
    remaining_ = FF6_STEP_FRAMES;
    return true;
}

void Player::follow(int pixelX, int pixelY, int facing) {
    pixelX_ = pixelX;
    pixelY_ = pixelY;
    tileX_ = pixelX / FF6_TILE_SIZE;
    tileY_ = pixelY / FF6_TILE_SIZE;
    facing_ = facing;
    remaining_ = 0;
    direction_ = FF6_NO_DIRECTION;
}

int Player::frameIndex() const {
    if (remaining_ == 0) return stopFrame(facing_);
    const int position = (direction_ == Right || direction_ == Left) ? pixelX_ : pixelY_;
    return moveFrame(facing_, position);
}

}  // namespace ff6
