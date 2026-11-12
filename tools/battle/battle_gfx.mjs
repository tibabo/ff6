// src/gfx/monster_gfx.asm: the graphics of the monsters, which the battles draw as a grid of tiles chosen by a stencil.
import { readBgGfx, readBgPal, readBgProp, readBgTiles } from './battle_bg.mjs';
import { readCharActions, readCharGfx, readMagitekGfx, readCharPal, readCharPalTable, readStatusGfx, readStatusPal } from './battle_chars.mjs';

export const BATTLE_GFX_MAGIC = 'FF6BGX01';

export const MONSTER_GFX_COUNT = 416;  // 384 monsters, then the graphics of the espers
export const MONSTER_GFX_PROP_BYTES = 5;  // gfx / 8 (bit 15: 3bpp), palette / 16 (bit 15: large stencil), stencil
const MONSTER_GFX_PROP = 0x127000;  // D2/7000
export const MONSTER_PAL_BYTES = 0x3000;
const MONSTER_PAL = 0x127820;  // D2/7820, 16 colors of 32 bytes each from every 16 bytes
const MONSTER_STENCIL = 0x12a820;  // D2/A820: the addresses of the small and of the large stencils
const BANK_D2 = 0x120000;
export const STENCIL_SMALL_COUNT = 128;  // 8 rows of 8 tiles, a byte each
export const STENCIL_SMALL_BYTES = 8;
export const STENCIL_LARGE_COUNT = 48;  // 16 rows of 16 tiles, two bytes each
export const STENCIL_LARGE_BYTES = 32;
export const MONSTER_GFX_BYTES = 0x37200;
const MONSTER_GFX = 0x297000;  // E9/7000, as the colosseum code addresses it

const table = (rom, start, length) => Buffer.from(rom.subarray(start, start + length));

export const readMonsterGfxProp = rom => table(rom, MONSTER_GFX_PROP, MONSTER_GFX_COUNT * MONSTER_GFX_PROP_BYTES);
export const readMonsterPal = rom => table(rom, MONSTER_PAL, MONSTER_PAL_BYTES);
export const readMonsterGfx = rom => table(rom, MONSTER_GFX, MONSTER_GFX_BYTES);
export const readStencils = rom => {
  const small = BANK_D2 + rom.readUInt16LE(MONSTER_STENCIL);
  const large = BANK_D2 + rom.readUInt16LE(MONSTER_STENCIL + 2);
  return {
    small: table(rom, small, STENCIL_SMALL_COUNT * STENCIL_SMALL_BYTES),
    large: table(rom, large, STENCIL_LARGE_COUNT * STENCIL_LARGE_BYTES),
  };
};

// `version` is the version of the whole pack.
export function buildBattleGfx(rom, version) {
  const header = Buffer.alloc(12);
  header.write(BATTLE_GFX_MAGIC, 0, 8, 'ascii');
  header.writeUInt32LE(version, 8);
  const stencils = readStencils(rom);
  return Buffer.concat([header, readMonsterGfxProp(rom), readMonsterPal(rom), stencils.small, stencils.large,
    readMonsterGfx(rom), readBgProp(rom), readBgPal(rom), readBgGfx(rom), readBgTiles(rom), readCharGfx(rom),
    readCharActions(rom), readCharPalTable(rom), readCharPal(rom), readStatusGfx(rom), readStatusPal(rom), readMagitekGfx(rom)]);
}
