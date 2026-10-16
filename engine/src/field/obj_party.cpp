#include <algorithm>

#include "field/obj.hpp"
#include "field/sprite_data.hpp"

namespace ff6 {

void Objects::resetCharacters() {
    for (size_t i = 0; i < chars_.size(); ++i) {
        Npc character;
        character.prop.gfx = static_cast<int>(i);  // each character has the map sprite of its number
        character.prop.palette = static_cast<int>(i) & (FF6_SPRITE_PALETTE_SLOTS - 1);
        character.frame = stopFrame(Down);
        chars_[i] = character;
    }
    camera_ = Npc();
    camera_.enabled = true;
    Npc& terra = chars_[0];
    terra.prop.palette = FF6_PLAYER_SPRITE_PAL;
    terra.enabled = terra.visible = true;
    terra.movement = FF6_OBJ_MOVE_USER;
    terra.party = 1;
    activeParty_ = 1;
    refreshParty();
}

// The characters of the active party, in the order of their places.
void Objects::refreshParty() {
    partySlots_.fill(-1);
    if (activeParty_ <= 0) return;
    std::vector<int> members;
    for (size_t i = 0; i < chars_.size(); ++i) {
        if (chars_[i].party == activeParty_) members.push_back(static_cast<int>(i));
    }
    std::stable_sort(members.begin(), members.end(), [&](int a, int b) {
        return chars_[static_cast<size_t>(a)].partySlot < chars_[static_cast<size_t>(b)].partySlot;
    });
    for (size_t i = 0; i < members.size() && i < partySlots_.size(); ++i) partySlots_[i] = members[i];
}

Npc* Objects::lead() { return partySlots_[0] >= 0 ? &chars_[static_cast<size_t>(partySlots_[0])] : nullptr; }

const Npc* Objects::lead() const { return const_cast<Objects*>(this)->lead(); }

void Objects::activateParty(int party) {
    const Npc* before = lead();
    const bool onMap = before && before->map == mapId_;
    const Npc where = onMap ? *before : Npc();  // the new party takes the place of the old one
    activeParty_ = party;
    refreshParty();
    if (onMap) {
        gather(where);
    } else {
        updateParty();
    }
}

void Objects::addToParty(int character, int party) {
    if (character < 0 || character >= FF6_CHAR_COUNT || party < 0 || party >= FF6_PARTY_COUNT) return;
    Npc& added = chars_[static_cast<size_t>(character)];
    added.party = party;
    added.partySlot = 0;
    if (party != 0) {  // FindEmptyCharSlot: the first place that no other member of the party has
        int taken = 0;
        for (size_t i = 0; i < chars_.size(); ++i) {
            if (static_cast<int>(i) != character && chars_[i].party == party) taken |= 1 << chars_[i].partySlot;
        }
        while (added.partySlot < FF6_PARTY_SIZE - 1 && (taken & (1 << added.partySlot))) ++added.partySlot;
    }
    refreshParty();
}

// The party is these characters, in this order: they are in the active party, and the others are not.
void Objects::setPartySlots(const std::array<int, FF6_PARTY_SIZE>& slots) {
    for (Npc& character : chars_) {
        if (activeParty_ > 0 && character.party == activeParty_) character.party = 0;
    }
    for (size_t i = 0; i < slots.size(); ++i) {
        partySlots_[i] = slots[i] >= 0 && slots[i] < FF6_CHAR_COUNT ? slots[i] : -1;
        if (partySlots_[i] < 0 || activeParty_ <= 0) continue;
        chars_[static_cast<size_t>(partySlots_[i])].party = activeParty_;
        chars_[static_cast<size_t>(partySlots_[i])].partySlot = static_cast<int>(i);
    }
}

void Objects::setPartyMap(int party, int map) {
    for (Npc& character : chars_) {
        if (party != 0 && character.party == party) character.map = map;
    }
}

// GetTopChar: the first character of the party shows and the others hide, all where the party stood.
void Objects::updateParty() {
    Npc* top = lead();
    if (!top) return;
    const Npc* anchor = top;
    for (int slot : partySlots_) {
        const Npc* member = slot >= 0 ? &chars_[static_cast<size_t>(slot)] : nullptr;
        if (member && member->visible && member->map == mapId_) {
            anchor = member;
            break;
        }
    }
    const Npc where = *anchor;
    gather(where);
}

// All the characters of the active party stand where `where` is; only the first one shows.
void Objects::gather(const Npc& where) {
    Npc* top = lead();
    if (!top) return;
    for (Npc& character : chars_) {
        if (activeParty_ <= 0 || character.party != activeParty_) continue;
        character.posX = where.posX;
        character.posY = where.posY;
        character.facing = where.facing;
        character.frame = where.frame;
        character.topAbove = where.topAbove;
        character.bottomAbove = where.bottomAbove;
        character.map = mapId_;
        character.visible = &character == top;
        stop(character);
    }
    top->enabled = top->visible = true;
}

void Objects::placeParty(int tileX, int tileY) {
    for (Npc& character : chars_) {
        if (activeParty_ <= 0 || character.party != activeParty_) continue;
        character.posX = tileX * FF6_OBJ_TILE_SUBPIXELS;
        character.posY = tileY * FF6_OBJ_TILE_SUBPIXELS;
        stop(character);
    }
}

void Objects::enterMap(int mapId, int tileX, int tileY, int facing) {
    for (Npc& character : chars_) {
        if (activeParty_ <= 0 || character.party != activeParty_) continue;
        character.map = mapId;
        character.facing = facing;
        character.frame = stopFrame(facing);
        character.steps = character.wait = character.jump = 0;
        character.scriptPtr = -1;
        character.movement = FF6_OBJ_MOVE_NONE;
    }
    placeParty(tileX, tileY);
    if (Npc* top = lead()) top->movement = FF6_OBJ_MOVE_USER;
    updateParty();
}

// The objects that act, in the order of obj.asm: the party, the camera, the other characters of the map, the npcs.
void Objects::buildList() {
    list_.clear();
    const auto listed = [&](const Npc* object) { return std::find(list_.begin(), list_.end(), object) != list_.end(); };
    const auto here = [&](const Npc& character) { return character.enabled && character.map == mapId_; };
    for (int slot : partySlots_) {
        if (slot >= 0 && here(chars_[static_cast<size_t>(slot)])) list_.push_back(&chars_[static_cast<size_t>(slot)]);
    }
    for (Npc& character : chars_) {
        if (activeParty_ > 0 && character.party == activeParty_ && here(character) && !listed(&character)) {
            list_.push_back(&character);
        }
    }
    if (list_.empty()) list_.push_back(nullptr);  // the first place is the party's, even when it is empty
    list_.push_back(&camera_);
    for (Npc& character : chars_) {
        if (here(character) && !listed(&character)) list_.push_back(&character);
    }
    for (Npc& npc : npcs_) {
        if (npc.enabled) list_.push_back(&npc);
    }
}

}  // namespace ff6
