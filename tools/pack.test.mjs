import assert from 'node:assert/strict';
import { existsSync, mkdtempSync, readFileSync, readdirSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { test } from 'node:test';
import { decompressLzss } from './common/bytes.mjs';
import { readMapParallax } from './data/map_parallax.mjs';
import { readBg3Anim, readBgAnim } from './field/anim.mjs';
import { readMapColorMath } from './field/scroll.mjs';
import { readLongEntrances, readShortEntrances } from './field/entrance.mjs';
import { MAP_PROP_SIZE, loadLayout, readMapProperties } from './field/map.mjs';
import { SPRITE_GFX_COUNT, buildSpriteGfx, spriteGfxOffset } from './field/obj.mjs';
import { encodeDialogueForEngine } from './field/text.mjs';
import { decodeNpcProp, readNpcProps, readNpcSwitchDefaults } from './event/npc_prop.mjs';
import { readEventScript, readEventTriggers } from './event/event_trigger.mjs';
import { COMMON_MAGIC, MAP_MAGIC, PACK_VERSION, buildCommonData, buildMapData, writePack } from './pack.mjs';
import { requireSupportedRom } from './rom.mjs';

const romPath = process.env.FF6_ROM;
const skip = !romPath;
const loadRom = () => requireSupportedRom(readFileSync(romPath)).bytes;

test('LZSS expands literals and overlapping ring references', () => {
  const block = Buffer.from([8, 0, 0x07, 0x41, 0x42, 0x43, 0xde, 0x1f]);
  assert.equal(decompressLzss(block, 0).toString('ascii'), 'ABCABCABC');
  assert.throws(() => decompressLzss(Buffer.from([4, 0, 0x00, 0xde]), 0), /Truncated LZSS/);
  assert.throws(() => decompressLzss(Buffer.from([9, 0, 0x01]), 0), /Invalid LZSS block length/);
  assert.throws(() => decompressLzss(block, 0, 4), /exceeds/);
});

test('dialogue encoding rejects characters missing from the ROM font unless lenient', () => {
  const rom = Buffer.alloc(0);
  const tokens = ['A', { type: 'control', code: 1, name: 'newline', args: [] }, { type: 'control', code: 0x19, name: 'gp', args: [] }];
  assert.deepEqual([...encodeDialogueForEngine(rom, tokens)], [0x20, 1, 0]);
  assert.throws(() => encodeDialogueForEngine(rom, ['\u00e9']), /missing from the ROM font: \u00e9/);
  assert.deepEqual([...encodeDialogueForEngine(rom, ['\u00e9'], { lenient: true })], [0x5f, 0]);
});

test('npc properties decode every field of the 9-byte item', () => {
  const npc = decodeNpcProp(Buffer.from([0xa7, 0xd2, 0x06, 0xc0, 0x08, 0x66, 0x36, 0x00, 0x02]));
  assert.deepEqual(npc, {
    eventOffset: 0x2d2a7, palette: 1, scrollBg2: false, switchId: 768, x: 8, special: false, y: 38, speed: 1,
    gfx: 54, movement: 0, direction: 2, noTurn: false, animFrame: 0,
  });
});

test('the Narshe inn data matches the disassembly', { skip }, () => {
  const rom = loadRom();
  const props = readNpcProps(rom, 28);
  assert.equal(props.length, 9);
  const npc = decodeNpcProp(props);
  assert.equal(npc.x, 8);
  assert.equal(npc.y, 38);
  assert.equal(npc.gfx, 54);
  assert.equal(readEventTriggers(rom, 28).length, 5);
  assert.deepEqual([...readShortEntrances(rom, 28)], [8, 46, 0x14, 0x20, 0x29, 0x26]);
  assert.equal(readLongEntrances(rom, 28).length, 0);
  // The innkeeper's script starts with the dialogue command, then a choice.
  const script = readEventScript(rom);
  assert.deepEqual([...script.subarray(npc.eventOffset, npc.eventOffset + 4)], [0x4b, 0x0d, 0x06, 0xb6]);
  // The npc is visible in a new game.
  const defaults = readNpcSwitchDefaults(rom);
  assert.equal(defaults.length, 128);
  assert.equal((defaults[npc.switchId >> 3] >> (npc.switchId & 7)) & 1, 1);
});

test('parallax, animation and sprite tables read the documented layouts', { skip }, () => {
  const rom = loadRom();
  assert.deepEqual([...readMapParallax(rom, 0)], [0, 0, 0, 0, 16, 16, 16, 16]);
  assert.deepEqual([...readMapParallax(rom, 1)], [0, 0, 0, 0, 8, 0, 32, 0]);  // phantom forest
  assert.deepEqual([...readMapParallax(rom, 3)], [0xd0, 0, 0, 0, 8, 16, 16, 16]);  // train
  // Narshe adds bg3 to the picture; most bg3 maps blend it at half strength; unused indices mean no color math.
  assert.deepEqual([...readMapColorMath(rom, 5)], [0x04, 0x17, 0x13]);
  assert.deepEqual([...readMapColorMath(rom, 1)], [0x44, 0x17, 0x13]);
  assert.deepEqual([...readMapColorMath(rom, 19)], [0x00, 0x17, 0x00]);
  assert.equal(readMapProperties(rom, 20).colorMath, 5);
  const anim = readBgAnim(rom, 0);
  assert.equal(anim.animated.length, 8);
  assert.equal(anim.animated[0].speed, 0x100);
  assert.equal(anim.initial.length, 16 * 0x80);
  assert.deepEqual([...anim.initial.subarray(0, 0x80)], [...anim.animated[0].frames[0]]);
  assert.equal(readBg3Anim(rom, 0), null);
  const steam = readBg3Anim(rom, 6);
  assert.ok(steam.data.length > 0 && steam.size > 0 && steam.frames.every(offset => offset < steam.data.length));
  assert.equal(spriteGfxOffset(rom, 0), 0x150000);  // Terra
  assert.equal(buildSpriteGfx(rom, 0).length, 12 * 16 * 24);
  assert.ok(buildSpriteGfx(rom, 0).some(index => index !== 0));
  assert.throws(() => buildSpriteGfx(rom, SPRITE_GFX_COUNT), /Invalid sprite graphics/);
  // Layers that are declared larger than their layout are padded with empty cells.
  const props = readMapProperties(rom, 6);
  props.layers.forEach((layer, index) => {
    assert.equal(loadLayout(rom, props.layouts[index], layer.width, layer.height).length, layer.width * layer.height);
  });
});

test('map files start with the magic, the map number and the raw properties', { skip }, () => {
  const rom = loadRom();
  const map = buildMapData(rom, 28);
  assert.equal(map.subarray(0, 8).toString('ascii'), MAP_MAGIC);
  assert.equal(map.readUInt32LE(8), PACK_VERSION);
  assert.equal(map.readUInt32LE(12), 28);
  assert.deepEqual([...map.subarray(16, 16 + MAP_PROP_SIZE)], [...readMapProperties(rom, 28).raw]);
  assert.deepEqual([...map.subarray(16 + MAP_PROP_SIZE, 18 + MAP_PROP_SIZE)], [8, 45]);  // start x, y
  assert.throws(() => buildMapData(rom, 1), /World maps/);
  const common = buildCommonData(rom);
  assert.equal(common.subarray(0, 8).toString('ascii'), COMMON_MAGIC);
});

test('a pack contains every field map and is never written over an existing one', { skip }, () => {
  const rom = loadRom();
  const directory = mkdtempSync(join(tmpdir(), 'ff6-pack-'));
  try {
    const destination = join(directory, 'pack');
    const result = writePack(rom, destination);
    assert.equal(result.written.length, 412);
    assert.deepEqual(result.skipped, []);
    assert.ok(existsSync(join(destination, 'common.ff6data')));
    assert.equal(readdirSync(join(destination, 'maps')).length, 412);
    assert.throws(() => writePack(rom, destination), /Refusing to overwrite/);
  } finally {
    rmSync(directory, { recursive: true, force: true });
  }
});
