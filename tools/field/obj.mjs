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
// The frames of the sheet, by the number that the game gives them (0x40 mirrors a frame). First the ones that the engine draws by
// index: down 1-3, up 1-3, left 1-3, right 1-3 (the left ones mirrored), riding left 1-2 and riding right 1-2; then the ones that
// scripts ask for. It is the list of kFrameIds in engine/src/field/sprite_data.cpp.
export const SPRITE_FRAMES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 0x46, 0x47, 0x48, 0x2e, 0x2f, 0x6e, 0x6f,
  0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x55, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x62, 0x63, 0x64, 0x65, 0x68, 0x6a, 0x72];
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

const VEHICLE_GFX = 0x183000;  // D8/3000, 4bpp: the Magitek armor, the chocobo and the raft
export const VEHICLE_GFX_BYTES = 0x2000;  // then the armor of the battles at 0x1500, 0x1800, 0x1900 and 0x1c00

export function readVehicleGfx(rom) {
  return Buffer.from(slice(rom, VEHICLE_GFX, VEHICLE_GFX_BYTES, 'Vehicle graphics'));
}

// InitSpecialNPCGfx copies up to 4 frames of 32x32 pixels (4bpp, 0x200 bytes each) from the graphics of the npc: the first
// SPECIAL_GFX_BYTES of every one, as the game keeps them, for the npcs that have graphics of their own.
export const SPECIAL_GFX_BYTES = 0x800;

export function buildSpecialGfx(rom) {
  const out = Buffer.alloc(SPRITE_GFX_COUNT * SPECIAL_GFX_BYTES);
  for (let gfx = 0; gfx < SPRITE_GFX_COUNT; gfx += 1) {
    const base = spriteGfxOffset(rom, gfx);
    rom.copy(out, gfx * SPECIAL_GFX_BYTES, base, Math.min(base + SPECIAL_GFX_BYTES, rom.length));
  }
  return out;
}
