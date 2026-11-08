// src/cutscene/title.asm, opening.asm and cutscene_main.asm: the title screen and the magitek march.
// The cutscene program runs from $7e5000 in the game, so its labels are addresses from 0x5000 on; the engine reads its
// tables (palettes, sprite frames, credits) at those addresses.
import { decompressLzss, read24, slice } from '../common/bytes.mjs';

export const TITLE_MAGIC = 'FF6TITL1';
const CUTSCENE_PROG = 0x02686c;  // C2/686C, compressed
export const CUTSCENE_PROG_BYTES = 11363;
const TITLE_OPENING_GFX = 0x18f000;  // D8/F000, compressed
export const TITLE_OPENING_GFX_BYTES = 44712;
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
  ]);
}
