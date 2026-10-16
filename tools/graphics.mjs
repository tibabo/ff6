import { crc32, deflateSync } from 'node:zlib';
import { ROM_SIZE, hex, sha256 } from './rom.mjs';

const CHARACTERS = [
  'terra', 'locke', 'cyan', 'shadow', 'edgar', 'sabin', 'celes',
  'strago', 'relm', 'setzer', 'mog', 'gau', 'gogo', 'umaro',
];
const PORTRAITS = [...CHARACTERS, 'soldier', 'imp', 'leo', 'banon', 'ghost'];
const REFERENCE_PALETTES = [2, 1, 4, 4, 0, 0, 0, 3, 3, 4, 5, 3, 3, 5];
const PORTRAIT_TILE_ORDER = Object.freeze([
  0, 1, 2, 3, 8,
  16, 17, 18, 19, 9,
  4, 5, 6, 7, 10,
  20, 21, 22, 23, 11,
  13, 14, 15, 24, 12,
]);

export function decodePalette(encoded) {
  if (!Buffer.isBuffer(encoded) || encoded.length !== 32) {
    throw new Error('A 16-color BGR555 palette must contain exactly 32 bytes.');
  }
  const expand = value => (value << 3) | (value >>> 2);
  return Array.from({ length: 16 }, (_, index) => {
    const color = encoded.readUInt16LE(index * 2);
    return [
      expand(color & 31), expand((color >>> 5) & 31),
      expand((color >>> 10) & 31), index === 0 ? 0 : 255,
    ];
  });
}

export function decode4bpp(encoded, palette, columns) {
  if (!Buffer.isBuffer(encoded) || encoded.length === 0 || encoded.length % 32 !== 0
      || !Number.isInteger(columns) || columns < 1 || columns > 512) {
    throw new Error('Invalid 4bpp tile data or column count.');
  }
  if (!Array.isArray(palette) || palette.length !== 16
      || palette.some(color => !Array.isArray(color) || color.length !== 4
        || color.some(channel => !Number.isInteger(channel) || channel < 0 || channel > 255))) {
    throw new Error('Expected 16 RGBA colors.');
  }
  const tileCount = encoded.length / 32;
  const width = columns * 8;
  const height = Math.ceil(tileCount / columns) * 8;
  if (height > 4096) throw new Error('Tile atlas exceeds the supported height.');
  const rgba = Buffer.alloc(width * height * 4);
  for (let tile = 0; tile < tileCount; tile += 1) {
    const tileOffset = tile * 32;
    for (let row = 0; row < 8; row += 1) {
      for (let column = 0; column < 8; column += 1) {
        const shift = 7 - column;
        const colorIndex = ((encoded[tileOffset + row * 2] >>> shift) & 1)
          | (((encoded[tileOffset + row * 2 + 1] >>> shift) & 1) << 1)
          | (((encoded[tileOffset + 16 + row * 2] >>> shift) & 1) << 2)
          | (((encoded[tileOffset + 17 + row * 2] >>> shift) & 1) << 3);
        const pixelX = (tile % columns) * 8 + column;
        const pixelY = Math.floor(tile / columns) * 8 + row;
        rgba.set(palette[colorIndex], (pixelY * width + pixelX) * 4);
      }
    }
  }
  return { width, height, tileCount, rgba };
}

export function decodePortrait(encoded, palette) {
  if (!Buffer.isBuffer(encoded) || encoded.length !== 0x320) {
    throw new Error('A portrait must contain exactly 25 SNES 4bpp tiles (800 bytes).');
  }
  const ordered = Buffer.concat(PORTRAIT_TILE_ORDER.map(tile => (
    encoded.subarray(tile * 32, (tile + 1) * 32)
  )));
  return decode4bpp(ordered, palette, 5);
}

function pngChunk(type, data) {
  const chunk = Buffer.alloc(data.length + 12);
  chunk.writeUInt32BE(data.length, 0);
  chunk.write(type, 4, 4, 'ascii');
  data.copy(chunk, 8);
  chunk.writeUInt32BE(crc32(chunk.subarray(4, -4)), chunk.length - 4);
  return chunk;
}

export function encodePng({ width, height, rgba }) {
  if (!Number.isInteger(width) || !Number.isInteger(height)
      || width < 1 || height < 1 || width > 4096 || height > 4096
      || !Buffer.isBuffer(rgba) || rgba.length !== width * height * 4) {
    throw new Error('Invalid RGBA image dimensions or buffer.');
  }
  const header = Buffer.alloc(13);
  header.writeUInt32BE(width, 0);
  header.writeUInt32BE(height, 4);
  header[8] = 8;
  header[9] = 6;
  const scanlines = Buffer.alloc((width * 4 + 1) * height);
  for (let row = 0; row < height; row += 1) {
    rgba.copy(scanlines, row * (width * 4 + 1) + 1, row * width * 4, (row + 1) * width * 4);
  }
  return Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    pngChunk('IHDR', header),
    pngChunk('IDAT', deflateSync(scanlines)),
    pngChunk('IEND', Buffer.alloc(0)),
  ]);
}

export function extractGraphics(rom) {
  if (rom.length !== ROM_SIZE) throw new Error('Expected a normalized 3 MiB ROM.');
  const assets = [];
  const files = new Map();
  const addAsset = (kind, id, name, graphicsOffset, byteCount, paletteOffset, columns) => {
    const encoded = rom.subarray(graphicsOffset, graphicsOffset + byteCount);
    const encodedPalette = rom.subarray(paletteOffset, paletteOffset + 32);
    const palette = decodePalette(encodedPalette);
    const image = kind === 'portraits'
      ? decodePortrait(encoded, palette) : decode4bpp(encoded, palette, columns);
    const stem = `${kind}/${String(id).padStart(2, '0')}-${name}`;
    const pngPath = `graphics/${stem}.png`;
    const tilesPath = `originals/${stem}.4bpp`;
    const palettePath = `originals/${stem}.pal`;
    files.set(pngPath, encodePng(image));
    files.set(tilesPath, encoded);
    files.set(palettePath, encodedPalette);
    assets.push({
      id: `${kind}/${id}`,
      name,
      kind,
      image: pngPath,
      width: image.width,
      height: image.height,
      layout: kind === 'portraits' ? 'ff6-portrait-5x5' : 'row-major-8x8-tiles',
      ...(kind === 'portraits' ? { sourceTileOrder: [...PORTRAIT_TILE_ORDER] } : {}),
      tileColumns: columns,
      tileCount: image.tileCount,
      palette,
      source: {
        graphicsOffset: `0x${hex(graphicsOffset)}`,
        graphicsBytes: byteCount,
        graphicsSha256: sha256(encoded),
        graphicsFile: tilesPath,
        paletteOffset: `0x${hex(paletteOffset)}`,
        paletteSha256: sha256(encodedPalette),
        paletteFile: palettePath,
      },
    });
  };
  PORTRAITS.forEach((name, id) => {
    addAsset('portraits', id, name, 0x2d1d00 + id * 0x320, 0x320, 0x2d5860 + id * 32, 5);
  });
  CHARACTERS.forEach((name, id) => {
    addAsset('sprite-tiles', id, name, 0x150000 + id * 0x16a0, 0x16a0,
      0x268000 + REFERENCE_PALETTES[id] * 32, 16);
  });
  return { assets, files };
}