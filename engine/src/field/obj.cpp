#include "field/obj.hpp"

#include <algorithm>

#include "field/sprite_data.hpp"

namespace ff6 {
namespace {

constexpr int kDirections = 16;  // straight and diagonal movement directions
constexpr int kWallLow = 0xF7;  // low byte of a wall tile

// ObjMoveMult: speed multipliers for the speed index.
constexpr int kSpeedMultiplier[FF6_OBJ_SPEED_COUNT] = {1, 2, 4, 8, 16, 32, 16, 8, 4, 2};

// ObjMoveRateH, ObjMoveMaskH, ObjMoveRateV and ObjMoveMaskV with the sign applied: the base speed of each movement
// direction, up, right, down, left, then the diagonals.
constexpr int kRateX[kDirections] = {0,    0x40, 0,     -0x40, 0x40, 0x40,  -0x40, -0x40,
                                     0x20, 0x40, 0x40,  0x20,  -0x20, -0x40, -0x40, -0x20};
constexpr int kRateY[kDirections] = {-0x40, 0,    0x40, 0,    -0x40, 0x40, 0x40, -0x40,
                                     -0x40, -0x20, 0x20, 0x40, 0x40,  0x20, -0x20, -0x40};

// ObjJumpLowTbl then ObjJumpHighTbl: the height above the ground for each value of the jump counter.
constexpr uint8_t kJumpHeight[FF6_OBJ_JUMP_HIGH + 1] = {
    0x02, 0x04, 0x06, 0x08, 0x09, 0x0A, 0x0B, 0x0B, 0x0B, 0x0B, 0x0A, 0x09, 0x08, 0x06, 0x04, 0x02,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0x05, 0x09, 0x0E, 0x11, 0x15, 0x18, 0x1B, 0x1E, 0x20, 0x22, 0x24, 0x26, 0x27, 0x28, 0x29, 0x29,
    0x29, 0x29, 0x28, 0x27, 0x26, 0x24, 0x22, 0x20, 0x1E, 0x1B, 0x18, 0x15, 0x11, 0x0E, 0x09, 0x05};

}  // namespace

NpcProp decodeNpcProp(const uint8_t* bytes) {
    NpcProp prop;
    prop.eventOffset = static_cast<uint32_t>(bytes[0] | (bytes[1] << 8) | ((bytes[2] & FF6_NPC_BANK_MASK) << 16));
    prop.palette = (bytes[2] >> FF6_NPC_PAL_SHIFT) & FF6_NPC_PAL_MASK;
    prop.scrollBg2 = (bytes[2] & FF6_NPC_SCROLL_BG2) != 0;
    prop.switchId = ((bytes[3] << 8) | bytes[2]) >> FF6_NPC_SWITCH_SHIFT;
    prop.x = bytes[4] & FF6_NPC_X_MASK;
    prop.special = (bytes[4] & FF6_NPC_SPECIAL_FLAG) != 0;
    prop.y = bytes[5] & FF6_NPC_Y_MASK;
    prop.speed = bytes[5] >> 6;
    prop.gfx = bytes[6];
    prop.movement = bytes[7] & 0x0F;
    prop.layerPriority = (bytes[8] >> FF6_NPC_LAYER_PRIORITY_SHIFT) & FF6_NPC_LAYER_PRIORITY_MASK;
    prop.facing = bytes[8] & FF6_NPC_DIR_MASK;
    prop.noTurn = (bytes[8] & FF6_NPC_NO_TURN) != 0;
    prop.animFrame = bytes[8] >> FF6_NPC_ANIM_FRAME_SHIFT;
    prop.vehicle = (bytes[7] & FF6_VEHICLE_NPC_MASK) >> FF6_VEHICLE_SHIFT;
    prop.vram = bytes[0] & 0x7F;
    prop.large = (bytes[8] & FF6_NPC_NO_TURN) != 0;
    // The bytes of the event, which a special npc does not have, say how it looks (InitNPCs and DrawObjSpecial).
    prop.frames = (bytes[8] & FF6_NPC_DIR_MASK) + 1;
    prop.speedShift = bytes[7] >> FF6_NPC_SPEED_SHIFT;
    prop.flip = (bytes[0] & 0x80) != 0;
    prop.order = (bytes[7] >> FF6_NPC_ORDER_SHIFT) & FF6_NPC_ORDER_MASK;
    const bool slave = (bytes[2] & 2) != 0, down = (bytes[2] & 1) != 0;
    const int shift = (bytes[1] & 0xE0) >> (slave ? 1 : 4);  // 16 pixels, or 2, for each step
    prop.shiftX = down ? 0 : shift;
    prop.shiftY = down ? shift : 0;
    prop.master = slave ? bytes[1] & 0x1F : -1;
    return prop;
}

bool Npc::topIsAbove() const {
    if (priority == FF6_OBJ_PRIORITY_TOP_ABOVE || priority == FF6_OBJ_PRIORITY_ABOVE) return true;
    return priority == FF6_OBJ_PRIORITY_BELOW ? false : topAbove;
}

bool Npc::bottomIsAbove() const {
    if (priority == FF6_OBJ_PRIORITY_ABOVE) return true;
    if (priority == FF6_OBJ_PRIORITY_TOP_ABOVE || priority == FF6_OBJ_PRIORITY_BELOW) return false;
    return bottomAbove;
}

void Objects::init(const MapData& map, const EventBits& bits) {
    npcs_.clear();
    mapId_ = static_cast<int>(map.id);
    width_ = std::max(1, map.layers[FF6_BG1].width);
    height_ = std::max(1, map.layers[FF6_BG1].height);
    eventRequested_ = false;
    const size_t limit = FF6_LAST_NPC_OBJECT - FF6_FIRST_NPC_OBJECT + 1;
    for (size_t offset = 0; offset + FF6_NPC_PROP_SIZE <= map.npcProps.size() && npcs_.size() < limit;
         offset += FF6_NPC_PROP_SIZE) {
        Npc npc;
        npc.prop = decodeNpcProp(&map.npcProps[offset]);
        npc.eventOffset = npc.prop.eventOffset;
        npc.enabled = npc.visible = bits.get(FF6_NPC_SWITCH_EVENT_BIT + npc.prop.switchId);
        npc.posX = npc.prop.x * FF6_OBJ_TILE_SUBPIXELS;
        npc.posY = npc.prop.y * FF6_OBJ_TILE_SUBPIXELS;
        npc.facing = npc.prop.facing;
        npc.frame = stopFrame(npc.facing);
        npc.speed = npc.prop.speed;
        npc.movement = npc.prop.movement;
        npc.priority = npc.prop.layerPriority;
        npc.passThrough = npc.prop.special;  // InitSpecialNPCGfx
        npc.vehicle = npc.prop.vehicle;
        const uint8_t low = map.propAt(npc.prop.x, npc.prop.y) & FF6_PROP_LOW_MASK;
        npc.topAbove = (low & FF6_TILE_PRIORITY_TOP) != 0;
        npc.bottomAbove = (low & FF6_TILE_PRIORITY_BOTTOM) != 0;
        npcs_.push_back(npc);
    }
}

Npc* Objects::npcByObject(int object) {
    const int index = object - FF6_FIRST_NPC_OBJECT;
    return index >= 0 && static_cast<size_t>(index) < npcs_.size() ? &npcs_[static_cast<size_t>(index)] : nullptr;
}

const Npc* Objects::npcByObject(int object) const { return const_cast<Objects*>(this)->npcByObject(object); }

Npc* Objects::object(int id) {
    if (id >= 0 && id < FF6_CHAR_COUNT) return &chars_[static_cast<size_t>(id)];
    if (id == FF6_OBJ_CAMERA) return &camera_;
    if (id >= FF6_OBJ_PARTY_FIRST && id <= FF6_OBJ_PARTY_LAST) {
        const int character = partySlots_[static_cast<size_t>(id - FF6_OBJ_PARTY_FIRST)];
        return character >= 0 ? &chars_[static_cast<size_t>(character)] : nullptr;
    }
    return npcByObject(id);
}

const Npc* Objects::object(int id) const { return const_cast<Objects*>(this)->object(id); }

bool Objects::acts(const Npc& object) const {
    if (&object == &camera_) return true;
    if (&object >= chars_.data() && &object < chars_.data() + chars_.size()) return object.enabled && object.map == mapId_;
    return object.enabled;
}

int Objects::wrapX(int tile) const { return ((tile % width_) + width_) % width_; }
int Objects::wrapY(int tile) const { return ((tile % height_) + height_) % height_; }

uint32_t Objects::random() {
    seed_ = seed_ * 1664525u + 1013904223u;
    return (seed_ >> 16) & 0xFF;
}

// An object holds the tile it stands on and the tile that it is walking onto.
bool Objects::covers(const Npc& npc, int tileX, int tileY) const {
    const int x0 = npc.tileX(), y0 = npc.tileY();
    const int x1 = x0 + ((npc.posX & (FF6_OBJ_TILE_SUBPIXELS - 1)) != 0 ? 1 : 0);
    const int y1 = y0 + ((npc.posY & (FF6_OBJ_TILE_SUBPIXELS - 1)) != 0 ? 1 : 0);
    return (wrapX(x0) == tileX || wrapX(x1) == tileX) && (wrapY(y0) == tileY || wrapY(y1) == tileY);
}

bool Objects::occupied(int tileX, int tileY) const {
    tileX = wrapX(tileX);
    tileY = wrapY(tileY);
    for (const Npc& npc : npcs_) {
        if (npc.solid() && covers(npc, tileX, tileY)) return true;
    }
    for (const Npc& character : chars_) {
        if (character.blocks() && character.map == mapId_ && covers(character, tileX, tileY)) return true;
    }
    return false;
}

Npc* Objects::npcAt(int tileX, int tileY) {
    tileX = wrapX(tileX);
    tileY = wrapY(tileY);
    for (Npc& npc : npcs_) {
        if (npc.solid() && covers(npc, tileX, tileY)) return &npc;
    }
    return nullptr;
}

Npc* Objects::npcFacing(const MapData& map, int tileX, int tileY, int direction) {
    const int x = wrapX(tileX + kDeltaX[direction]);
    const int y = wrapY(tileY + kDeltaY[direction]);
    if (Npc* npc = npcAt(x, y)) return npc;
    // A counter tile (but not a wall) lets the party talk to someone standing behind it.
    const uint8_t low = map.propAt(x, y) & FF6_PROP_LOW_MASK;
    if (low != kWallLow && (low & FF6_TILE_TYPE_MASK) == FF6_TILE_IMPASSABLE) {
        return npcAt(x + kDeltaX[direction], y + kDeltaY[direction]);
    }
    return nullptr;
}

void Objects::activate(Npc& npc, int partyFacing) {
    if (!npc.prop.noTurn) {
        npc.facing = (partyFacing + 2) % FF6_DIRECTION_COUNT;
        npc.frame = stopFrame(npc.facing);
    }
    if (npc.movement != FF6_OBJ_MOVE_ACTIVATED) npc.savedMovement = npc.movement;
    npc.movement = FF6_OBJ_MOVE_ACTIVATED;
}

bool Objects::takeEventRequest(uint32_t* offset) {
    if (!eventRequested_) return false;
    eventRequested_ = false;
    *offset = eventOffset_;
    return true;
}

void Objects::command(uint8_t op, Npc& npc, uint32_t arg) {
    switch (op) {
        case FF6_EVENT_OBJ_PASS_OFF: npc.passThrough = false; break;
        case FF6_EVENT_OBJ_PASS_ON: npc.passThrough = true; break;
        case FF6_EVENT_OBJ_GFX: npc.prop.gfx = static_cast<int>(arg); break;
        case FF6_EVENT_OBJ_PAL: npc.prop.palette = static_cast<int>(arg) & FF6_NPC_PAL_MASK; break;
        case FF6_EVENT_OBJ_VEHICLE: npc.vehicle = static_cast<int>(arg) & (FF6_VEHICLE_MASK | FF6_VEHICLE_RIDER); break;
        case FF6_EVENT_OBJ_CREATE:
            if (!npc.enabled) {
                npc.enabled = true;
                npc.visible = false;
                if (npc.map < 0 || npc.map != mapId_) npc.map = mapId_;  // a character created by an event is on the current map
            }
            break;
        case FF6_EVENT_OBJ_DELETE:
            npc.enabled = npc.visible = false;
            npc.savedMovement = FF6_OBJ_MOVE_NONE;
            break;
        case FF6_EVENT_OBJ_SHOW:
            if (npc.enabled) npc.visible = true;
            break;
        case FF6_EVENT_OBJ_HIDE:
            npc.visible = false;
            npc.savedMovement = npc.movement = FF6_OBJ_MOVE_NONE;
            break;
        case FF6_EVENT_OBJ_EVENT: npc.eventOffset = arg; break;
        case FF6_EVENT_OBJ_TOUCH_ON: npc.touch = true; break;
        case FF6_EVENT_OBJ_TOUCH_OFF: npc.touch = false; break;
        default: break;
    }
}

void Objects::startScript(Npc& npc, uint32_t offset) {
    npc.movement = FF6_OBJ_MOVE_SCRIPT;
    npc.steps = 0;
    npc.scriptPtr = static_cast<int>(offset);
}

void Objects::stop(Npc& npc) { npc.speedX = npc.speedY = 0; }

// CalcObjMoveDir: the speed for the movement direction and the speed index.
void Objects::startMove(Npc& npc) {
    const int index = npc.moveDirection - 1;
    if (index < 0 || index >= kDirections) return stop(npc);
    const int multiplier = kSpeedMultiplier[std::max(0, std::min(npc.speed, FF6_OBJ_SPEED_COUNT - 1))];
    npc.speedX = kRateX[index] * multiplier;
    npc.speedY = kRateY[index] * multiplier;
}

// UpdateObjLayerPriorityBefore: the sprite priority that suits the tile that the npc walks onto.
void Objects::priorityBefore(Npc& npc, int tileX, int tileY) {
    if (npc.priority != FF6_OBJ_PRIORITY_NORMAL) return;
    const uint8_t low = map_->propAt(tileX, tileY) & FF6_PROP_LOW_MASK;
    if (low == kWallLow || (low & FF6_TILE_BRIDGE)) {
        npc.topAbove = npc.bottomAbove = false;
        return;
    }
    if (!(low & FF6_TILE_PRIORITY_TOP)) npc.topAbove = false;
    if (!(low & FF6_TILE_PRIORITY_BOTTOM)) npc.bottomAbove = false;
}

// UpdateObjLayerPriorityAfter: the sprite priority that suits the tile that the npc stands on.
void Objects::priorityAfter(Npc& npc) {
    if (npc.priority != FF6_OBJ_PRIORITY_NORMAL) return;
    const uint8_t low = map_->propAt(npc.tileX(), npc.tileY()) & FF6_PROP_LOW_MASK;
    if (low == kWallLow || (low & FF6_TILE_BRIDGE)) {
        npc.topAbove = npc.bottomAbove = false;
        return;
    }
    if (low & FF6_TILE_PRIORITY_TOP) npc.topAbove = true;
    if (low & FF6_TILE_PRIORITY_BOTTOM) npc.bottomAbove = true;
}

// NPCMoveRand: a random direction; the npc only walks onto a free tile that allows it, on its own level.
void Objects::randomMove(Npc& npc) {
    const int direction = static_cast<int>(random() & (FF6_DIRECTION_COUNT - 1));
    const int x = wrapX(npc.tileX() + kDeltaX[direction]);
    const int y = wrapY(npc.tileY() + kDeltaY[direction]);
    if (occupied(x, y)) return stop(npc);
    const uint16_t prop = map_->propAt(x, y);
    if (!((prop >> FF6_PROP_HIGH_SHIFT) & FF6_PROP_NPC_RANDOM)) return stop(npc);
    const int level = map_->propAt(npc.tileX(), npc.tileY()) & FF6_TILE_LEVEL_MASK;
    const uint8_t type = prop & FF6_TILE_TYPE_MASK;
    const bool upper = level == FF6_Z_LEVEL_1;
    if (upper ? (type != FF6_TILE_LEVEL_1 && type != FF6_TILE_BRIDGE) : type != FF6_TILE_LEVEL_2) return stop(npc);
    priorityBefore(npc, x, y);
    npc.moveDirection = direction + 1;
    npc.facing = direction;
    startMove(npc);
}

// UpdateObjActions for one object whose turn it is: nothing happens while it is between two tiles.
void Objects::act(Npc& npc) {
    if (!npc.aligned()) return;
    if (npc.visible && npc.movement != FF6_OBJ_MOVE_USER) priorityAfter(npc);  // the party keeps the player's priority
    switch (npc.movement) {
        case FF6_OBJ_MOVE_SCRIPT: runScript(npc); break;
        case FF6_OBJ_MOVE_RANDOM: randomMove(npc); break;
        case FF6_OBJ_MOVE_ACTIVATED:
            stop(npc);
            if (!eventBusy_ && &npc != lead()) npc.movement = npc.savedMovement;  // the game gives the lead its own back
            break;
        default: break;
    }
}

// MoveObjs and UpdateObjFrame for one object.
void Objects::moveOne(Npc& npc) {
    npc.posX += npc.speedX;
    npc.posY += npc.speedY;
    if ((npc.jump & FF6_OBJ_JUMP_MASK) != 0) {
        npc.jumpHeight = kJumpHeight[npc.jump];
        --npc.jump;
    } else {
        npc.jumpHeight = 0;
    }
    if (npc.walkAnim) {
        if (npc.speedX != 0) {
            npc.frame = moveFrame(npc.facing, npc.pixelX());
        } else if (npc.speedY != 0) {
            npc.frame = moveFrame(npc.facing, npc.pixelY());
        }
    }
    if (npc.aligned()) stop(npc);  // it reached the next tile
}

void Objects::moveAll() {
    for (size_t slot = 0; slot < list_.size() && slot < FF6_OBJ_UPDATED_MAX; ++slot) {
        if (list_[slot]) moveOne(*list_[slot]);
    }
}

void Objects::update(const MapData& map, const std::vector<uint8_t>& script, EventBits& bits, uint32_t frame,
                     bool eventBusy) {
    map_ = &map;
    script_ = &script;
    bits_ = &bits;
    eventBusy_ = eventBusy;
    buildList();
    const size_t phase = frame % FF6_OBJ_ACTION_PERIOD;
    // The place in the object list decides on which frame an object acts.
    for (size_t slot = 0; slot < list_.size() && slot < FF6_OBJ_UPDATED_MAX; ++slot) {
        if (list_[slot] && slot / FF6_OBJ_ACTIONS_PER_FRAME == phase) act(*list_[slot]);
    }
    moveAll();
    map_ = nullptr;
    script_ = nullptr;
    bits_ = nullptr;
}

}  // namespace ff6
