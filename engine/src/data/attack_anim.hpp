#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "data/battle_gfx.hpp"

namespace ff6 {

// The properties of an attack: the scripts of its animation by layer (0xFFFF for none, bit 15 for graphics only).
struct AttackAnimProp {
    int sprite = 0xFFFF, bg1 = 0xFFFF, bg3 = 0xFFFF, extra = 0xFFFF;
    int spritePal = 0, bg1Pal = 0, bg3Pal = 0, sfx = 0, init = 0, delay = 0;
};

// The sheet of frames that goes with a script.
struct AttackGfxProp {
    int frames = 0;
    bool twoBits = false;
    int tileOffset = 0, firstFrame = 0, width = 0, height = 0;  // the size of a frame, in sprites of 16 pixels
};

// The animation of a blow: the scripts of the weapon in each hand, its palette, the effect of the hit and its sound.
struct WeaponAnimProp {
    int rightScript = 0, leftScript = 0, weaponPal = 0, hitScript = 0, hitPal = 0, init = 0, sfx = 0;
    bool hitIsSprite = false;
};

// A frame drawn: the sprites that make it up, the top left corner relative to the center of the thread.
struct AttackFrame {
    int x = 0, y = 0;
    BattleImage image;
};

// attackanim.ff6data: the scripts of the attack animations, their frames and the tiles of these, raw as the game keeps them.
struct AttackAnim {
    std::vector<uint8_t> props, gfxProps, scriptPtrs, scripts, framePtrs, frames, tiles3, pal, tiles2, gfx3, gfx2, weapons, mode7Gfx,
        mode7Map, rng, rowSpeeds, lights, eventPal;  // the random numbers, the steps toward each row, the palettes of the lights of Chadarnook, the sparkles of the frozen esper

    AttackAnimProp prop(int attack) const;
    // The animation of the blows of a weapon (the item) or of an attack of a monster (FF6_ATTACK_WEAPON_FISTS + the index).
    WeaponAnimProp weapon(int index) const;
    AttackGfxProp gfx(int script) const;
    // The script from its header, or null; `length` is what the data holds from there.
    const uint8_t* script(int id, size_t* length = nullptr) const;
    // A frame of the sheet of a script in a palette of the animations, mirrored for those that face the other way.
    // Empty (no pixels) for a blank frame or one that the sheet does not have. Decoded once. `cycle` turns some of the colors:
    // steps * 256 + first color * 16 + count (0 for none). `esper` is the picture of an esper that takes the place of the sheet of
    // the script (the animations of the espers load it in video memory); `palette` must then tell the espers apart (1000 + its order).
    const AttackFrame& frame(int script, int index, int palette, bool mirrored, int cycle = 0, const BattleImage* esper = nullptr) const;
    // The plane of mode 7 that a set of tiles and a map make (256 x 256 pixels), in a palette of the animations; the colors of
    // the palette repeat every 8, and `cycle` turns some of them as for `frame`.
    BattleImage mode7Plane(int tileSet, int mapSet, int palette, int cycle = 0) const;
    // The 8 colors of a palette of the animations as the game keeps them (BGR555); zeros past the data.
    std::array<uint16_t, 8> paletteWords(int palette) const;
    // The same for a palette of the sparkles of the frozen esper (BattleEventPal).
    std::array<uint16_t, 8> eventPaletteWords(int palette) const;

private:
    mutable std::map<long, AttackFrame> cache_;
};

AttackAnim loadAttackAnim(const std::string& packDir);

}  // namespace ff6
