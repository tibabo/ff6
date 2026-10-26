import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { MENU_MAGIC, buildMenuData, readCursorPalette, readCursorTiles, readMenuFont, readMenuPalette } from './menu/menu_gfx.mjs';
import { requireSupportedRom } from './rom.mjs';

const romPath = process.env.FF6_ROM;
const skip = !romPath;
const loadRom = () => requireSupportedRom(readFileSync(romPath)).bytes;

test('menu data holds the font, the palettes and the hand cursor', { skip }, () => {
  const rom = loadRom();
  const font = readMenuFont(rom);
  assert.equal(font.length, 256 * 16);
  // The tile of the letter A (code 0x80) has ink in its first rows and nothing blank at the end of the row.
  assert.ok(font.subarray(0x80 * 16, 0x81 * 16).some(byte => byte !== 0));
  assert.deepEqual([...font.subarray(0, 16)], new Array(16).fill(0));  // the first tile is blank
  assert.equal(readMenuPalette(rom).length, 160);
  assert.equal(readCursorTiles(rom).length, 128);
  assert.equal(readCursorPalette(rom).length, 8);
  const data = buildMenuData(rom, 4);
  assert.equal(data.subarray(0, 8).toString('ascii'), MENU_MAGIC);
  assert.equal(data.readUInt32LE(8), 4);
  assert.equal(data.length, 12 + 4096 + 160 + 128 + 8 + 19 * 0x320 + 19 * 32 + 8 * 0x380 + 8 * 14 + 256 * 13);
});
