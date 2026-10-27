import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import {
  CHAR_PROP_BYTES, CHAR_PROP_COUNT, ITEM_PROP_BYTES, ITEM_PROP_COUNT, LEVEL_COUNT, RULES_MAGIC, SHOP_COUNT, SHOP_PROP_BYTES,
  NATURAL_MAGIC_BYTES, MAGIC_PROP_BYTES, MAGIC_PROP_COUNT, buildRulesData, readAbilityLevels, readMagicProp, readCharProp, readItemProp, readLevelUp, readShopProp,
} from './data/game_rules.mjs';
import { requireSupportedRom } from './rom.mjs';

const romPath = process.env.FF6_ROM;
const skip = !romPath;
const loadRom = () => requireSupportedRom(readFileSync(romPath)).bytes;

test('character properties start with the values of Terra', { skip }, () => {
  const terra = readCharProp(loadRom()).subarray(0, CHAR_PROP_BYTES);
  assert.equal(terra[0], 40);  // hp
  assert.equal(terra[1], 16);  // mp
  assert.deepEqual([...terra.subarray(6, 10)], [31, 33, 28, 39]);  // vigor, speed, stamina, magic power
  assert.equal(terra[15], 1);  // weapon
});

test('item properties start with the dirk, and the potion is a consumable', { skip }, () => {
  const items = readItemProp(loadRom());
  const dirk = items.subarray(0, ITEM_PROP_BYTES);
  assert.equal(dirk[0] & 7, 1);  // weapon
  assert.equal(dirk[20], 26);  // battle power
  assert.equal(items[233 * ITEM_PROP_BYTES] & 7, 6);  // consumable
});

test('level tables give the first and last steps', { skip }, () => {
  const levels = readLevelUp(loadRom());
  assert.equal(levels.length, LEVEL_COUNT * 4);
  assert.equal(levels.readUInt16LE(0), 4);
  assert.equal(levels.readUInt16LE((LEVEL_COUNT - 1) * 2), 11111);
  assert.equal(levels[LEVEL_COUNT * 2], 11);  // hp of level 2
  assert.equal(levels[LEVEL_COUNT * 2 + LEVEL_COUNT - 1], 88);
  assert.equal(levels[LEVEL_COUNT * 3], 4);  // mp of level 2
  assert.equal(levels[LEVEL_COUNT * 3 + LEVEL_COUNT - 1], 13);
});

test('shop 0 sells the weapons of Narshe', { skip }, () => {
  const shop = readShopProp(loadRom()).subarray(0, SHOP_PROP_BYTES);
  assert.equal(shop[0] & 7, 1);  // weapons, no price modifier
  assert.equal(shop[0] >> 3, 0);
  assert.notEqual(shop[1], 0xff);  // seven items, the eighth place is empty
  assert.equal(shop[8], 0xff);
});

test('game data holds the tables one after the other', { skip }, () => {
  const data = buildRulesData(loadRom(), 4);
  assert.equal(data.subarray(0, 8).toString('ascii'), RULES_MAGIC);
  assert.equal(data.readUInt32LE(8), 4);
  assert.equal(data.length, 12 + CHAR_PROP_COUNT * CHAR_PROP_BYTES + ITEM_PROP_COUNT * ITEM_PROP_BYTES + LEVEL_COUNT * 4
    + SHOP_COUNT * SHOP_PROP_BYTES + NATURAL_MAGIC_BYTES + 16 + MAGIC_PROP_COUNT * MAGIC_PROP_BYTES);
});

test('magic properties: fire costs 4 mp, cure 5', { skip }, () => {
  const magic = readMagicProp(loadRom());
  assert.equal(magic.length, MAGIC_PROP_COUNT * MAGIC_PROP_BYTES);
  assert.equal(magic[0 * MAGIC_PROP_BYTES + 5], 4);
  assert.equal(magic[45 * MAGIC_PROP_BYTES + 5], 5);
});

test('ability tables: the natural magic of Terra and Celes and the levels of the techniques', { skip }, () => {
  const data = readAbilityLevels(loadRom());
  assert.equal(data.length, NATURAL_MAGIC_BYTES + 16);
  assert.deepEqual([...data.subarray(0, 4)], [45, 1, 0, 3]);  // Terra: cure at level 1, then fire at level 3
  assert.deepEqual([...data.subarray(32, 36)], [1, 1, 45, 4]);  // Celes: ice, then cure at level 4
  assert.deepEqual([...data.subarray(64, 72)], [1, 6, 12, 15, 24, 34, 44, 70]);  // swordtechs
  assert.deepEqual([...data.subarray(72, 80)], [1, 6, 10, 15, 23, 30, 42, 70]);  // blitzes
});
