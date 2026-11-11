// src/cutscene/title.asm, opening.asm and cutscene_main.asm: the title screen and the magitek march.
// The cutscene program runs from $7e5000 in the game, so its labels are addresses from 0x5000 on; the engine reads its
// tables (palettes, sprite frames, credits) at those addresses.
import { decompressLzss, read24, slice } from '../common/bytes.mjs';

export const TITLE_MAGIC = 'FF6TITL1';
const CUTSCENE_PROG = 0x02686c;  // C2/686C, compressed
export const CUTSCENE_PROG_BYTES = 11363;
const TITLE_OPENING_GFX = 0x18f000;  // D8/F000, compressed
export const TITLE_OPENING_GFX_BYTES = 44712;
const RUIN_GFX = 0x2ce900;  // EC/E900, compressed: the planet that breaks up
export const RUIN_GFX_BYTES = 9408;
const FLOATING_GFX = 0x194e96;  // D9/4E96, compressed: the continent that rises
export const FLOATING_GFX_BYTES = 2304;
const THE_END_GFX = 0x296300;  // E9/6300: the letters and the stars, 4 bits per pixel
const THE_END_GFX_2 = 0x09fe00;  // C9/FE00, then the palettes of the letters and of the stars at C9/FF00
export const THE_END_GFX_BYTES = 0xd00;
export const THE_END_GFX_2_BYTES = 0x100;
export const THE_END_PAL_BYTES = 0x100;
const CREDITS = 0x029894;  // C2/9894: the pages of the credits, the tables of their words, then their texts
export const CREDITS_BYTES = 0xdc6;
const FLIGHT_GFX = 0x19568f;  // D9/568F, compressed: the land, the sea and the clouds of the flight of the credits, with their maps
export const FLIGHT_GFX_BYTES = 0x74fb;
const FLIGHT_TABLES = 0x0ff969;  // CF/F969: the palette of each tile (clouds, then land and sea), then four lists of boxes of the map
export const FLIGHT_TABLES_BYTES = 0x1c0;
const FLIGHT_PALETTES = 0x0295bc;  // C2/95BC: six palettes of 16 colors
export const FLIGHT_PALETTES_BYTES = 0xc0;
const FLIGHT_SCROLLS = 0x028a70;  // C2/8A70: the buttons that move the camera, for the scenes and the clouds
export const FLIGHT_SCROLLS_BYTES = 0xd5;
const SKY_GFX = 0x2eb290;  // EE/B290, compressed: the backdrop of the world, which is the sky of the flights
const SKY_MAP = 0x2ec295;  // EE/C295, compressed
export const SKY_GFX_BYTES = 0x1a20;  // what the credits take of them
export const SKY_MAP_BYTES = 0x400;
const SKY_PAL = 0x02953c;  // C2/953C
export const SKY_PAL_BYTES = 0x20;
const ENDING_ANIM = 0x028a70;  // C2/8A70: the animations and sprites of the ending (the big names), then the palettes of the scenes
export const ENDING_ANIM_BYTES = 0xe24;
const ENDING_FONT = 0x04ba00;  // C4/BA00, compressed: 2 bits per pixel, the letters of the names
export const ENDING_FONT_BYTES = 0x8a0;
const ENDING_GFX_1 = 0x04c008;  // C4/C008, compressed: the characters and the maps of the backgrounds of the scenes of the characters and of the book
export const ENDING_GFX_1_BYTES = 0x5040;
const ENDING_ANIM_2 = 0x0ff450;  // CF/F450: the animations and sprites of the characters, the birds, the boat and "and you" (whose count reads 12 sprites too many, as the game does)
export const ENDING_ANIM_2_BYTES = 0x4c0;
const ENDING_PHASE = 0x03cb68;  // C3/CB68: the code of the birds, whose bytes the game reads as the first phase of each one (an error of the game)
export const ENDING_PHASE_BYTES = 0x80;
const ENDING_AIRSHIP_PAL = 0x2ffac8;  // EF/FAC8: the colors of the ground in the scene of the Falcon
export const ENDING_AIRSHIP_PAL_BYTES = 0x100;
const FALCON_FRAMES = 0x2f4e96;  // EE/5196 + $FD00: the table of the frames of the particles, which the game reads past its end for the first one of each frame
export const FALCON_FRAMES_BYTES = 0x308;
const CREDITS_SMALL_FONT = [[0x0487c0, 0x200], [0x048c10, 0x10], [0x048fb0, 0x10]];  // C4/7FC0 + $800, + $C50 and + $FF0: the letters, the period and the space
export const CREDITS_SMALL_FONT_BYTES = 0x220;
const ENDING_COIN = 0x18ec93;  // D8/EC93: the animation and the sprites of the coin of Edgar, in the data of the menus
export const ENDING_COIN_BYTES = 0x1b4;
// The graphics of the sprites of the scenes of the characters (C4/F477, C4/F6FB, D9/9D4B and D9/A4E5, compressed), decompressed.
const ENDING_SPRITE_GFX = [[0x04f477, 0x380], [0x04f6fb, 0xeb0], [0x199d4b, 0x800], [0x19a4e5, 0xc0]];
export const ENDING_SPRITE_GFX_BYTES = ENDING_SPRITE_GFX.reduce((total, [, bytes]) => total + bytes, 0);
const SINE_16 = 0x02fc6d;  // C2/FC6D: 256 signed words
export const SINE_16_BYTES = 0x200;
const MAP_GFX_PTRS = 0x1fda00;
const MAP_GFX = 0x1fdb00;
export const MAP_GFX_BYTES = 0x2000;
// The map graphics that the cutscenes use: the sealed gate (the title), then three of Narshe's (the opening).
export const CUTSCENE_MAP_GFX = [75, 34, 31, 32];
const RNG_TABLE = 0x00fd00;  // C0/FD00
const SINE_TABLE = 0x02fe6d;  // C2/FE6D, signed bytes
export const TABLE_BYTES = 256;

