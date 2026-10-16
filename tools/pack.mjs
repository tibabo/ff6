// Game data pack: common.ff6data (font, sprites, dialogue, event script) and maps/NNN.ff6map.
// Everything is raw game data in the layouts documented in the disassembly; the engine decodes it.
import { existsSync, mkdirSync, readFileSync, renameSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { ROM_SIZE } from './rom.mjs';
import { extractDialogues, validateDialogues } from './text.mjs';
import { readBg3Anim, readBgAnim } from './field/anim.mjs';
import { readMapColorMath } from './field/scroll.mjs';
import { readLongEntrances, readShortEntrances } from './field/entrance.mjs';
import {
  MAP_COUNT, findStart, loadBg3Graphics, loadLayout, loadMapGraphics, loadMapPalette, loadTileProperties,
  loadTileset, readMapProperties, BG3_GFX_BYTES, BG3_PALETTE_BYTES,
} from './field/map.mjs';
import {
  SPRITE_GFX_COUNT, SPRITE_PAL_COUNT, buildSpriteGfx, readSpritePalettes,
} from './field/obj.mjs';
import {
  buildFontRows, buildFontWidths, buildWindowTiles, encodeDialogueForEngine,
} from './field/text.mjs';
import { readNpcProps, readNpcSwitchDefaults } from './event/npc_prop.mjs';
import { readEventScript, readEventTriggers } from './event/event_trigger.mjs';
import { readMapParallax } from './data/map_parallax.mjs';

export const COMMON_MAGIC = 'FF6COMMN';
export const MAP_MAGIC = 'FF6MAP01';
export const PACK_VERSION = 2;
export const FIRST_FIELD_MAP = 3;  // maps 0-2 are the world maps

class Writer {
  constructor() {
    this.parts = [];
  }

  magic(text) {
    this.parts.push(Buffer.from(text, 'ascii'));
  }

  u32(value) {
    const part = Buffer.alloc(4);
    part.writeUInt32LE(value);
    this.parts.push(part);
  }

  u16(value) {
    const part = Buffer.alloc(2);
    part.writeUInt16LE(value);
    this.parts.push(part);
  }

  u8s(...values) {
    this.parts.push(Buffer.from(values));
  }

  bytes(buffer) {
    this.parts.push(Buffer.from(buffer.buffer ? Buffer.from(buffer.buffer, buffer.byteOffset, buffer.byteLength) : buffer));
  }

  words(array) {
    const part = Buffer.alloc(array.length * 2);
    array.forEach((value, i) => part.writeUInt16LE(value, i * 2));
    this.parts.push(part);
  }

  build() {
    return Buffer.concat(this.parts);
  }
}

export function buildCommonData(rom, { textFile } = {}) {
  if (rom.length !== ROM_SIZE) throw new Error('Expected a normalized 3 MiB ROM.');
  const dialogues = textFile ? JSON.parse(readFileSync(textFile, 'utf8')) : extractDialogues(rom);
  validateDialogues(dialogues);
  const encoded = dialogues.entries.map(entry => encodeDialogueForEngine(rom, entry.tokens, { lenient: !textFile }));
  const out = new Writer();
  out.magic(COMMON_MAGIC);
  out.u32(PACK_VERSION);
  out.bytes(buildFontWidths(rom));
  out.bytes(buildFontRows(rom));
  out.bytes(buildWindowTiles(rom));
  out.u32(SPRITE_PAL_COUNT);
  out.bytes(readSpritePalettes(rom));
  out.u32(SPRITE_GFX_COUNT);
  for (let gfx = 0; gfx < SPRITE_GFX_COUNT; gfx += 1) out.bytes(buildSpriteGfx(rom, gfx));
  out.u32(encoded.length);
  let offset = 0;
  for (const codes of encoded) {
    out.u32(offset);
    offset += codes.length;
  }
  out.u32(offset);
  for (const codes of encoded) out.bytes(codes);
  out.bytes(readNpcSwitchDefaults(rom));
  const script = readEventScript(rom);
  out.u32(script.length);
  out.bytes(script);
  return out.build();
}

function writeCounted(out, items, itemSize) {
  if (items.length % itemSize !== 0) throw new Error('Misaligned item table.');
  out.u32(items.length / itemSize);
  out.bytes(items);
}

export function buildMapData(rom, mapId) {
  if (mapId < FIRST_FIELD_MAP) throw new Error('World maps are not supported.');
  const props = readMapProperties(rom, mapId);
  const [layer1, layer2, layer3] = props.layers;
  const layouts = [
    loadLayout(rom, props.layouts[0], layer1.width, layer1.height),
    loadLayout(rom, props.layouts[1], layer2.width, layer2.height),
    loadLayout(rom, props.layouts[2], layer3.width, layer3.height),
  ];
  const tileProps = loadTileProperties(rom, props.tileProperties);
  const vram = loadMapGraphics(rom, props);
  const bg3 = loadBg3Graphics(rom, props.graphicsLayer3);
  const bgAnim = readBgAnim(rom, props.bgAnimation);
  const bg3Anim = readBg3Anim(rom, props.bg3Animation);
  const start = findStart(rom, mapId, layouts[0], tileProps, layer1.width, layer1.height);

  const out = new Writer();
  out.magic(MAP_MAGIC);
  out.u32(PACK_VERSION);
  out.u32(mapId);
  out.bytes(props.raw);
  out.u8s(start.x, start.y, start.facing);
  out.bytes(tileProps);
  for (const layout of layouts) out.bytes(layout);
  out.words(loadTileset(rom, props.tilesets[0]));
  out.words(loadTileset(rom, props.tilesets[1]));
  out.bytes(vram);
  out.bytes(bg3 ? bg3.palette : Buffer.alloc(BG3_PALETTE_BYTES));
  out.bytes(bg3 ? bg3.graphics : Buffer.alloc(BG3_GFX_BYTES));
  out.bytes(loadMapPalette(rom, props));
  for (const tile of bgAnim.animated) {
    out.u16(tile.speed);
    for (const frame of tile.frames) out.bytes(frame);
  }
  if (bg3Anim) {
    out.u16(bg3Anim.speed);
    out.u16(bg3Anim.size);
    out.words(bg3Anim.frames);
    out.u32(bg3Anim.data.length);
    out.bytes(bg3Anim.data);
  } else {
    out.u16(0);
    out.u16(0);
    out.words(new Array(8).fill(0));
    out.u32(0);
  }
  out.bytes(readMapParallax(rom, props.parallax));
  out.bytes(readMapColorMath(rom, props.colorMath));
  writeCounted(out, readNpcProps(rom, mapId), 9);
  writeCounted(out, readEventTriggers(rom, mapId), 5);
  writeCounted(out, readShortEntrances(rom, mapId), 6);
  writeCounted(out, readLongEntrances(rom, mapId), 7);
  return out.build();
}

// Writes the pack to a new directory; maps that cannot be decoded are reported, not fatal.
export function writePack(rom, outDir, options = {}) {
  if (existsSync(outDir)) throw new Error(`Refusing to overwrite existing output: ${outDir}`);
  const temp = `${outDir}.partial-${process.pid}`;
  mkdirSync(join(temp, 'maps'), { recursive: true });
  try {
    writeFileSync(join(temp, 'common.ff6data'), buildCommonData(rom, options), { flag: 'wx' });
    const written = [];
    const skipped = [];
    for (let mapId = FIRST_FIELD_MAP; mapId < MAP_COUNT; mapId += 1) {
      try {
        writeFileSync(join(temp, 'maps', `${String(mapId).padStart(3, '0')}.ff6map`), buildMapData(rom, mapId), { flag: 'wx' });
        written.push(mapId);
      } catch (error) {
        skipped.push({ map: mapId, reason: error.message });
      }
    }
    mkdirSync(dirname(outDir), { recursive: true });
    renameSync(temp, outDir);
    return { written, skipped };
  } catch (error) {
    rmSync(temp, { recursive: true, force: true });
    throw error;
  }
}
