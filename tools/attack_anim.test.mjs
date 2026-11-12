import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import {
  ATTACK_ANIM_MAGIC, ATTACK_ANIM_PROP_BYTES, ATTACK_ANIM_PROP_COUNT, ATTACK_ANIM_SCRIPT_BYTES, ATTACK_ANIM_SCRIPT_COUNT,
  ATTACK_FRAME_END, ATTACK_FRAME_FIRST, ATTACK_FRAME_PTR_COUNT, ATTACK_GFX_2BPP_BYTES, ATTACK_GFX_3BPP_BYTES, ATTACK_GFX_PROP_BYTES,
  ATTACK_MODE7_GFX_BYTES, ATTACK_MODE7_MAP_BYTES, ATTACK_PAL_BYTES, ATTACK_TILES_2BPP_BYTES, ATTACK_TILES_3BPP_BYTES,
  ATTACK_WEAPON_PROP_BYTES, ATTACK_WEAPON_PROP_COUNT, ATTACK_RNG_BYTES, ATTACK_ROW_SPEED_BYTES, ATTACK_LIGHTS_BYTES, ATTACK_EVENT_PAL_BYTES, readAttackEventPal, buildAttackAnim,
  readAttackAnimProp, readAttackFramePtrs, readAttackGfxProp, readAttackLights, readAttackMode7Gfx, readAttackMode7Map, readAttackRng,
  readAttackRowSpeeds, readAttackScriptPtrs, readAttackScripts, readWeaponAnimProp,
} from './battle/attack_anim.mjs';
import { requireSupportedRom } from './rom.mjs';

const romPath = process.env.FF6_ROM;
const skip = !romPath;
const loadRom = () => requireSupportedRom(readFileSync(romPath)).bytes;

const FIRE_SPRITE = 0xc1;  // ATTACK_ANIM_SCRIPT::FIRE_SPRITE, and the sprite script of the attack 0 (fire)

test('the properties of the attacks start with fire, a script of frames and a palette', { skip }, () => {
  const props = readAttackAnimProp(loadRom());
  assert.equal(props.readUInt16LE(0), FIRE_SPRITE);  // the sprite script
  assert.equal(props.readUInt16LE(2), 0xffff);  // no bg1 script
  assert.equal(props[6], 56);  // sprite palette
  assert.equal(props[10], 37);  // init function
  assert.equal(props.readUInt16LE(11), 0xffff);  // no extra script
  assert.equal(props[13], 16);  // delay between the targets
  assert.equal(props.length, ATTACK_ANIM_PROP_COUNT * ATTACK_ANIM_PROP_BYTES);
});

test('the scripts are found by pointers, and fire is the sprite script that moves two flames apart', { skip }, () => {
  const rom = loadRom();
  const ptrs = readAttackScriptPtrs(rom), scripts = readAttackScripts(rom);
  for (let i = 0; i < ATTACK_ANIM_SCRIPT_COUNT; i++) assert.ok(ptrs.readUInt16LE(i * 2) < ATTACK_ANIM_SCRIPT_BYTES, `script ${i}`);
  const at = ptrs.readUInt16LE(FIRE_SPRITE * 2);
  // speed 5 and bottom, priority 0, jump per thread (3 threads), then the first thread: sfx, target palette, move back 26
  assert.deepEqual([...scripts.subarray(at, at + 5)], [0x40, 0x00, 0x90, 0x00, 0xeb]);
  const first = scripts.subarray(scripts.readUInt16LE(at + 5));
  assert.equal(first[0], 0xc9);  // sound effect
  assert.deepEqual([...first.subarray(2, 6)], [0x80, 0x3b, 0x83, 0x80 | 25]);  // target palette, move back 26
  // the second thread plays frames 0 to 4 by a loop whose frame offset grows
  const second = scripts.subarray(scripts.readUInt16LE(at + 7));
  assert.deepEqual([...second.subarray(0, 7)], [0x8b, 0x04, 0x00, 0x80, 0x3c, 0x8c, 0xff]);
  // the first one goes on by the same loop
  assert.deepEqual([...first.subarray(6, 8)], [0x8b, 0x04]);
  // the third moves forward 26 before the loop
  assert.deepEqual([...scripts.subarray(scripts.readUInt16LE(at + 9), scripts.readUInt16LE(at + 9) + 4)], [0x83, 0x60 | 25, 0x8b, 0x04]);
});

test('the sheets of the animations are described by their first frame, size and tiles', { skip }, () => {
  const props = readAttackGfxProp(loadRom());
  assert.equal(props.length, ATTACK_ANIM_SCRIPT_COUNT * ATTACK_GFX_PROP_BYTES);
  assert.deepEqual([...props.subarray(0, 6)], [0x8d, 0x20, 0, 0, 4, 4]);  // 13 frames of 2 bits
  assert.deepEqual([...props.subarray(6, 12)], [0x88, 0x22, 13, 0, 4, 4]);
  for (let i = 0; i < ATTACK_ANIM_SCRIPT_COUNT; i++) {
    const at = i * ATTACK_GFX_PROP_BYTES;
    assert.ok(props.readUInt16LE(at + 2) + (props[at] & 0x3f) < ATTACK_FRAME_PTR_COUNT, `gfx ${i}`);
  }
});

