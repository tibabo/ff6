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
    std::vector<uint8_t> magitekGfx;  // FF6_MAGITEK_FRAMES of FF6_MAGITEK_FRAME_BYTES, the armor of the characters in a magitek battle
    std::vector<uint8_t> statusGfx, statusPal;  // the sprites of the statuses (FF6_STATUS_GFX_BYTES) and their four palettes of the colors 12 to 15

    // The picture of a monster from the top left tile of its stencil; empty for a number that has none. Decoded once.
    const BattleImage& monster(int id) const;
    // The same with the first 8 colors of its palette replaced (BGR555, the first one stays transparent); not kept.
    BattleImage monsterWith(int id, const std::array<uint16_t, 8>& colors) const;
    // The battlefield behind the monsters, FF6_SCREEN_WIDTH wide, one with no pixel for a background that has none. `step` is
    // the turn of the palette of the fields whose colors run (see backgroundStep). `palette` (3 of 16 BGR555 words) replaces the
    // colors of the field, `variant` (1 and up) telling the replacements apart.
    const BattleImage& background(int index, int step = 0, const uint8_t* palette = nullptr, int variant = 0) const;
    // The turn of the colors of a field at a frame: 0 for a field whose colors stay (UpdateBattleBG: the raft, the waterfall,
    // Cyan's dream).
    static int backgroundStep(int index, int frame);
    // Whether the field ripples like the desert's heat (a flag of its palette).
    bool wavy(int index) const;
    // A character (an actor of the game) in one of the poses of the table, 16 x 24 pixels; empty when there is none.
    const BattleImage& character(int actor, int pose) const;
    // The same with the colors of the palette changed (16 BGR555 words), as the statuses do; `outlineOnly` keeps just the pixels of
    // the outline (the color 1), which is what vanish leaves. Not kept.
    BattleImage characterWith(int actor, int pose, const std::array<uint16_t, 16>& colors, bool outlineOnly = false) const;
    std::array<uint16_t, 16> characterPalette(int actor) const;
    // The sprite of a status (1 poison, 2 confuse, 3 blind, 4 berserk, 5 silence, 6 sleep, 7 the shadow under a character) at one of
    // its four frames, 16 x 16.
    const BattleImage& statusSprite(int kind, int frame) const;
    // The magitek armor at one of its frames, 32 x 32.
    const BattleImage& magitekArmor(int frame) const;

private:
    BattleImage decodeMonster(int id, const std::array<uint16_t, 8>* replace) const;
    mutable std::map<int, BattleImage> monsters_, backgrounds_, characters_, statuses_, armors_;
    mutable std::map<int, std::vector<uint8_t>> characterColors_;  // the numbers of the colors of each pixel, 0 for none
};

BattleGfx loadBattleGfx(const std::string& packDir);

// A tile of 4 or 3 bitplanes, as the SNES keeps it, into 8 x 8 colors; `palette` is 16 BGR555 colors, color 0 transparent.
void decodeTile(const uint8_t* tile, bool threePlanes, const uint8_t palette[][FF6_BYTES_PER_PIXEL], uint8_t* rgba,
    int stride, int x, int y);

}  // namespace ff6
