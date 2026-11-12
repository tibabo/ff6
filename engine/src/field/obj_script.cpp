#include <cstdio>

#include "field/obj.hpp"
#include "field/sprite_data.hpp"

namespace ff6 {
namespace {

// ObjMoveDirTbl: the facing direction of each diagonal movement direction.
constexpr int kDiagonalFacing[16] = {0, 1, 2, 3, 1, 1, 3, 3, 0, 1, 1, 2, 2, 3, 3, 0};

}  // namespace

// ObjCmd_ff: the script is over and the npc stands still.
void Objects::endScript(Npc& npc) {
    npc.scriptPtr = -1;
    npc.movement = FF6_OBJ_MOVE_NONE;
    stop(npc);
}

// The walking part of ExecObjScript: one tile of the current walk command. The npc waits for a tile that another
// object holds, unless it may pass through objects.
void Objects::stepScript(Npc& npc) {
    // The camera goes through everything.
    if (&npc != &camera_ && npc.moveDirection < FF6_OBJ_DIAGONAL_FIRST && !npc.prop.scrollBg2) {
        const int facing = npc.moveDirection - 1;
        const int x = wrapX(npc.tileX() + kDeltaX[facing]);
        const int y = wrapY(npc.tileY() + kDeltaY[facing]);
        if (!npc.passThrough && occupied(x, y)) return stop(npc);
        priorityBefore(npc, x, y);
    }
    startMove(npc);
    --npc.steps;
}

// Executes the next object command. Returns true when the commands after it may run in the same action.
bool Objects::nextCommand(Npc& npc) {
    const std::vector<uint8_t>& script = *script_;
    if (npc.scriptPtr < 0 || static_cast<size_t>(npc.scriptPtr) >= script.size()) {
        endScript(npc);
        return false;
    }
    const size_t at = static_cast<size_t>(npc.scriptPtr);
    const uint8_t cmd = script[at];
    const auto arg = [&](size_t n) -> int { return at + n < script.size() ? script[at + n] : 0; };
    const auto bit = [&](int group) { return group * 0x100 + arg(1); };
    npc.scriptPtr += 1;  // past the command byte; commands with arguments add their size below

    if (cmd <= FF6_OBJ_CMD_FRAME_LAST) {
        npc.frame = actionFrame(cmd);
        return true;
    }
    if (cmd <= FF6_OBJ_CMD_WALK_LAST) {
        npc.facing = cmd & (FF6_DIRECTION_COUNT - 1);
        npc.moveDirection = npc.facing + 1;
        npc.steps = ((cmd - FF6_OBJ_CMD_WALK) >> 2) + 1;
        return true;
    }
    if (cmd <= FF6_OBJ_CMD_DIAGONAL_LAST) {
        const int index = cmd - FF6_OBJ_DIAGONAL_BASE;
        if (index < 16) {
            npc.moveDirection = index + 1;
            npc.facing = kDiagonalFacing[index];
            npc.steps = 1;
            return true;
        }
    } else if (cmd >= FF6_OBJ_CMD_SPEED && cmd <= FF6_OBJ_CMD_SPEED_LAST) {
        npc.speed = cmd - FF6_OBJ_CMD_SPEED;
        return true;
    } else if (cmd >= FF6_OBJ_CMD_TURN_UP && cmd <= FF6_OBJ_CMD_TURN_LAST) {
        npc.facing = cmd - FF6_OBJ_CMD_TURN_UP;
        npc.frame = stopFrame(npc.facing);
        return true;
    } else if (cmd >= FF6_OBJ_CMD_SET_BIT_0 && cmd < FF6_OBJ_CMD_CLEAR_BIT_0) {
        bits_->set(bit(cmd - FF6_OBJ_CMD_SET_BIT_0), true);
        npc.scriptPtr += 1;
        return true;
    } else if (cmd >= FF6_OBJ_CMD_CLEAR_BIT_0 && cmd <= FF6_OBJ_CMD_CLEAR_BIT_LAST) {
        bits_->set(bit(cmd - FF6_OBJ_CMD_CLEAR_BIT_0), false);
        npc.scriptPtr += 1;
        return true;
    }
    switch (cmd) {
        case FF6_OBJ_CMD_WALK_ANIM_ON: npc.walkAnim = true; return true;
        case FF6_OBJ_CMD_WALK_ANIM_OFF: npc.walkAnim = false; return true;
        case FF6_OBJ_CMD_PRIORITY:
            npc.priority = arg(1) & 3;
            npc.scriptPtr += 1;
            return true;
        case FF6_OBJ_CMD_VEHICLE:
            npc.vehicle |= arg(1) & (FF6_VEHICLE_MASK | FF6_VEHICLE_RIDER);
            npc.scriptPtr += 1;
            return true;
        case FF6_OBJ_CMD_SHOW:
            if (!npc.visible) {
                npc.visible = true;
                npc.priority = FF6_OBJ_PRIORITY_NORMAL;
            }
            return true;
        case FF6_OBJ_CMD_HIDE:
            npc.visible = false;
            npc.savedMovement = npc.movement = FF6_OBJ_MOVE_NONE;
            return true;
        case FF6_OBJ_CMD_SET_POSITION:
            npc.posX = arg(1) * FF6_OBJ_TILE_SUBPIXELS;
            npc.posY = arg(2) * FF6_OBJ_TILE_SUBPIXELS;
            npc.scriptPtr += 2;
            priorityAfter(npc);
            return false;
        case FF6_OBJ_CMD_SCROLL_TO:  // the camera goes to the party
            if (const Npc* party = lead()) {
                npc.posX = party->posX;
                npc.posY = party->posY;
            }
            return false;
        case FF6_OBJ_CMD_JUMP_LOW: npc.jump = FF6_OBJ_JUMP_LOW; return true;
        case FF6_OBJ_CMD_JUMP_HIGH: npc.jump = FF6_OBJ_JUMP_HIGH; return true;
        case FF6_OBJ_CMD_PAUSE:
            stop(npc);
            npc.wait = arg(1);
            npc.scriptPtr += 1;
            return false;
        case FF6_OBJ_CMD_EVENT:  // jump to an event script, once no event runs
            if (eventBusy_ || eventRequested_) {
                npc.scriptPtr -= 1;
                return false;
            }
            eventRequested_ = true;
            eventOffset_ = static_cast<uint32_t>(arg(1) | (arg(2) << 8) | (arg(3) << 16));
            npc.scriptPtr += 3;
            return false;
        case FF6_OBJ_CMD_RANDOM_BACK:
        case FF6_OBJ_CMD_RANDOM_FORWARD:
            if (random() >= FF6_RANDOM_HALF) {
                npc.scriptPtr = static_cast<int>(at) + (cmd == FF6_OBJ_CMD_RANDOM_BACK ? -arg(1) : arg(1));
            } else {
                npc.scriptPtr += 1;
            }
            return true;
        case FF6_OBJ_CMD_BACK: npc.scriptPtr = static_cast<int>(at) - arg(1); return true;
        case FF6_OBJ_CMD_FORWARD: npc.scriptPtr = static_cast<int>(at) + arg(1); return true;
        case FF6_OBJ_CMD_END: endScript(npc); return false;
        default: break;
    }
    std::fprintf(stderr, "object script: unsupported command 0x%02X at 0x%05zX of object 0x%02X, script ended\n", cmd, at,
                 static_cast<unsigned>(&npc - &npcs_[0]) + FF6_FIRST_NPC_OBJECT);
    endScript(npc);
    return false;
}

// ExecObjScript: waits, walks, then runs commands until one of them has to wait for the next action.
void Objects::runScript(Npc& npc) {
    for (int count = 0; count < FF6_OBJ_SCRIPT_STEPS_PER_ACTION; ++count) {
        if (npc.wait > 0) {
            --npc.wait;
            return;
        }
        if (npc.steps > 0 && npc.moveDirection != 0) return stepScript(npc);
        if (!nextCommand(npc)) return;
    }
}

}  // namespace ff6