test('the frames start at the first byte of their data and end where the table says', { skip }, () => {
  const ptrs = readAttackFramePtrs(loadRom());
  assert.equal(ptrs.readUInt16LE(0), ATTACK_FRAME_FIRST);
  assert.equal(ptrs.readUInt16LE((ATTACK_FRAME_PTR_COUNT - 1) * 2), ATTACK_FRAME_END);
  for (let i = 1; i < ATTACK_FRAME_PTR_COUNT; i++) {  // frames may be shared, so the order is not checked
    const ptr = ptrs.readUInt16LE(i * 2);
    assert.ok(ptr >= ATTACK_FRAME_FIRST && ptr <= ATTACK_FRAME_END, `frame ${i}`);
  }
});

test('attack animations hold the tables one after the other', { skip }, () => {
  const data = buildAttackAnim(loadRom(), 8);
  assert.equal(data.subarray(0, 8).toString('ascii'), ATTACK_ANIM_MAGIC);
  assert.equal(data.readUInt32LE(8), 8);
  assert.equal(data.length, 12 + ATTACK_ANIM_PROP_COUNT * ATTACK_ANIM_PROP_BYTES + ATTACK_ANIM_SCRIPT_COUNT * (ATTACK_GFX_PROP_BYTES + 2)
    + ATTACK_ANIM_SCRIPT_BYTES + ATTACK_FRAME_PTR_COUNT * 2 + ATTACK_FRAME_END - ATTACK_FRAME_FIRST + ATTACK_TILES_3BPP_BYTES
    + ATTACK_PAL_BYTES + ATTACK_TILES_2BPP_BYTES + ATTACK_GFX_3BPP_BYTES + ATTACK_GFX_2BPP_BYTES
    + ATTACK_WEAPON_PROP_COUNT * ATTACK_WEAPON_PROP_BYTES + ATTACK_MODE7_GFX_BYTES + ATTACK_MODE7_MAP_BYTES
    + ATTACK_RNG_BYTES + ATTACK_ROW_SPEED_BYTES + ATTACK_LIGHTS_BYTES + ATTACK_EVENT_PAL_BYTES);
});

test('the random numbers, the row speeds and the palettes of the lights are those of the game', { skip }, () => {
  const rom = loadRom();
  const rng = readAttackRng(rom);
  assert.deepEqual([...rng.subarray(0, 4)], [0x07, 0xb6, 0xf0, 0x1f]);
  const speeds = readAttackRowSpeeds(rom);
  assert.deepEqual([...speeds.subarray(0, 4)].map(v => (v << 24) >> 24), [2, 2, 2, 2]);  // toward the back row on a normal battle
  assert.deepEqual([...speeds.subarray(16, 20)].map(v => (v << 24) >> 24), [-2, -2, -2, -2]);  // and toward the front row
  const lights = readAttackLights(rom);
  assert.deepEqual([...lights.subarray(0, 6)], [0x00, 0x00, 0xad, 0x31, 0x0a, 0x19]);  // on
  assert.deepEqual([...lights.subarray(96, 102)], [0x00, 0x00, 0x84, 0x0c, 0x44, 0x0c]);  // off
  const sparkles = readAttackEventPal(rom);
  assert.equal(sparkles.length, ATTACK_EVENT_PAL_BYTES);
  assert.deepEqual([...sparkles.subarray(0, 6)], [0x29, 0x25, 0x43, 0x08, 0xfe, 0x7f]);  // the first of the five
});

test('the mode 7 pictures are three sets of tiles and three maps of 32 x 32 tiles', { skip }, () => {
  const rom = loadRom();
  const gfx = readAttackMode7Gfx(rom), map = readAttackMode7Map(rom);
  assert.equal(gfx.length, ATTACK_MODE7_GFX_BYTES);
  assert.equal(map.length, ATTACK_MODE7_MAP_BYTES);
  assert.deepEqual([...map.subarray(0, 8)], [0, 1, 2, 3, 4, 5, 6, 7]);  // the first row of the first map
  assert.ok(map.every(tile => tile <= 0x3f));  // each set holds 63 tiles; the 64th is the empty one
});

test('the weapons start with the dirk: a script for each hand, a palette, a hit script and a sound', { skip }, () => {
  const props = readWeaponAnimProp(loadRom());
  assert.deepEqual([...props.subarray(0, 8)], [0x24, 0x24, 0x23, 0x08, 0x36, 0x00, 0x33, 0x00]);
  assert.deepEqual([...props.subarray(0x5d * 8, 0x5d * 8 + 4)], [0x41, 0x41, 0x00, 0x08]);  // the first attack of a monster
});
