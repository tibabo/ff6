// src/menu/menu_gfx.asm and menu_anim.asm: the font, the palettes and the hand cursor of the menus.
export const MENU_MAGIC = 'FF6MENU1';

const SMALL_FONT = 0x047fc0;  // C4/7FC0, 2bpp
export const MENU_FONT_TILES = 256;
export const MENU_FONT_TILE_BYTES = 16;
const FONT_PAL = 0x18e800;  // D8/E800
export const MENU_FONT_COLORS = 80;  // the five palettes that LoadFontPal copies
const CURSOR_PAL = 0x18e8b8;  // GrayscalePal + 0x18: four colors, which the game puts at the sprite colors 12 to 15
const MENU_SPRITE_GFX = 0x2d5ac0;  // ED/5AC0, 4bpp, 16 tiles per row
export const CURSOR_TILE_BYTES = 32;
const CURSOR_TILES = [0, 1, 16, 17];  // the 16x16 hand: upper left, upper right, lower left, lower right

export function readMenuFont(rom) {
  return Buffer.from(rom.subarray(SMALL_FONT, SMALL_FONT + MENU_FONT_TILES * MENU_FONT_TILE_BYTES));
}

// BGR555 words, as the console keeps them.
export function readMenuPalette(rom) {
  return Buffer.from(rom.subarray(FONT_PAL, FONT_PAL + MENU_FONT_COLORS * 2));
}

export function readCursorTiles(rom) {
  return Buffer.concat(CURSOR_TILES.map(tile => (
    rom.subarray(MENU_SPRITE_GFX + tile * CURSOR_TILE_BYTES, MENU_SPRITE_GFX + (tile + 1) * CURSOR_TILE_BYTES)
  )));
}

export function readCursorPalette(rom) {
  return Buffer.from(rom.subarray(CURSOR_PAL, CURSOR_PAL + 8));
}

export const ITEM_COUNT = 256;
export const ITEM_NAME_BYTES = 13;  // the symbol of the kind of item, then 12 letters; 0xFF fills it
const ITEM_NAMES = 0x12b300;  // D2/B300

export function readItemNames(rom) {
  return Buffer.from(rom.subarray(ITEM_NAMES, ITEM_NAMES + ITEM_COUNT * ITEM_NAME_BYTES));
}

const WINDOW_GFX = 0x2d0000;
const WINDOW_PAL = 0x2d1c00;
export const WINDOW_COUNT = 8;
export const WINDOW_GFX_BYTES = 0x380;  // 28 tiles of 4bpp
export const WINDOW_COLORS = 7;  // colors 1 to 7 of the 16 of each palette, which the player can change

// The eight window patterns as they are in the ROM, then the colors that each one starts with.
export function readWindows(rom) {
  const colors = [];
  for (let window = 0; window < WINDOW_COUNT; window += 1) {
    const start = WINDOW_PAL + window * 32 + 2;
    colors.push(rom.subarray(start, start + WINDOW_COLORS * 2));
  }
  return Buffer.concat([rom.subarray(WINDOW_GFX, WINDOW_GFX + WINDOW_COUNT * WINDOW_GFX_BYTES), ...colors]);
}

const PORTRAIT_GFX = 0x2d1d00;
const PORTRAIT_PAL = 0x2d5860;
export const PORTRAIT_COUNT = 19;
export const PORTRAIT_BYTES = 0x320;  // 25 tiles of 4bpp

// The portraits as they are in the ROM, then their palettes of 16 BGR555 colors.
export function readPortraits(rom) {
  return Buffer.concat([
    rom.subarray(PORTRAIT_GFX, PORTRAIT_GFX + PORTRAIT_COUNT * PORTRAIT_BYTES),
    rom.subarray(PORTRAIT_PAL, PORTRAIT_PAL + PORTRAIT_COUNT * 32),
  ]);
}

// `version` is the version of the whole pack.
export function buildMenuData(rom, version) {
  const header = Buffer.alloc(12);
  header.write(MENU_MAGIC, 0, 8, 'ascii');
  header.writeUInt32LE(version, 8);
  return Buffer.concat([header, readMenuFont(rom), readMenuPalette(rom), readCursorTiles(rom), readCursorPalette(rom),
    readPortraits(rom), readWindows(rom), readItemNames(rom)]);
}
