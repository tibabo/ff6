import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { crc32, inflateSync } from 'node:zlib';
import { requireSupportedRom, sha256 } from './rom.mjs';
import { decode4bpp, decodePalette, decodePortrait, encodePng, extractGraphics } from './graphics.mjs';

const EXPECTED_PORTRAIT_ROWS = [
  [0, 1, 2, 3, 8],
  [16, 17, 18, 19, 9],
  [4, 5, 6, 7, 10],
  [20, 21, 22, 23, 11],
  [13, 14, 15, 24, 12],
];

function unpackPng(png) {
  assert.deepEqual([...png.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);
  const chunks = new Map();
  let offset = 8;
  while (offset < png.length) {
    const length = png.readUInt32BE(offset);
    const type = png.toString('ascii', offset + 4, offset + 8);
    const data = png.subarray(offset + 8, offset + 8 + length);
    assert.equal(crc32(png.subarray(offset + 4, offset + 8 + length)),
      png.readUInt32BE(offset + 8 + length));
    chunks.set(type, data);
    offset += length + 12;
  }
  assert.equal(offset, png.length);
  assert.deepEqual([...chunks.keys()], ['IHDR', 'IDAT', 'IEND']);
  const width = chunks.get('IHDR').readUInt32BE(0);
  const height = chunks.get('IHDR').readUInt32BE(4);
  assert.deepEqual([...chunks.get('IHDR').subarray(8)], [8, 6, 0, 0, 0]);
  const scanlines = inflateSync(chunks.get('IDAT'));
  assert.equal(scanlines.length, height * (width * 4 + 1));
  const rows = [];
  for (let row = 0; row < height; row += 1) {
    assert.equal(scanlines[row * (width * 4 + 1)], 0);
    rows.push(scanlines.subarray(row * (width * 4 + 1) + 1, (row + 1) * (width * 4 + 1)));
  }
  return { width, height, rgba: Buffer.concat(rows) };
}

test('BGR555 colors and palette-zero transparency decode correctly', () => {
  const encoded = Buffer.alloc(32);
  encoded.writeUInt16LE(0x001f, 2);
  encoded.writeUInt16LE(0x03e0, 4);
  encoded.writeUInt16LE(0x7c00, 6);
  const palette = decodePalette(encoded);
  assert.deepEqual(palette[0], [0, 0, 0, 0]);
  assert.deepEqual(palette[1], [255, 0, 0, 255]);
  assert.deepEqual(palette[2], [0, 255, 0, 255]);
  assert.deepEqual(palette[3], [0, 0, 255, 255]);
  assert.throws(() => decodePalette(Buffer.alloc(31)), /exactly 32 bytes/);
});

test('all four SNES bitplanes, horizontal ordering and atlas padding are preserved', () => {
  const encoded = Buffer.alloc(32);
  encoded[0] = 0x80;
  encoded[1] = 0x40;
  encoded[16] = 0x20;
  encoded[17] = 0x10;
  encoded[2] = 0x01;
  const palette = Array.from({ length: 16 }, (_, index) => [index, 0, 0, 255]);
  const image = decode4bpp(encoded, palette, 2);
  assert.equal(image.width, 16);
  assert.equal(image.height, 8);
  assert.deepEqual([0, 1, 2, 3].map(column => image.rgba[column * 4]), [1, 2, 4, 8]);
  assert.equal(image.rgba[(16 + 7) * 4], 1);
  assert.deepEqual([...image.rgba.subarray(8 * 4, 9 * 4)], [0, 0, 0, 0]);
  assert.throws(() => decode4bpp(Buffer.alloc(31), palette, 1), /Invalid 4bpp/);
  assert.throws(() => decode4bpp(encoded, palette, 0), /Invalid 4bpp/);
});

test('portraits place all 25 source tiles in the documented display order', () => {
  const encoded = Buffer.alloc(25 * 32);
  for (let tile = 0; tile < 25; tile += 1) encoded[tile * 32] = tile;
  const before = Buffer.from(encoded);
  const palette = Array.from({ length: 16 }, (_, index) => [index, 0, 0, 255]);
  const image = decodePortrait(encoded, palette);
  assert.equal(image.width, 40);
  assert.equal(image.height, 40);
  assert.equal(image.tileCount, 25);
  for (let tileRow = 0; tileRow < 5; tileRow += 1) {
    const actualRow = [];
    for (let tileColumn = 0; tileColumn < 5; tileColumn += 1) {
      let sourceTile = 0;
      for (let pixel = 0; pixel < 8; pixel += 1) {
        const offset = (tileRow * 8 * image.width + tileColumn * 8 + pixel) * 4;
        sourceTile = (sourceTile << 1) | image.rgba[offset];
      }
      actualRow.push(sourceTile);
    }
    assert.deepEqual(actualRow, EXPECTED_PORTRAIT_ROWS[tileRow]);
  }
  assert.deepEqual(encoded, before);
  assert.throws(() => decodePortrait(Buffer.alloc(799), palette), /exactly 25/);
  assert.throws(() => decodePortrait(Buffer.alloc(832), palette), /exactly 25/);
});

test('PNG chunks, checksums and scanlines preserve an RGBA image', () => {
  const image = { width: 2, height: 1, rgba: Buffer.from([255, 0, 0, 255, 0, 255, 0, 0]) };
  assert.deepEqual(unpackPng(encodePng(image)), image);
  assert.throws(() => encodePng({ ...image, width: 0 }), /Invalid RGBA/);
});

test('the real ROM produces 19 portraits and 14 character tile atlases', {
  skip: !process.env.FF6_ROM,
}, () => {
  const { bytes } = requireSupportedRom(readFileSync(process.env.FF6_ROM));
  const { assets, files } = extractGraphics(bytes);
  assert.equal(assets.filter(asset => asset.kind === 'portraits').length, 19);
  assert.equal(assets.filter(asset => asset.kind === 'sprite-tiles').length, 14);
  assert.equal(files.size, 99);
  for (const asset of assets) {
    const source = files.get(asset.source.graphicsFile);
    assert.equal(source.length, asset.source.graphicsBytes);
    assert.equal(sha256(source), asset.source.graphicsSha256);
    const original = decode4bpp(source, asset.palette, asset.tileColumns);
    const decoded = unpackPng(files.get(asset.image));
    assert.equal(decoded.width, asset.kind === 'portraits' ? 40 : 128);
    assert.equal(decoded.height, asset.kind === 'portraits' ? 40 : 96);
    if (asset.kind === 'portraits') {
      assert.equal(asset.layout, 'ff6-portrait-5x5');
      assert.deepEqual(asset.sourceTileOrder, EXPECTED_PORTRAIT_ROWS.flat());
      for (const [destinationTile, sourceTile] of EXPECTED_PORTRAIT_ROWS.flat().entries()) {
        for (let row = 0; row < 8; row += 1) {
          const sourceOffset = ((Math.floor(sourceTile / 5) * 8 + row) * 40 + (sourceTile % 5) * 8) * 4;
          const destinationOffset = ((Math.floor(destinationTile / 5) * 8 + row) * 40
            + (destinationTile % 5) * 8) * 4;
          assert.deepEqual(decoded.rgba.subarray(destinationOffset, destinationOffset + 32),
            original.rgba.subarray(sourceOffset, sourceOffset + 32));
        }
      }
    } else {
      assert.equal(asset.layout, 'row-major-8x8-tiles');
      assert.deepEqual(decoded.rgba, original.rgba);
    }
    assert.ok(decoded.rgba.some(channel => channel !== 0));
  }
});