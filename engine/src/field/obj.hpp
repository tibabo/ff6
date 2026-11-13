#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "field/event.hpp"
#include "field/map.hpp"

namespace ff6 {

// event/npc_prop.asm: the 9-byte npc property item, decoded.
struct NpcProp {
    uint32_t eventOffset = 0;  // into the event script
    int palette = 0;  // sprite palette slot 0-7
    bool scrollBg2 = false;
    int switchId = 0;  // npc switch: the npc is visible while event bit FF6_NPC_SWITCH_EVENT_BIT + id is set
    int x = 0, y = 0;  // map tile
    bool special = false;  // vehicles and other npcs with their own graphics
    int speed = 0;
    int gfx = 0;  // map sprite graphics
    int movement = 0;
    int layerPriority = 0;  // FF6_OBJ_PRIORITY_*
    int facing = Down;
    bool noTurn = false;  // does not turn toward the party when activated
    int animFrame = 0;
    int vehicle = 0;  // FF6_VEHICLE_MASK bits: none, chocobo, magitek or raft
    int vram = 0;  // special npcs: where their graphics go, in steps of two tiles
    bool large = false;  // special npcs: 32x32 pixels instead of 16x16
    int frames = 1;  // special npcs: the frames of their graphics (1, 2 or 4 of those that the sprite shows in turn)
    int speedShift = 0;  // special npcs: the frames change every 4 << this many game frames
    bool flip = false;  // special npcs: the sprite is mirrored
    int order = 0;  // special npcs: FF6_NPC_ORDER_*, where the sprite is among the others
    int shiftX = 0, shiftY = 0;  // special npcs: pixels from the place (or from the master) where the sprite is drawn
    int master = -1;  // special npcs: the npc whose place it follows, -1 for none
};
NpcProp decodeNpcProp(const uint8_t* bytes);

// An object of obj.asm that stands for an npc: where it is, how it moves and which script it runs.
struct Npc {
    NpcProp prop;
    uint32_t eventOffset = 0;  // run when the party talks to it; event command 0x7A changes it
    bool enabled = false;  // the object exists
    bool visible = false;
    int32_t posX = 0, posY = 0;  // 1/FF6_OBJ_SUBPIXELS pixel
    int speedX = 0, speedY = 0;  // subpixels per frame, both 0 while it stands on a tile
    int speed = FF6_OBJ_DEFAULT_SPEED;  // index of the speed multiplier
    int facing = Down;
    int frame = 0;  // index into the sprite strip
    bool walkAnim = true;
    int movement = FF6_OBJ_MOVE_NONE, savedMovement = FF6_OBJ_MOVE_NONE;  // type, and the one to return to after a talk
    int moveDirection = 0;  // 0 none, 1-4 up, right, down, left, then the diagonals
    int steps = 0;  // tiles that the script still has to walk
    int wait = 0;  // actions that the script still has to wait for
    int scriptPtr = -1;  // into the event script, -1 without a script
    int jump = 0;  // counter into the jump height table, 0 when not jumping
    int jumpHeight = 0;  // pixels above the ground
    int priority = FF6_OBJ_PRIORITY_NORMAL;  // sprite order override
    bool passThrough = false;  // may walk onto a tile that another object holds
    int vehicle = 0;  // FF6_VEHICLE_* flags: the type and whether the rider shows
    bool touch = false;  // starts its event when the party touches it
    bool topAbove = false, bottomAbove = false;  // sprite priority of the tile under the npc
    int strip = -1;  // texture of the sprite frames, set by the game
    int party = 0;  // characters only: party number, 0 for none
    int partySlot = 0;  // characters only: place in the party
    int map = -1;  // characters only: the map that they are on

    int pixelX() const { return posX >> 8; }
    int pixelY() const { return posY >> 8; }
    int tileX() const { return posX >> 12; }
    int tileY() const { return posY >> 12; }
    bool aligned() const { return (posX & (FF6_OBJ_TILE_SUBPIXELS - 1)) == 0 && (posY & (FF6_OBJ_TILE_SUBPIXELS - 1)) == 0; }
    bool blocks() const { return enabled && visible && !prop.special; }
    // InitNPCMap: in the object map, where it stops the party and can be talked to; not one that scrolls with bg2 or whose
    // sprite priority is not the normal one (a save point, which the party walks onto).
    bool solid() const { return blocks() && !prop.scrollBg2 && prop.order == FF6_NPC_ORDER_NORMAL; }
    bool topIsAbove() const;
    bool bottomIsAbove() const;
};

// Port of the object parts of obj.asm: InitNPCs, UpdateObjActions, MoveObjs, NPCMoveRand and ExecObjScript. Objects
// 0x00-0x0F are the characters, 0x10-0x2F the npcs of the map, 0x30 the camera and 0x31-0x34 the active party.
class Objects {
public:
    // Object 0x10 + n is the n-th property item of the map, also when its switch hides it. Characters and the camera
    // keep their state from one map to the next.
    void init(const MapData& map, const EventBits& bits);
    std::vector<Npc>& npcs() { return npcs_; }
    const std::vector<Npc>& npcs() const { return npcs_; }
    Npc* npcByObject(int object);
    const Npc* npcByObject(int object) const;
    // Any object by its number; null for an empty party slot or a number that is not an object.
    Npc* object(int id);
    const Npc* object(int id) const;
    std::array<Npc, FF6_CHAR_COUNT>& characters() { return chars_; }
    const std::array<Npc, FF6_CHAR_COUNT>& characters() const { return chars_; }
    Npc& camera() { return camera_; }
    const Npc& camera() const { return camera_; }
    int mapId() const { return mapId_; }
    // The object is in the object list, so its script runs: the camera always, a character on its map, an npc when it exists.
    bool acts(const Npc& object) const;

