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

export const MENU_SPRITE_BYTES = 0x800;  // ED/5AC0 to ED/62BF: the hand, the arrow under the characters that must stay, the status icons
const STATUS_ICON_PAL = 0x18e8c0;  // the last of the 8-byte palettes of D8/E800: the sprite colors 12 to 15 of the status icons
export const MENU_ICON_PALETTE_BYTES = 8;

// The sheet of sprites of the menus (4bpp, 16 tiles per row), then the palette of the status icons.
export function readMenuSprites(rom) {
  return Buffer.concat([
    rom.subarray(MENU_SPRITE_GFX, MENU_SPRITE_GFX + MENU_SPRITE_BYTES),
    rom.subarray(STATUS_ICON_PAL, STATUS_ICON_PAL + MENU_ICON_PALETTE_BYTES),
  ]);
}

export const ITEM_COUNT = 256;
export const ITEM_NAME_BYTES = 13;  // the symbol of the kind of item, then 12 letters; 0xFF fills it
const ITEM_NAMES = 0x12b300;  // D2/B300

export function readItemNames(rom) {
  return Buffer.from(rom.subarray(ITEM_NAMES, ITEM_NAMES + ITEM_COUNT * ITEM_NAME_BYTES));
}

const ITEM_DESC = 0x2d6400;  // ED/6400: texts in the codes of the small font, 0x01 for a new line, 0 at the end
const ITEM_DESC_PTRS = 0x2d7aa0;  // ED/7AA0: a word for each item, from the start of the texts
export const ITEM_DESC_BYTES = 0x13a0;

// The names of the abilities in the codes of the small font: a symbol and six letters for a spell, eight letters
// for an esper, ten for a swordtech or a blitz.
export const SPELL_COUNT = 54;
export const SPELL_NAME_BYTES = 7;
export const ESPER_COUNT = 27;
export const ESPER_NAME_BYTES = 8;
export const TECH_COUNT = 8;
export const TECH_NAME_BYTES = 10;
const SPELL_NAMES = 0x26f567;  // E6/F567
const ESPER_NAMES = 0x26f6e1;  // E6/F6E1
const BUSHIDO_NAMES = 0x0f3c40;  // CF/3C40
const BLITZ_NAMES = 0x26f831;  // E6/F831

export function readSkillNames(rom) {
  return Buffer.concat([
    rom.subarray(SPELL_NAMES, SPELL_NAMES + SPELL_COUNT * SPELL_NAME_BYTES),
    rom.subarray(ESPER_NAMES, ESPER_NAMES + ESPER_COUNT * ESPER_NAME_BYTES),
    rom.subarray(BUSHIDO_NAMES, BUSHIDO_NAMES + TECH_COUNT * TECH_NAME_BYTES),
    rom.subarray(BLITZ_NAMES, BLITZ_NAMES + TECH_COUNT * TECH_NAME_BYTES),
  ]);
}

// The pointers of the 256 items, then the texts.
export function readItemDescriptions(rom) {
  return Buffer.concat([
    rom.subarray(ITEM_DESC_PTRS, ITEM_DESC_PTRS + ITEM_COUNT * 2),
    rom.subarray(ITEM_DESC, ITEM_DESC + ITEM_DESC_BYTES),
  ]);
}

export const RARE_ITEM_COUNT = 20;
export const RARE_NAME_BYTES = 13;
export const RARE_DESC_BYTES = 0x260;
const RARE_NAMES = 0x0efba0;  // CE/FBA0
const RARE_DESC_PTRS = 0x0efb60;  // CE/FB60: a word for each, from the start of the texts
const RARE_DESC = 0x0efcb0;  // CE/FCB0: the texts of the small font, like the items'

// The names of the rare items (the key items of the story), then the pointers to their descriptions, then the texts.
export function readRareItems(rom) {
  return Buffer.concat([
    rom.subarray(RARE_NAMES, RARE_NAMES + RARE_ITEM_COUNT * RARE_NAME_BYTES),
    rom.subarray(RARE_DESC_PTRS, RARE_DESC_PTRS + RARE_ITEM_COUNT * 2),
    rom.subarray(RARE_DESC, RARE_DESC + RARE_DESC_BYTES),
  ]);
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
    readPortraits(rom), readWindows(rom), readItemNames(rom), readItemDescriptions(rom), readSkillNames(rom), readMenuSprites(rom), readRareItems(rom)]);
}
