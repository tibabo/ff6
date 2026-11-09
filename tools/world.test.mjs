import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { decompressLzss } from './common/bytes.mjs';
import { requireSupportedRom } from './rom.mjs';
import {
  ANIM_DATA_BYTES, ANIM_GFX_BYTES, ANIM_PTR_BYTES, AIRSHIP_GFX_BYTES, BACKDROP_GFX_BYTES, BACKDROP_TILES_BYTES,
  CHR_BYTES, FORMATION_BYTES, MINIMAP_GFX_BYTES, MISC_SPRITE_GFX_BYTES, PALETTE_BYTES, TILEMAP_BYTES, TILE_PROP_BYTES,
  WORLD_BATTLE_GROUP_BYTES, WORLD_BATTLE_RATE_BYTES, WORLD_MAGIC, buildChr, buildWorldData, readModifications,
} from './world/world.mjs';

const romPath = process.env.FF6_ROM;
const skip = !romPath;
const loadRom = () => requireSupportedRom(readFileSync(romPath)).bytes;

test('a character takes the palette of its pair and keeps two pixels in each byte', () => {
  const gfx = Buffer.alloc(FORMATION_BYTES + 256 * 32 + 128);
  gfx[FORMATION_BYTES] = 0x5a;  // character 0: pixels 0xa then 0x5
  gfx[FORMATION_BYTES + 256 * 32] = 0x21;  // palettes: 1 for character 0, 2 for character 1
  gfx[FORMATION_BYTES + 32] = 0x03;
  const chr = buildChr(gfx);
  assert.equal(chr.length, CHR_BYTES);
  assert.deepEqual([...chr.subarray(0, 2)], [0x1a, 0x15]);
  assert.deepEqual([...chr.subarray(64, 66)], [0x23, 0x20]);
});

test('the modifications of the maps are rectangles that wait for an event bit', { skip }, () => {
  const rom = loadRom();
  const balance = readModifications(rom, 0);
  assert.equal(balance.length, 15);
  assert.deepEqual([balance[0].bit, balance[0].x, balance[0].y, balance[0].width, balance[0].height], [0x10b, 64, 75, 2, 2]);
  assert.deepEqual([...balance[0].tiles], [0x37, 0x38, 0x47, 0x48]);  // Figaro castle in the desert
  const ruin = readModifications(rom, 1);
  assert.equal(ruin.length, 3);
  assert.deepEqual([...ruin[2].tiles], [0x41, 0x64]);  // Ebot's rock
});

test('the world data holds both worlds and the tables that they share', { skip }, () => {
  const rom = loadRom();
  const data = buildWorldData(rom, 14);
  assert.equal(data.subarray(0, 8).toString('ascii'), WORLD_MAGIC);
  assert.equal(data.readUInt32LE(8), 14);
  // The first world starts with the map, whose tiles are the ones that the game decompresses.
  const map = decompressLzss(rom, 0x2ed434, 0x20000);
  assert.equal(map.length, TILEMAP_BYTES);
  assert.ok(data.subarray(12, 12 + TILEMAP_BYTES).equals(map));
  const props = 12 + TILEMAP_BYTES + FORMATION_BYTES + CHR_BYTES + 2 * PALETTE_BYTES;
  assert.equal(data.readUInt16LE(props), 0x0004);  // WorldTileProp
  assert.equal(data.readUInt16LE(props + 6 * 2), 0x0053);
  const tail = data.subarray(data.length - WORLD_BATTLE_GROUP_BYTES - WORLD_BATTLE_RATE_BYTES);
  assert.equal(tail.length, 0x280);
  // After the battle tables: the frames of the animated sprites, then the graphics of the airship and of the sky.
  const frames = data.subarray(data.length - (BACKDROP_TILES_BYTES + BACKDROP_GFX_BYTES + 2 * AIRSHIP_GFX_BYTES + ANIM_GFX_BYTES + ANIM_DATA_BYTES + ANIM_PTR_BYTES), data.length - (BACKDROP_TILES_BYTES + BACKDROP_GFX_BYTES + 2 * AIRSHIP_GFX_BYTES + ANIM_GFX_BYTES));
  assert.equal(frames.readUInt16LE(4), 0x31);  // the second frame of the airship starts after the first one
  assert.equal(frames[ANIM_PTR_BYTES], 12);  // which has twelve sprites
  assert.ok(MISC_SPRITE_GFX_BYTES > MINIMAP_GFX_BYTES && TILE_PROP_BYTES === 0x200);
});
