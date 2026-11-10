// src/btlgfx/sprite_data.asm: the characters of the battles are the sprites of the map, posed by a table of tiles.
import { SPRITE_GFX_COUNT, spriteGfxOffset } from '../field/obj.mjs';
import { decompressLzss } from '../common/bytes.mjs';

export const CHAR_GFX_COUNT = 24;  // the characters and the ones that join the story
export const CHAR_GFX_BYTES = 0x1500;  // from the start of the graphics of a map sprite
export const CHAR_ACTION_COUNT = 32;  // poses of 8 tiles: two unused, then six in reading order of 16 x 24 pixels
export const CHAR_ACTION_TILES = 8;
const CHAR_ACTIONS = 0x02c745;  // C2/C745, a word for each tile: its offset in the graphics, 0xFFFF for none
export const CHAR_PAL_TABLE_BYTES = 24;
const CHAR_PAL_TABLE = 0x02ce2b;  // C2/CE2B: the palette of each character
export const CHAR_PAL_COUNT = 8;
export const CHAR_PAL_BYTES = 32;
const CHAR_PAL = 0x2d6300;  // ED/6300

const table = (rom, start, length) => Buffer.from(rom.subarray(start, start + length));

export const STATUS_GFX_BYTES = 0x1000;  // the sprites of the statuses (poison to sleep): 4 frames of 16 x 16 pixels for each, 16 tiles wide
const STATUS_GFX = 0x12e000;  // D2/E000, compressed
export const STATUS_PAL_BYTES = 32;  // four palettes of the colors 12 to 15
const STATUS_PAL = 0x02c689;  // C2/C689

export function readCharGfx(rom) {
  if (CHAR_GFX_COUNT > SPRITE_GFX_COUNT) throw new Error('Too many character graphics.');
  const blocks = [];
  for (let gfx = 0; gfx < CHAR_GFX_COUNT; gfx++) {
    const start = spriteGfxOffset(rom, gfx);
    blocks.push(table(rom, start, CHAR_GFX_BYTES));
  }
  return Buffer.concat(blocks);
}

export const readCharActions = rom => table(rom, CHAR_ACTIONS, CHAR_ACTION_COUNT * CHAR_ACTION_TILES * 2);
export const readCharPalTable = rom => table(rom, CHAR_PAL_TABLE, CHAR_PAL_TABLE_BYTES);
export const readCharPal = rom => table(rom, CHAR_PAL, CHAR_PAL_COUNT * CHAR_PAL_BYTES);
export const readStatusGfx = rom => Buffer.from(decompressLzss(rom, STATUS_GFX)).subarray(0, STATUS_GFX_BYTES);
export const readStatusPal = rom => table(rom, STATUS_PAL, STATUS_PAL_BYTES);
