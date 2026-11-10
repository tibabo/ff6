#pragma once

#include <array>
#include <string>
#include <vector>

#include "constants.hpp"
#include "state/party_state.hpp"

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
    std::vector<int> variables;  // the 256 event variables, empty when they are all 0
    std::vector<int> timers;  // flags, counter and event address of each of the 4 timers, empty when they are all 0
    std::array<int, FF6_SPRITE_PALETTE_SLOTS> spritePalettes{0, 1, 2, 3, 4, 5, 6, 7};  // palette of each slot
    bool hasParent = false;  // the map that the party goes back to from a house
    int parentMap = 0, parentX = 0, parentY = 0, parentFacing = 0;
    int doomGazeCounter = 0;  // where the squares of Doom Gaze come from in the table of random numbers
    int airshipX = 0, airshipY = 0;  // the tile of the world map where the airship is parked
    std::array<int, 3> config{FF6_CONFIG_BATTLE_DEFAULT, 0, 0};  // the bytes of GameConfig; a save without them has the defaults
    // The colors that the player chose; without them the font is white and the windows have the colors of the ROM.
    bool hasColors = false;
    int fontColor = FF6_CONFIG_FONT_COLOR_DEFAULT;
    std::array<int, FF6_MENU_WINDOW_COUNT * FF6_MENU_WINDOW_COLORS> windowColors{};  // pattern by pattern
    // The statistics, the inventory and the gil; a save without them starts the characters from their properties.
    bool hasParty = false;
    PartyState party;
    std::vector<std::string> names;  // what the player named the characters, "" for the names of the game; empty when none
};

std::string saveToJson(const SaveState& state);
// Throws std::runtime_error when the text is not a save of this version, or has values that do not exist.
SaveState saveFromJson(const std::string& text);
SaveState readSave(const std::string& path);
void writeSave(const std::string& path, const SaveState& state);

}  // namespace ff6