export function readCutsceneProgram(rom) {
  const program = decompressLzss(rom, CUTSCENE_PROG, 0x10000);
  if (program.length !== CUTSCENE_PROG_BYTES) throw new Error('Unexpected cutscene program size.');
  return program;
}

export function readTitleOpeningGfx(rom) {
  const gfx = decompressLzss(rom, TITLE_OPENING_GFX, 0x20000);
  if (gfx.length !== TITLE_OPENING_GFX_BYTES) throw new Error('Unexpected title graphics size.');
  return gfx;
}

export function readRuinGfx(rom) {
  const gfx = decompressLzss(rom, RUIN_GFX, 0x20000);
  if (gfx.length !== RUIN_GFX_BYTES) throw new Error('Unexpected ruin graphics size.');
  return gfx;
}

export function readFloatingGfx(rom) {
  const gfx = decompressLzss(rom, FLOATING_GFX, 0x20000);
  if (gfx.length !== FLOATING_GFX_BYTES) throw new Error('Unexpected floating continent graphics size.');
  return gfx;
}

export function readFlightGfx(rom) {
  const gfx = decompressLzss(rom, FLIGHT_GFX, 0x40000);
  if (gfx.length !== FLIGHT_GFX_BYTES) throw new Error('Unexpected flight graphics size.');
  return gfx;
}

export function readEndingFont(rom) {
  const font = decompressLzss(rom, ENDING_FONT, 0x20000);
  if (font.length !== ENDING_FONT_BYTES) throw new Error('Unexpected ending font size.');
  return font;
}

export function readEndingSpriteGfx(rom) {
  return Buffer.concat(ENDING_SPRITE_GFX.map(([offset, bytes]) => {
    const gfx = decompressLzss(rom, offset, 0x20000);
    if (gfx.length !== bytes) throw new Error('Unexpected ending sprite graphics size.');
    return gfx;
  }));
}

export function readEndingGfx1(rom) {
  const gfx = decompressLzss(rom, ENDING_GFX_1, 0x20000);
  if (gfx.length !== ENDING_GFX_1_BYTES) throw new Error('Unexpected ending graphics size.');
  return gfx;
}

export function readCutsceneMapGfx(rom) {
  return Buffer.concat(CUTSCENE_MAP_GFX.map(index => (
    slice(rom, MAP_GFX + read24(rom, MAP_GFX_PTRS + index * 3), MAP_GFX_BYTES, `Map graphics ${index}`)
  )));
}

// `version` is the version of the whole pack.
export function buildTitleData(rom, version) {
  const header = Buffer.alloc(12);
  header.write(TITLE_MAGIC, 0, 8, 'ascii');
  header.writeUInt32LE(version, 8);
  return Buffer.concat([
    header,
    readCutsceneProgram(rom),
    readTitleOpeningGfx(rom),
    readCutsceneMapGfx(rom),
    slice(rom, RNG_TABLE, TABLE_BYTES, 'Random numbers'),
    slice(rom, SINE_TABLE, TABLE_BYTES, 'Sine table'),
    readRuinGfx(rom),
    readFloatingGfx(rom),
    slice(rom, THE_END_GFX, THE_END_GFX_BYTES, 'The end graphics'),
    slice(rom, THE_END_GFX_2, THE_END_GFX_2_BYTES, 'The end stars'),
    slice(rom, THE_END_GFX_2 + 0x100, THE_END_PAL_BYTES, 'The end palettes'),
    slice(rom, CREDITS, CREDITS_BYTES, 'Credits'),
    readFlightGfx(rom),
    slice(rom, FLIGHT_TABLES, FLIGHT_TABLES_BYTES, 'Flight tables'),
    slice(rom, FLIGHT_PALETTES, FLIGHT_PALETTES_BYTES, 'Flight palettes'),
    slice(rom, FLIGHT_SCROLLS, FLIGHT_SCROLLS_BYTES, 'Flight scrolls'),
    slice(rom, SINE_16, SINE_16_BYTES, 'Sine'),
    decompressLzss(rom, SKY_GFX, 0x20000).subarray(0, SKY_GFX_BYTES),
    decompressLzss(rom, SKY_MAP, 0x20000).subarray(0, SKY_MAP_BYTES),
    slice(rom, SKY_PAL, SKY_PAL_BYTES, 'Sky palette'),
    slice(rom, ENDING_ANIM, ENDING_ANIM_BYTES, 'Ending animations'),
    readEndingFont(rom),
    readEndingGfx1(rom),
    slice(rom, ENDING_ANIM_2, ENDING_ANIM_2_BYTES, 'Ending animations 2'),
    readEndingSpriteGfx(rom),
    slice(rom, ENDING_COIN, ENDING_COIN_BYTES, 'Ending coin'),
    slice(rom, ENDING_PHASE, ENDING_PHASE_BYTES, 'Ending bird phases'),
    slice(rom, ENDING_AIRSHIP_PAL, ENDING_AIRSHIP_PAL_BYTES, 'Ending airship palette'),
    slice(rom, FALCON_FRAMES, FALCON_FRAMES_BYTES, 'Falcon particle frames'),
    ...CREDITS_SMALL_FONT.map(([offset, bytes]) => slice(rom, offset, bytes, 'Credits small font')),
  ]);
}
