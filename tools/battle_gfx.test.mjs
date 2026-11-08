import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import {
  BG_COUNT, BG_GFX_BYTES, BG_GFX_COUNT, BG_PAL_BYTES, BG_PROP_BYTES, BG_TILES_BYTES, BG_TILES_COUNT, readBgGfx, readBgProp,
  readBgTiles,
} from './battle/battle_bg.mjs';
import {
  CHAR_ACTION_COUNT, CHAR_ACTION_TILES, CHAR_GFX_BYTES, CHAR_GFX_COUNT, CHAR_PAL_BYTES, CHAR_PAL_COUNT, CHAR_PAL_TABLE_BYTES,
  readCharActions, readCharGfx, readCharPalTable,
} from './battle/battle_chars.mjs';
import {
  BATTLE_GFX_MAGIC, MONSTER_GFX_BYTES, MONSTER_GFX_COUNT, MONSTER_GFX_PROP_BYTES, MONSTER_PAL_BYTES, STENCIL_LARGE_BYTES,
  STENCIL_LARGE_COUNT, STENCIL_SMALL_BYTES, STENCIL_SMALL_COUNT, buildBattleGfx, readMonsterGfx, readMonsterGfxProp, readStencils,
} from './battle/battle_gfx.mjs';
import { requireSupportedRom } from './rom.mjs';

const romPath = process.env.FF6_ROM;
const skip = !romPath;
const loadRom = () => requireSupportedRom(readFileSync(romPath)).bytes;

const bits = bytes => [...bytes].reduce((sum, byte) => sum + byte.toString(2).replace(/0/g, '').length, 0);

test('the graphics of the monsters fill the room of their tiles, one stencil after the other', { skip }, () => {
  const rom = loadRom();
  const props = readMonsterGfxProp(rom), stencils = readStencils(rom);
  const entries = [];
  for (let i = 0; i < MONSTER_GFX_COUNT; i++) {
    const at = i * MONSTER_GFX_PROP_BYTES;
    const gfx = props.readUInt16LE(at), pal = props.readUInt16BE(at + 2), stencil = props[at + 4];
    const large = (pal & 0x8000) !== 0;
    const rows = large
      ? stencils.large.subarray(stencil * STENCIL_LARGE_BYTES, (stencil + 1) * STENCIL_LARGE_BYTES)
      : stencils.small.subarray(stencil * STENCIL_SMALL_BYTES, (stencil + 1) * STENCIL_SMALL_BYTES);
    entries.push({ start: (gfx & 0x7fff) * 8, tileBytes: gfx & 0x8000 ? 24 : 32, tiles: bits(rows), palette: (pal & 0x3ff) * 16 });
  }
  const sorted = [...entries].sort((a, b) => a.start - b.start);
  let end = 0;
  for (let i = 0; i < sorted.length; i++) {
    const at = sorted[i];
    end = Math.max(end, at.start + at.tiles * at.tileBytes);
    const next = sorted.slice(i + 1).find(other => other.start !== at.start);
    if (next) assert.equal(next.start - at.start, at.tiles * at.tileBytes);
  }
  const gfx = readMonsterGfx(rom);
  assert.ok(end < MONSTER_GFX_BYTES && gfx.subarray(end).every(byte => byte === 0));  // then the block is zero padded
  assert.ok(entries.every(entry => entry.palette + 32 <= MONSTER_PAL_BYTES));
});

test('battle graphics hold the tables one after the other', { skip }, () => {
  const data = buildBattleGfx(loadRom(), 6);
  assert.equal(data.subarray(0, 8).toString('ascii'), BATTLE_GFX_MAGIC);
  assert.equal(data.readUInt32LE(8), 6);
  assert.equal(data.length, 12 + MONSTER_GFX_COUNT * MONSTER_GFX_PROP_BYTES + MONSTER_PAL_BYTES
    + STENCIL_SMALL_COUNT * STENCIL_SMALL_BYTES + STENCIL_LARGE_COUNT * STENCIL_LARGE_BYTES + MONSTER_GFX_BYTES
    + BG_COUNT * (BG_PROP_BYTES + BG_PAL_BYTES) + BG_GFX_COUNT * BG_GFX_BYTES + BG_TILES_COUNT * BG_TILES_BYTES
    + CHAR_GFX_COUNT * CHAR_GFX_BYTES + CHAR_ACTION_COUNT * CHAR_ACTION_TILES * 2 + CHAR_PAL_TABLE_BYTES
    + CHAR_PAL_COUNT * CHAR_PAL_BYTES);
});

test('the characters of a battle are posed with tiles of their map sprite', { skip }, () => {
  const rom = loadRom();
  const actions = readCharActions(rom), gfx = readCharGfx(rom), palettes = readCharPalTable(rom);
  assert.equal(actions.readUInt16LE(0), 0x0ae0);  // the first tile of the first pose
  assert.equal(actions.readUInt16LE((17 * CHAR_ACTION_TILES + 2) * 2), 0x03c0);  // the pose of the fight
  for (let tile = 0; tile < CHAR_ACTION_COUNT * CHAR_ACTION_TILES; tile++) {
    const offset = actions.readUInt16LE(tile * 2);
    assert.ok(offset === 0xffff || offset + 32 <= CHAR_GFX_BYTES, `tile ${tile} at ${offset}`);
  }
  assert.ok(gfx.subarray(0x3c0, 0x3c0 + 32 * 6).some(byte => byte !== 0));  // the tiles of Terra's fight pose
  assert.deepEqual([...palettes.subarray(0, 14)], [2, 1, 4, 4, 0, 0, 0, 3, 3, 4, 5, 3, 3, 5]);  // from Terra to Umaro
});

test('the backgrounds point at blocks of graphics and tilemaps that exist', { skip }, () => {
  const rom = loadRom();
  const props = readBgProp(rom), gfx = readBgGfx(rom), tiles = readBgTiles(rom);
  assert.equal(gfx.length, BG_GFX_COUNT * BG_GFX_BYTES);
  assert.equal(tiles.length, BG_TILES_COUNT * BG_TILES_BYTES);
  for (let bg = 0; bg < 53; bg++) {
    const at = bg * BG_PROP_BYTES;
    if (props[at] === 0xff) continue;  // no graphics at all
    assert.ok((props[at] & 0x7f) < BG_GFX_COUNT, `background ${bg} block ${props[at]}`);
    assert.ok(props[at + 3] < BG_TILES_COUNT);
    const block = gfx.subarray((props[at] & 0x7f) * BG_GFX_BYTES, (props[at] & 0x7f) * BG_GFX_BYTES + 0x1000);
    assert.ok(block.some(byte => byte !== 0), `background ${bg} has graphics`);
    const map = tiles.subarray(props[at + 3] * BG_TILES_BYTES, (props[at + 3] + 1) * BG_TILES_BYTES);
    assert.ok(map.some(byte => byte !== 0), `background ${bg} has a tilemap`);
  }
});
