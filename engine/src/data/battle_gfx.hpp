#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "constants.hpp"

namespace ff6 {

// A picture of the battle, RGBA, with transparent pixels.
struct BattleImage {
    int width = 0, height = 0;
    std::vector<uint8_t> rgba;
};

// battlegfx.ff6data: the graphics of the monsters, raw as the game keeps them.
struct BattleGfx {
    std::vector<uint8_t> monsterProp;  // FF6_MONSTER_GFX_COUNT of FF6_MONSTER_GFX_PROP_BYTES
    std::vector<uint8_t> monsterPal;
    std::vector<uint8_t> stencilSmall, stencilLarge;
    std::vector<uint8_t> monsterGfx;
    std::vector<uint8_t> bgProp, bgPal, bgGfx, bgTiles;  // the backgrounds of the battlefield
    std::vector<uint8_t> charGfx, charActions, charPalTable, charPal;  // the characters, in their poses

    // The picture of a monster from the top left tile of its stencil; empty for a number that has none. Decoded once.
    const BattleImage& monster(int id) const;
    // The battlefield behind the monsters, FF6_SCREEN_WIDTH wide, one with no pixel for a background that has none.
    const BattleImage& background(int index) const;
    // Whether the field ripples like the desert's heat (a flag of its palette).
    bool wavy(int index) const;
    // A character (an actor of the game) in one of the poses of the table, 16 x 24 pixels; empty when there is none.
    const BattleImage& character(int actor, int pose) const;

private:
    mutable std::map<int, BattleImage> monsters_, backgrounds_, characters_;
};

BattleGfx loadBattleGfx(const std::string& packDir);

// A tile of 4 or 3 bitplanes, as the SNES keeps it, into 8 x 8 colors; `palette` is 16 BGR555 colors, color 0 transparent.
void decodeTile(const uint8_t* tile, bool threePlanes, const uint8_t palette[][FF6_BYTES_PER_PIXEL], uint8_t* rgba,
    int stride, int x, int y);

}  // namespace ff6
