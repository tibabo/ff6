import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { MENU_MAGIC, MENU_SPRITE_BYTES, buildMenuData, readCursorPalette, readCursorTiles, readItemDescriptions, readMenuFont, readMenuPalette, readMenuSprites, readRareItems, readSkillNames } from './menu/menu_gfx.mjs';
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
  assert.equal(data.length, 12 + 4096 + 160 + 128 + 8 + 19 * 0x320 + 19 * 32 + 8 * 0x380 + 8 * 14 + 256 * 13 + 256 * 2 + 0x13a0 + 54 * 7 + 27 * 8 + 16 * 10 + MENU_SPRITE_BYTES + 8 + 20 * 13 + 20 * 2 + 0x260);
  const sprites = readMenuSprites(rom);  // the status icons start at the tile 0x20, and their palette has four colors
  assert.equal(sprites.length, MENU_SPRITE_BYTES + 8);
  assert.ok(sprites.subarray(0x20 * 32, 0x21 * 32).some(byte => byte !== 0));
  assert.deepEqual([...sprites.subarray(MENU_SPRITE_BYTES, MENU_SPRITE_BYTES + 4)], [0x84, 0x10, 0xff, 0x7f]);
  const names = readSkillNames(rom);  // Fire, Ramuh, Dispatch and Pummel come first in their lists
  assert.equal(names[1], 0x85);  // F, after the symbol
  assert.equal(names[54 * 7], 0x91);  // R
  assert.equal(names[54 * 7 + 27 * 8], 0x83);  // D
  assert.equal(names[54 * 7 + 27 * 8 + 80], 0x8f);  // P
  const descriptions = readItemDescriptions(rom);  // the potion recovers 250 HP
  const start = descriptions.readUInt16LE(233 * 2);
  assert.equal(descriptions[256 * 2 + start], 0x91);  // R
  assert.equal(descriptions[256 * 2 + start + 15], 0);
  const rare = readRareItems(rom);  // the cider, its description starts after the pointers
  assert.equal(rare[0], 0x82);  // C
  assert.equal(rare[20 * 13 + 20 * 2 + rare.readUInt16LE(20 * 13 + 2) - 1], 0);
});
