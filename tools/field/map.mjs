// src/field/map.asm (+ map_tileset, map_tile_prop, sub_tilemap): map properties, tilesets, layouts, graphics.
import { decompressLzss, read24, slice } from '../common/bytes.mjs';
import { BG_ANIM_VRAM, readBgAnim } from './anim.mjs';
import { readShortEntrances } from './entrance.mjs';

export const MAP_COUNT = 415;
export const MAP_PROP_SIZE = 33;
export const VRAM_SIZE = 0x10000;
export const MAP_PALETTE_BYTES = 256;
export const TILE_PROPS_BYTES = 0x200;
export const TILESET_WORDS = 0x400;
export const BG3_PALETTE_BYTES = 0x40;
export const BG3_GFX_BYTES = 0x1000;
const MAP_PROP = 0x2d8f00;
const MAP_GFX_PTRS = 0x1fda00;
const MAP_GFX = 0x1fdb00;
const MAP_GFX_BYTES = 0x2000;
const MAP_TILESET_PTRS = 0x1fba00;
const MAP_TILESET = 0x1e0000;
const SUB_TILEMAP_PTRS = 0x19cd90;
const SUB_TILEMAP = 0x19d1b0;
const TILE_PROP_PTRS = 0x19cd10;
const TILE_PROP = 0x19a800;
const MAP_PAL = 0x2dc480;
const MAP_GFX_BG3_PTRS = 0x26cd60;
const MAP_GFX_BG3 = 0x268780;
const BG3_GFX_COUNT = 18;

export function readMapProperties(rom, mapId) {
  if (!Number.isInteger(mapId) || mapId < 0 || mapId >= MAP_COUNT) {
    throw new Error(`Map ID must be between 0 and ${MAP_COUNT - 1}.`);
  }
  const p = slice(rom, MAP_PROP + mapId * MAP_PROP_SIZE, MAP_PROP_SIZE, 'Map properties');
  const word = offset => p[offset] | (p[offset + 1] << 8);
  const size = (byte, shift) => ({
    width: 16 << ((byte >> (shift + 2)) & 3),
    height: 16 << ((byte >> shift) & 3),
  });
  return {
    mapId,
    raw: Buffer.from(p),
    tileProperties: p[4],
    graphics: [p[7] & 0x7f, (word(7) >> 7) & 0x7f, (word(8) >> 6) & 0x7f, (word(9) >> 5) & 0x7f],
    graphicsLayer3: (word(10) >> 4) & 0x3f,
    tilesets: [(word(11) >> 2) & 0x7f, (p[12] >> 1) & 0x7f],
    layouts: [word(13) & 0x3ff, (word(14) >> 2) & 0x3ff, (word(15) >> 4) & 0x3ff],
    layer2Offset: [p[18], p[19]],
    layer3Offset: [p[20], p[21]],
    parallax: p[22],
    layers: [size(p[23], 4), size(p[23], 0), size(p[24], 4)],
    palette: p[25],
    paletteAnimation: p[26],
    bgAnimation: p[27] & 0x1f,
    bg3Animation: p[27] >> 5,
    animation: p[27],
    colorMath: p[32],
    width: p[30],
    height: p[31],
  };
}

// Tile graphics as the game copies them to vram, with frame 1 of the animated tiles at 0x5000.
export function loadMapGraphics(rom, props) {
  const vram = Buffer.alloc(VRAM_SIZE);
  const copyGfx = (index, byteAddress) => {
    const source = MAP_GFX + read24(rom, MAP_GFX_PTRS + index * 3);
    slice(rom, source, MAP_GFX_BYTES, `Map graphics ${index}`).copy(vram, byteAddress);
  };
  const [gfx1, gfx2, gfx3, gfx4] = props.graphics;
  copyGfx(gfx1, 0x0000);
  copyGfx(gfx2, 0x2000);
  copyGfx(gfx3, 0x3000);
  if (gfx4 !== gfx3) copyGfx(gfx4, 0x4000);
  readBgAnim(rom, props.bgAnimation).initial.copy(vram, BG_ANIM_VRAM);
  return vram;
}

// 0x40 bytes of per-tile palette numbers followed by 256 2bpp tiles; null when the index is not a bg3 graphic.
export function loadBg3Graphics(rom, index) {
  if (index >= BG3_GFX_COUNT) return null;
  const data = decompressLzss(rom, MAP_GFX_BG3 + read24(rom, MAP_GFX_BG3_PTRS + index * 3), 0x2000);
  if (data.length < BG3_PALETTE_BYTES + BG3_GFX_BYTES) throw new Error(`Bg3 graphics ${index} are too short.`);
  return { palette: data.subarray(0, BG3_PALETTE_BYTES), graphics: data.subarray(BG3_PALETTE_BYTES, BG3_PALETTE_BYTES + BG3_GFX_BYTES) };
}

export function loadMapPalette(rom, props) {
  return Buffer.from(slice(rom, MAP_PAL + props.palette * MAP_PALETTE_BYTES, MAP_PALETTE_BYTES, 'Map palette'));
}

// Quadrant k (top-left, top-right, bottom-left, bottom-right) of tile t is word k * 256 + t.
export function loadTileset(rom, index) {
  const data = decompressLzss(rom, MAP_TILESET + read24(rom, MAP_TILESET_PTRS + index * 3));
  if (data.length < 0x800) throw new Error(`Tileset ${index} is too short.`);
  return Uint16Array.from({ length: TILESET_WORDS }, (_, i) => data[i] | (data[i + 0x400] << 8));
}

// A few maps declare a layer larger than its layout; the missing cells are left empty.
export function loadLayout(rom, index, width, height) {
  const layout = new Uint8Array(width * height);
  if (index !== 0) {
    const data = decompressLzss(rom, SUB_TILEMAP + read24(rom, SUB_TILEMAP_PTRS + index * 3));
    layout.set(data.subarray(0, width * height));
  }
  return layout;
}

// 256 low bytes, then 256 high bytes (passability bits).
export function loadTileProperties(rom, index) {
  const start = TILE_PROP + rom.readUInt16LE(TILE_PROP_PTRS + index * 2);
  const data = decompressLzss(rom, start);
  if (data.length < TILE_PROPS_BYTES) throw new Error(`Tile properties ${index} are too short.`);
  return Buffer.from(data.subarray(0, TILE_PROPS_BYTES));
}

// Where the party appears when entering the map from another one.
export function findStart(rom, mapId, layout, tileProps, width, height) {
  for (let source = 0; source < MAP_COUNT; source += 1) {
    const entrances = readShortEntrances(rom, source);
    for (let offset = 0; offset + 6 <= entrances.length; offset += 6) {
      if ((entrances.readUInt16LE(offset + 2) & 0x1ff) !== mapId) continue;
      const x = entrances[offset + 4];
      const y = entrances[offset + 5];
      if (x < width && y < height && tileProps[layout[y * width + x]] !== 0xf7) {
        return { x, y, facing: (entrances[offset + 3] >> 4) & 3 };
      }
    }
  }
  return { x: Math.min(8, width - 1), y: Math.min(8, height - 1), facing: 2 };
}
