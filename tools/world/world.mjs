// src/world/*.asm: the world maps (the world of balance and the world of ruin) and what the party needs to walk on them.
import { decompressLzss, read24, slice } from '../common/bytes.mjs';
import { readShortEntrances } from '../field/entrance.mjs';
import { readEventTriggers } from '../event/event_trigger.mjs';

export const WORLD_MAGIC = 'FF6WRLD1';
export const WORLD_COUNT = 2;
export const TILEMAP_BYTES = 0x10000;  // 256 x 256 tiles of 16 x 16 pixels
export const FORMATION_BYTES = 0x400;  // for each tile, its four characters
export const CHR_COUNT = 256;
export const CHR_BYTES = CHR_COUNT * 64;  // mode 7 characters: 8 x 8 pixels of one byte each
export const WORLD_GFX_BYTES = FORMATION_BYTES + CHR_COUNT * 32 + CHR_COUNT / 2;
export const PALETTE_BYTES = 0x100;  // 128 colors
export const TILE_PROP_BYTES = 0x200;
export const MINIMAP_GFX_BYTES = 0x800;
export const MISC_SPRITE_GFX_BYTES = 0x1800;
export const WORLD_BATTLE_GROUP_BYTES = 0x200;
export const WORLD_BATTLE_RATE_BYTES = 0x80;
export const ANIM_PTR_BYTES = 0xd8;  // 108 frames of animated sprites
export const ANIM_DATA_BYTES = 0x13d6;
export const ANIM_GFX_BYTES = 0x800;
export const AIRSHIP_GFX_BYTES = 0x1800;
export const BACKDROP_GFX_BYTES = 0x2000;
export const BACKDROP_TILES_BYTES = 0x1000;

const GFX_PTRS = 0x2eb200;  // EE/B200
const TILEMAP_PTR = [GFX_PTRS + 0x0f, GFX_PTRS + 0x24];
const GFX_PTR = [GFX_PTRS + 0x12, GFX_PTRS + 0x1e];
const MISC_SPRITE_PTR = GFX_PTRS + 0x45;
const BACKDROP_GFX_PTR = GFX_PTRS + 0x06;
const BACKDROP_TILES_PTR = GFX_PTRS + 0x09;
const AIRSHIP_GFX_PTR = [GFX_PTRS + 0x0c, GFX_PTRS + 0x51];
const ANIM_GFX_PTR = GFX_PTRS + 0x42;
const ANIM_PTRS = 0x2e573e;  // EE/573E: where the sprites of each frame are in the data that follows
const ANIM_DATA = 0x2e5816;
const MINIMAP_PTR = [GFX_PTRS + 0x4b, GFX_PTRS + 0x4e];
const MOD_PTRS = 0x2eb260;  // EE/B260: where the modifications of each world start, then where they end
const PALETTES = 0x12ec00;  // D2/EC00: the background palette of each world, then the sprite palette of each
const TILE_PROPS = 0x2e9b14;  // EE/9B14, one table for each world
const WORLD_BATTLE_GROUP = 0x0f5400;  // CF/5400
const WORLD_BATTLE_RATE = 0x0f5800;  // CF/5800

const farPointer = (rom, at) => read24(rom, at) & 0x3fffff;

function decompress(rom, at, size, label) {
  const data = decompressLzss(rom, farPointer(rom, at), 0x20000);
  if (data.length !== size) throw new Error(`${label} has an unexpected size.`);
  return data;
}

// TfrWorldMapGfx: the characters hold two pixels per byte, the low half first, and the palette of each pair of
// characters comes from a table; mode 7 wants a byte for each pixel, the palette in its high half.
export function buildChr(gfx) {
  const chr = Buffer.alloc(CHR_BYTES);
  const pixels = FORMATION_BYTES;
  const palettes = FORMATION_BYTES + CHR_COUNT * 32;
  for (let c = 0; c < CHR_COUNT; c += 1) {
    const palette = (gfx[palettes + (c >> 1)] >> ((c & 1) * 4)) & 0x0f;
    for (let i = 0; i < 32; i += 1) {
      const byte = gfx[pixels + c * 32 + i];
      chr[c * 64 + i * 2] = (palette << 4) | (byte & 0x0f);
      chr[c * 64 + i * 2 + 1] = (palette << 4) | (byte >> 4);
    }
  }
  return chr;
}

