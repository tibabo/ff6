// src/field/obj.asm + sprite_data.asm: map sprite graphics (npcs and party) and their palettes.
import { slice } from '../common/bytes.mjs';

const SPRITE_GFX_PTRS_LO = 0x00d0f2;
const SPRITE_GFX_PTRS_HI = 0x00d23c;
export const SPRITE_GFX_COUNT = 165;
const SPRITE_TILE_OFFSETS = 0x00ce3a;
const TOP_HFLIP = 0x00cd3a;
const BOTTOM_HFLIP = 0x00cdba;
const MAP_SPRITE_PAL = 0x268000;
export const SPRITE_PAL_COUNT = 60;
const SPRITE_PAL_BYTES = 32;
export const FRAME_WIDTH = 16;
export const FRAME_HEIGHT = 24;
// Down 1-3, up 1-3, left 1-3, then right 1-3 (left frames with the flip bit).
export const SPRITE_FRAMES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 0x46, 0x47, 0x48];
const TILES_PER_FRAME = 6;
const TILE_BYTES = 32;

export function spriteGfxOffset(rom, gfx) {
  if (gfx >= SPRITE_GFX_COUNT) throw new Error(`Invalid sprite graphics ${gfx}.`);
  return (((rom[SPRITE_GFX_PTRS_HI + gfx * 2] << 16) | rom.readUInt16LE(SPRITE_GFX_PTRS_LO + gfx * 2))) & 0x3fffff;
}

function decode4bppPixel(data, tileOffset, x, y) {
  const shift = 7 - x;
  return ((data[tileOffset + y * 2] >> shift) & 1)
    | (((data[tileOffset + y * 2 + 1] >> shift) & 1) << 1)
    | (((data[tileOffset + 16 + y * 2] >> shift) & 1) << 2)
    | (((data[tileOffset + 17 + y * 2] >> shift) & 1) << 3);
}

// Palette indices (0 = transparent) of every frame in SPRITE_FRAMES, FRAME_WIDTH x FRAME_HEIGHT each.
export function buildSpriteGfx(rom, gfx) {
  const base = spriteGfxOffset(rom, gfx);
  const out = Buffer.alloc(SPRITE_FRAMES.length * FRAME_WIDTH * FRAME_HEIGHT);
  SPRITE_FRAMES.forEach((frame, frameIndex) => {
    const entry = SPRITE_TILE_OFFSETS + (frame & 0x3f) * TILES_PER_FRAME * 2;
    const flips = [rom[TOP_HFLIP + frame] & 0x40, rom[BOTTOM_HFLIP + frame] & 0x40];
    for (let tile = 0; tile < TILES_PER_FRAME; tile += 1) {
      const source = base + rom.readUInt16LE(entry + tile * 2);
      slice(rom, source, TILE_BYTES, 'Sprite tile');
      const flip = tile < 4 ? flips[0] : flips[1];
      // Each 16-pixel half is mirrored as a whole, so the two columns swap.
      const column = flip ? 1 - (tile & 1) : tile & 1;
      for (let y = 0; y < 8; y += 1) {
        for (let x = 0; x < 8; x += 1) {
          const index = decode4bppPixel(rom, source, flip ? 7 - x : x, y);
          out[frameIndex * FRAME_WIDTH * FRAME_HEIGHT + ((tile >> 1) * 8 + y) * FRAME_WIDTH + column * 8 + x] = index;
        }
      }
    }
  });
  return out;
}

// Raw BGR555 colors, 16 per palette. Palettes 0-7 are the default slots of the sprite palettes in vram.
export function readSpritePalettes(rom) {
  return Buffer.from(slice(rom, MAP_SPRITE_PAL, SPRITE_PAL_COUNT * SPRITE_PAL_BYTES, 'Sprite palettes'));
}
