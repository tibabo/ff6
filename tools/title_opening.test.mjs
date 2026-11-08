import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import {
  CUTSCENE_MAP_GFX, CUTSCENE_PROG_BYTES, MAP_GFX_BYTES, TABLE_BYTES, TITLE_MAGIC, TITLE_OPENING_GFX_BYTES, buildTitleData,
  readCutsceneProgram, readTitleOpeningGfx,
} from './cutscene/title_opening.mjs';
import { requireSupportedRom } from './rom.mjs';

const romPath = process.env.FF6_ROM;
const skip = !romPath;
const loadRom = () => requireSupportedRom(readFileSync(romPath)).bytes;

test('the cutscene program starts with the jumps to the title and the opening', { skip }, () => {
  const program = readCutsceneProgram(loadRom());
  assert.equal(program.length, CUTSCENE_PROG_BYTES);
  assert.deepEqual([...program.subarray(0, 3)], [0x4c, 0x0c, 0x50]);  // jmp TitleMain ($500c)
  assert.deepEqual([...program.subarray(3, 6)], [0x4c, 0xf3, 0x57]);  // jmp OpeningMain ($57f3)
  // Tables that the engine reads by their address in the program: the black palette and the first credits.
  assert.ok(program.subarray(0x7465 - 0x5000, 0x7465 - 0x5000 + 32).every(byte => byte === 0));
  assert.equal(program.readUInt16LE(0x6598 - 0x5000), 0x4458);  // CreditsText_00: where "producer" goes
});

test('the title graphics decompress to the size that the game transfers', { skip }, () => {
  const gfx = readTitleOpeningGfx(loadRom());
  assert.equal(gfx.length, TITLE_OPENING_GFX_BYTES);
  assert.ok(gfx.subarray(0, 0x940).some(byte => byte !== 0));  // the armors
});

test('the title data holds the program, the graphics, the map graphics and two tables', { skip }, () => {
  const data = buildTitleData(loadRom(), 11);
  assert.equal(data.subarray(0, 8).toString('ascii'), TITLE_MAGIC);
  assert.equal(data.readUInt32LE(8), 11);
  assert.equal(data.length, 12 + CUTSCENE_PROG_BYTES + TITLE_OPENING_GFX_BYTES + CUTSCENE_MAP_GFX.length * MAP_GFX_BYTES + 2 * TABLE_BYTES);
  const tables = data.subarray(data.length - 2 * TABLE_BYTES);
  assert.equal(tables[0], 0x07);  // the table of random numbers
  assert.equal(tables[TABLE_BYTES + 64], 0x7f);  // sin(90 degrees) of the sine table
  assert.equal(tables[TABLE_BYTES + 192], 0x81);  // and -sin
});