    // obj_party.cpp: the party.
    // A new game: Terra is the only character, she is in party 1 and leads it.
    void resetCharacters();
    // The party that a game starts with instead of Terra alone: the first of these characters leads it.
    void startParty(const std::vector<int>& characters);
    int activeParty() const { return activeParty_; }
    const std::array<int, FF6_PARTY_SIZE>& partySlots() const { return partySlots_; }
    Npc* lead();  // the first character of the active party, null when it is empty
    const Npc* lead() const;
    void activateParty(int party);
    // A saved game: the party that is active, with the characters as the save has set them.
    void restoreParty(int party) {
        activeParty_ = party;
        refreshParty();
    }
    void addToParty(int character, int party);
    void setPartySlots(const std::array<int, FF6_PARTY_SIZE>& slots);
    void setPartyMap(int party, int map);
    // GetTopChar: the first character of the party is the one that shows, where the party stood.
    void updateParty();
    // The active party arrives on a map at a tile, all together.
    void enterMap(int mapId, int tileX, int tileY, int facing);
    // The active party stands on a tile of the current map.
    void placeParty(int tileX, int tileY);

    bool occupied(int tileX, int tileY) const;
    // The npc to talk to when the party at (tileX, tileY) faces `direction`, also across a counter tile.
    Npc* npcFacing(const MapData& map, int tileX, int tileY, int direction);
    // The party talks to the npc: it turns toward the party and stops until the event is over.
    void activate(Npc& npc, int partyFacing);

    // Runs one game frame: the actions of the objects whose turn it is, then the movement of all of them.
    // `eventBusy` tells that an event runs; the script of an object may start one when none does.
    void update(const MapData& map, const std::vector<uint8_t>& script, EventBits& bits, uint32_t frame, bool eventBusy);
    // The event that an object script asked for (object command 0xF9), once.
    bool takeEventRequest(uint32_t* offset);

    // InitObjScript: the object runs the script that starts at `offset` of the event script.
    void startScript(Npc& npc, uint32_t offset);
    // Event commands 0x36 to 0x7D that change an object.
    void command(uint8_t op, Npc& npc, uint32_t arg);

private:
    Npc* npcAt(int tileX, int tileY);
    bool covers(const Npc& npc, int tileX, int tileY) const;
    int wrapX(int tile) const;
    int wrapY(int tile) const;
    uint32_t random();

    void buildList();
    void refreshParty();
    void gather(const Npc& where);
    void act(Npc& npc);
    void moveAll();
    void moveOne(Npc& npc);
    void stop(Npc& npc);
    void startMove(Npc& npc);
    void randomMove(Npc& npc);
    void priorityBefore(Npc& npc, int tileX, int tileY);
    void priorityAfter(Npc& npc);

    // obj_script.cpp
    void runScript(Npc& npc);
    void stepScript(Npc& npc);
    void endScript(Npc& npc);
    bool nextCommand(Npc& npc);

    std::vector<Npc> npcs_;
    std::array<Npc, FF6_CHAR_COUNT> chars_;
    Npc camera_;
    std::array<int, FF6_PARTY_SIZE> partySlots_{-1, -1, -1, -1};  // character numbers
    int activeParty_ = 1;
    int mapId_ = -1;
    std::vector<Npc*> list_;  // the objects that act, in the order of obj.asm; null for the empty party place
    int width_ = 1, height_ = 1;  // of the first layer, in tiles
    const MapData* map_ = nullptr;  // only during update()
    const std::vector<uint8_t>* script_ = nullptr;
    EventBits* bits_ = nullptr;
    bool eventBusy_ = false, eventRequested_ = false;
    uint32_t eventOffset_ = 0;
    uint32_t seed_ = 0x2545F491;
};

}  // namespace ff6
