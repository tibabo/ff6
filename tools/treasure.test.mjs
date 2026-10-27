import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { TREASURE_PROP_SIZE, readTreasureProps } from './event/treasure_prop.mjs';
import { requireSupportedRom } from './rom.mjs';

const romPath = process.env.FF6_ROM;
const skip = !romPath;
const loadRom = () => requireSupportedRom(readFileSync(romPath)).bytes;

test('treasure data of a map lists its chests', { skip }, () => {
  const rom = loadRom();
  assert.equal(readTreasureProps(rom, 0).length, 0);
  const chests = readTreasureProps(rom, 38);  // two chests: a potion-like item at (65, 28) and one at (52, 33)
  assert.equal(chests.length, 2 * TREASURE_PROP_SIZE);
  assert.deepEqual([...chests.subarray(0, 5)], [0x41, 0x1c, 0x0b, 0x40, 0xc8]);
  assert.throws(() => readTreasureProps(rom, 415), /Invalid map/);
});
