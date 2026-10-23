#pragma once

#include <array>
#include <string>
#include <vector>

#include "constants.hpp"

namespace ff6 {

// One character of the game: where it is, which party it is in and how it looks.
struct SaveCharacter {
    bool enabled = false, visible = false;
    int party = 0, slot = 0;  // party number (0 for none) and place in it
    int map = -1;
    int gfx = 0, palette = 0;  // map sprite, and its palette slot
    int x = 0, y = 0, facing = 2;  // tile, and direction (0 up, 1 right, 2 down, 3 left)
};

// What the field needs to go on from where the player left: the map, the party and the story. It is saved as JSON.
struct SaveState {
    int map = 0, x = 0, y = 0, facing = 2;  // where the party stands
    int activeParty = 1;
    std::array<SaveCharacter, FF6_CHAR_COUNT> characters;
    std::vector<int> eventBits;  // the event bits that are set
    std::vector<int> battleFlags;  // the battle flags that are set
    std::array<int, FF6_SPRITE_PALETTE_SLOTS> spritePalettes{0, 1, 2, 3, 4, 5, 6, 7};  // palette of each slot
    bool hasParent = false;  // the map that the party goes back to from a house
    int parentMap = 0, parentX = 0, parentY = 0, parentFacing = 0;
};

std::string saveToJson(const SaveState& state);
// Throws std::runtime_error when the text is not a save of this version, or has values that do not exist.
SaveState saveFromJson(const std::string& text);
SaveState readSave(const std::string& path);
void writeSave(const std::string& path, const SaveState& state);

}  // namespace ff6
