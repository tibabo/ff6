// src/btlgfx/attack_anim_script.asm, src/data/attack_anim_prop.asm, src/data/attack_gfx_prop.asm, src/gfx/attack_anim_gfx.asm:
// the animations of the attacks: a script of frames and moves, the sprites of the frames, and the tiles they are made of.
import { decompressLzss } from '../common/bytes.mjs';

export const ATTACK_ANIM_MAGIC = 'FF6ATK01';

export const ATTACK_ANIM_PROP_COUNT = 406;  // one for each attack
export const ATTACK_ANIM_PROP_BYTES = 14;  // 3 script words, 3 palettes, sfx, init function, extra script word, delay
export const ATTACK_ANIM_SCRIPT_COUNT = 650;  // the scripts, and the graphics (6 bytes each) that go with them
export const ATTACK_GFX_PROP_BYTES = 6;  // flags | frames, tile offset, first frame (word), frame width, frame height
export const ATTACK_ANIM_SCRIPT_BYTES = 0xd000;  // D0/0000: the scripts, the properties of the attacks, then the battle events and their scripts
export const ATTACK_FRAME_PTR_COUNT = 2949;  // the frames, and the end of the last one
export const ATTACK_FRAME_FIRST = 0x141;  // D1/0141, the pointers are addresses of bank D1
export const ATTACK_FRAME_END = 59785;  // the pointer after the last frame
export const ATTACK_TILES_3BPP_BYTES = 0x6000;  // D2/0000: the tiles of the sheets, 2 bytes each (14 bits + flips)
export const ATTACK_PAL_BYTES = 0x6000;  // D2/6000: 16 colors of 2 bytes each
export const ATTACK_TILES_2BPP_BYTES = 0x4000;  // D2/C000
export const ATTACK_GFX_3BPP_BYTES = 0x1ca00;  // D3/0000
export const ATTACK_GFX_2BPP_BYTES = 0x6000;  // D8/7000
export const ATTACK_WEAPON_PROP_COUNT = 128;  // the weapons (0-0x5C), then the attacks of the monsters (0x5D-0x7F)
export const ATTACK_WEAPON_PROP_BYTES = 8;  // right and left hand scripts, weapon palette, hit script and palette, init function, sound
export const ATTACK_MODE7_GFX_BYTES = 0x1800;  // D8/D000, compressed: three sets of 63 tiles of 4bpp (0x800 apart)
export const ATTACK_MODE7_MAP_BYTES = 0xc00;  // D8/DAF2, compressed: three maps of 32 x 32 tile numbers (0x400 apart)

export const ATTACK_RNG_BYTES = 256;  // C0/FD00 (RNGTbl): the random numbers that the scripts and the scrolls draw from
export const ATTACK_ROW_SPEED_BYTES = 32;  // C2/A86F: the steps (signed) toward the back row, then C2/A87F toward the front row; by battle type * 4 + slot
export const ATTACK_LIGHTS_BYTES = 192;  // C2/E288: the 3 palettes of 16 colors of the field of Chadarnook, lights on then lights off

const PROP = 0x107fb2;  // D0/7FB2
const SCRIPT = 0x100000;  // D0/0000
const SCRIPT_PTRS = 0x11ead8;  // D1/EAD8
const FRAME_PTRS = 0x14df3c;  // D4/DF3C
const FRAME_BANK = 0x110000;  // D1/0000
const GFX_PROP = 0x14d000;  // D4/D000
const TILES_3BPP = 0x120000;  // D2/0000
const PAL = 0x126000;  // D2/6000
const TILES_2BPP = 0x12c000;  // D2/C000
const GFX_3BPP = 0x130000;  // D3/0000
const GFX_2BPP = 0x187000;  // D8/7000
const WEAPON_PROP = 0x2ce400;  // EC/E400 (src/data/weapon_anim_prop.asm)
const MODE7_GFX = 0x18d000;  // D8/D000
const MODE7_MAP = 0x18daf2;  // D8/DAF2
const RNG = 0x00fd00;  // C0/FD00
const ROW_SPEEDS = 0x02a86f;  // C2/A86F
const LIGHTS = 0x02e288;  // C2/E288

const table = (rom, start, length) => Buffer.from(rom.subarray(start, start + length));

export const readAttackAnimProp = rom => table(rom, PROP, ATTACK_ANIM_PROP_COUNT * ATTACK_ANIM_PROP_BYTES);
export const readAttackGfxProp = rom => table(rom, GFX_PROP, ATTACK_ANIM_SCRIPT_COUNT * ATTACK_GFX_PROP_BYTES);
export const readAttackScriptPtrs = rom => table(rom, SCRIPT_PTRS, ATTACK_ANIM_SCRIPT_COUNT * 2);
export const readAttackScripts = rom => table(rom, SCRIPT, ATTACK_ANIM_SCRIPT_BYTES);
export const readAttackFramePtrs = rom => table(rom, FRAME_PTRS, ATTACK_FRAME_PTR_COUNT * 2);
export const readAttackFrames = rom => table(rom, FRAME_BANK + ATTACK_FRAME_FIRST, ATTACK_FRAME_END - ATTACK_FRAME_FIRST);
export const readAttackTiles3bpp = rom => table(rom, TILES_3BPP, ATTACK_TILES_3BPP_BYTES);
export const readAttackPal = rom => table(rom, PAL, ATTACK_PAL_BYTES);
export const readAttackTiles2bpp = rom => table(rom, TILES_2BPP, ATTACK_TILES_2BPP_BYTES);
export const readAttackGfx3bpp = rom => table(rom, GFX_3BPP, ATTACK_GFX_3BPP_BYTES);
export const readAttackGfx2bpp = rom => table(rom, GFX_2BPP, ATTACK_GFX_2BPP_BYTES);
export const readWeaponAnimProp = rom => table(rom, WEAPON_PROP, ATTACK_WEAPON_PROP_COUNT * ATTACK_WEAPON_PROP_BYTES);
export const readAttackMode7Gfx = rom => decompressLzss(rom, MODE7_GFX, ATTACK_MODE7_GFX_BYTES);
export const readAttackMode7Map = rom => decompressLzss(rom, MODE7_MAP, ATTACK_MODE7_MAP_BYTES);
export const readAttackRng = rom => table(rom, RNG, ATTACK_RNG_BYTES);
export const readAttackRowSpeeds = rom => table(rom, ROW_SPEEDS, ATTACK_ROW_SPEED_BYTES);
export const readAttackLights = rom => table(rom, LIGHTS, ATTACK_LIGHTS_BYTES);

// `version` is the version of the whole pack.
export function buildAttackAnim(rom, version) {
  const header = Buffer.alloc(12);
  header.write(ATTACK_ANIM_MAGIC, 0, 8, 'ascii');
  header.writeUInt32LE(version, 8);
  return Buffer.concat([header, readAttackAnimProp(rom), readAttackGfxProp(rom), readAttackScriptPtrs(rom), readAttackScripts(rom),
    readAttackFramePtrs(rom), readAttackFrames(rom), readAttackTiles3bpp(rom), readAttackPal(rom), readAttackTiles2bpp(rom),
    readAttackGfx3bpp(rom), readAttackGfx2bpp(rom), readWeaponAnimProp(rom), readAttackMode7Gfx(rom), readAttackMode7Map(rom),
    readAttackRng(rom), readAttackRowSpeeds(rom), readAttackLights(rom)]);
}