// ModifyMap: rectangles of tiles that replace the map once an event bit is set (a castle that comes out of the sand,
// the floating continent that goes away...).
export function readModifications(rom, world) {
  const first = farPointer(rom, MOD_PTRS + world * 3);
  const end = farPointer(rom, MOD_PTRS + world * 3 + 3);
  const base = farPointer(rom, MOD_PTRS);
  const mods = [];
  for (let at = first; at < end; at += 4) {
    const bit = rom.readUInt16LE(at);
    const tiles = base + rom.readUInt16LE(at + 2);
    const x = rom[tiles], y = rom[tiles + 1];
    const width = rom[tiles + 2] >> 4, height = rom[tiles + 2] & 0x0f;
    mods.push({ bit, x, y, width, height, tiles: Buffer.from(slice(rom, tiles + 3, width * height, 'Map modification')) });
  }
  return mods;
}

function writeCounted(parts, items, itemSize) {
  const count = Buffer.alloc(2);
  count.writeUInt16LE(items.length / itemSize);
  parts.push(count, Buffer.from(items));
}

export function buildWorldData(rom, version) {
  const header = Buffer.alloc(12);
  header.write(WORLD_MAGIC, 0, 8, 'ascii');
  header.writeUInt32LE(version, 8);
  const parts = [header];
  for (let world = 0; world < WORLD_COUNT; world += 1) {
    const gfx = decompress(rom, GFX_PTR[world], WORLD_GFX_BYTES, 'World graphics');
    parts.push(decompress(rom, TILEMAP_PTR[world], TILEMAP_BYTES, 'World map'));
    parts.push(gfx.subarray(0, FORMATION_BYTES));
    parts.push(buildChr(gfx));
    parts.push(slice(rom, PALETTES + world * PALETTE_BYTES, PALETTE_BYTES, 'World palette'));
    parts.push(slice(rom, PALETTES + (WORLD_COUNT + world) * PALETTE_BYTES, PALETTE_BYTES, 'World sprite palette'));
    parts.push(slice(rom, TILE_PROPS + world * TILE_PROP_BYTES, TILE_PROP_BYTES, 'World tile properties'));
    parts.push(decompress(rom, MINIMAP_PTR[world], MINIMAP_GFX_BYTES, 'Mini-map'));
    const mods = readModifications(rom, world);
    const count = Buffer.alloc(2);
    count.writeUInt16LE(mods.length);
    parts.push(count);
    for (const mod of mods) {
      const head = Buffer.alloc(6);
      head.writeUInt16LE(mod.bit, 0);
      head.writeUInt8(mod.x, 2);
      head.writeUInt8(mod.y, 3);
      head.writeUInt8(mod.width, 4);
      head.writeUInt8(mod.height, 5);
      parts.push(head, mod.tiles);
    }
    writeCounted(parts, readShortEntrances(rom, world), 6);
    writeCounted(parts, readEventTriggers(rom, world), 5);
  }
  parts.push(decompress(rom, MISC_SPRITE_PTR, MISC_SPRITE_GFX_BYTES, 'World sprites'));
  parts.push(slice(rom, WORLD_BATTLE_GROUP, WORLD_BATTLE_GROUP_BYTES, 'World battle groups'));
  parts.push(slice(rom, WORLD_BATTLE_RATE, WORLD_BATTLE_RATE_BYTES, 'World battle rates'));
  parts.push(slice(rom, ANIM_PTRS, ANIM_PTR_BYTES, 'Animation pointers'));
  parts.push(slice(rom, ANIM_DATA, ANIM_DATA_BYTES, 'Animation frames'));
  parts.push(decompress(rom, ANIM_GFX_PTR, ANIM_GFX_BYTES, 'Animated sprites'));
  parts.push(decompress(rom, AIRSHIP_GFX_PTR[0], AIRSHIP_GFX_BYTES, 'Airship'));
  parts.push(decompress(rom, AIRSHIP_GFX_PTR[1], AIRSHIP_GFX_BYTES, 'Falcon'));
  parts.push(decompress(rom, BACKDROP_GFX_PTR, BACKDROP_GFX_BYTES, 'Backdrop graphics'));
  parts.push(decompress(rom, BACKDROP_TILES_PTR, BACKDROP_TILES_BYTES, 'Backdrop tiles'));
  return Buffer.concat(parts);
}
