// src/field/text.asm: the dialogue font, the window graphics and the dialogue codes the engine reads.
import { decodePalette } from '../graphics.mjs';
import { CONTROLS } from '../text.mjs';

const FONT_WIDTHS = 0x048fc0;
const FONT_GFX = 0x0490c0;
export const FONT_WIDTH_COUNT = 256;
export const GLYPH_COUNT = 96;
export const GLYPH_ROWS = 11;
const WINDOW_GFX = 0x2d0000;
const WINDOW_PAL = 0x2d1c00;
export const WINDOW_TILE_COUNT = 28;
const CHAR_NAMES = 0x0478c0;
const UNKNOWN_GLYPH = 0x5f;  // '?'

const TEXT_TO_GLYPH = new Map([
  ...[...'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789']
    .map((letter, index) => [letter, 0x20 + index]),
  ['!', 0x5e], ['?', 0x5f], ['/', 0x60], [':', 0x61], ['"', 0x62], ['\u201d', 0x62],
  ["'", 0x63], ['\u2019', 0x63], ['-', 0x64], ['.', 0x65], [',', 0x66], ['\u2026', 0x67],
  [';', 0x68], ['#', 0x69], ['+', 0x6a], ['(', 0x6b], [')', 0x6c], ['%', 0x6d],
  ['~', 0x6e], ['*', 0x6f], ['@', 0x70], ['=', 0x72], ['\u201c', 0x73], [' ', 0x7f],
]);

function decode4bppPixel(data, tileOffset, x, y) {
  const shift = 7 - x;
  return ((data[tileOffset + y * 2] >> shift) & 1)
    | (((data[tileOffset + y * 2 + 1] >> shift) & 1) << 1)
    | (((data[tileOffset + 16 + y * 2] >> shift) & 1) << 2)
    | (((data[tileOffset + 17 + y * 2] >> shift) & 1) << 3);
}

export function buildFontWidths(rom) {
  return Buffer.from(rom.subarray(FONT_WIDTHS, FONT_WIDTHS + FONT_WIDTH_COUNT));
}

export function buildFontRows(rom) {
  return Buffer.from(rom.subarray(FONT_GFX, FONT_GFX + GLYPH_COUNT * GLYPH_ROWS * 2));
}

export function buildWindowTiles(rom) {
  const palette = decodePalette(rom.subarray(WINDOW_PAL, WINDOW_PAL + 32));
  const tiles = Buffer.alloc(WINDOW_TILE_COUNT * 64 * 4);
  for (let tile = 0; tile < WINDOW_TILE_COUNT; tile += 1) {
    for (let y = 0; y < 8; y += 1) {
      for (let x = 0; x < 8; x += 1) {
        const index = decode4bppPixel(rom, WINDOW_GFX + tile * 32, x, y);
        tiles.set(index === 0 ? [0, 0, 0, 0] : palette[index], (tile * 64 + y * 8 + x) * 4);
      }
    }
  }
  return tiles;
}

function readCharacterName(rom, characterId) {
  const codes = [];
  for (let i = 0; i < 6; i += 1) {
    const value = rom[CHAR_NAMES + characterId * 6 + i];
    if (value === 0xff) break;
    if (value < 0x80 || value > 0xdf) throw new Error(`Unsupported name byte 0x${value.toString(16)}.`);
    codes.push(value - 0x60);
  }
  return codes;
}

// lenient: characters missing from the font become '?' instead of an error (used for the original text).
export function encodeDialogueForEngine(rom, tokens, { lenient = false } = {}) {
  const codes = [];
  const unsupported = new Set();
  for (const token of tokens) {
    if (typeof token === 'string') {
      for (const character of token) {
        const glyph = TEXT_TO_GLYPH.get(character);
        if (glyph !== undefined) codes.push(glyph);
        else if (lenient) codes.push(UNKNOWN_GLYPH);
        else unsupported.add(character);
      }
    } else if (token.type === 'name') {
      codes.push(...readCharacterName(rom, token.characterId));
    } else if (token.type === 'icon') {
      codes.push(token.code);
    } else if (token.type === 'control' && CONTROLS.has(token.code)) {
      // Spell names need game state that the engine does not have; the item name and the gil amount are filled in by the game.
      if (token.code >= 0x1b) continue;
      codes.push(token.code, ...token.args);
    } else {
      throw new Error(`Unsupported dialogue token: ${JSON.stringify(token)}`);
    }
  }
  if (unsupported.size > 0) {
    throw new Error(`Characters missing from the ROM font: ${[...unsupported].join(' ')}`);
  }
  codes.push(0);
  return Buffer.from(codes);
}
