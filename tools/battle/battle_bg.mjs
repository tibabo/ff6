// src/gfx/battle_bg.asm: the backgrounds of the battles; each one is a tilemap of tiles of three blocks of graphics.
import { decompressLzss, read24, slice } from '../common/bytes.mjs';

export const BG_COUNT = 56;  // 53 backgrounds and three blank ones
export const BG_PROP_BYTES = 6;  // three blocks of graphics, two tilemaps, a palette (bit 7: wavy)
const BG_PROP = 0x270000;  // E7/0000
export const BG_PAL_BYTES = 96;  // three palettes of 16 colors
const BG_PAL = 0x270150;  // E7/0150
export const BG_GFX_COUNT = 75;
export const BG_GFX_BYTES = 0x2000;
const BG_GFX_PTRS = 0x271650;  // E7/1650, 24 bits each
export const BG_TILES_COUNT = 49;
export const BG_TILES_BYTES = 0x800;  // 32 x 32 words
const BG_TILES_PTRS = 0x271848;  // E7/1848, 16 bits each, in bank E7
const BANK_E7 = 0x270000;
const FIRST_COMPRESSED_BANK = 0xe7;  // blocks in a bank under it are map graphics, as they are

const hiRomOffset = address => ((address >> 16) - 0xc0) * 0x10000 + (address & 0xffff);

export const readBgProp = rom => Buffer.from(rom.subarray(BG_PROP, BG_PROP + BG_COUNT * BG_PROP_BYTES));
export const readBgPal = rom => Buffer.from(rom.subarray(BG_PAL, BG_PAL + BG_COUNT * BG_PAL_BYTES));

const padded = (data, size) => {
  const block = Buffer.alloc(size);
  data.copy(block, 0, 0, Math.min(size, data.length));
  return block;
};

export function readBgGfx(rom) {
  const blocks = [];
  for (let index = 0; index < BG_GFX_COUNT; index++) {
    const address = read24(rom, BG_GFX_PTRS + index * 3);
    const offset = hiRomOffset(address);
    const compressed = (address >> 16) >= FIRST_COMPRESSED_BANK;
    blocks.push(padded(compressed ? decompressLzss(rom, offset, BG_GFX_BYTES) : slice(rom, offset, BG_GFX_BYTES, `Battle bg graphics ${index}`), BG_GFX_BYTES));
  }
  return Buffer.concat(blocks);
}

export function readBgTiles(rom) {
  const blocks = [];
  for (let index = 0; index < BG_TILES_COUNT; index++) {
    const offset = BANK_E7 + rom.readUInt16LE(BG_TILES_PTRS + index * 2);
    blocks.push(padded(decompressLzss(rom, offset, BG_TILES_BYTES), BG_TILES_BYTES));
  }
  return Buffer.concat(blocks);
}
