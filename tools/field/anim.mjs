// src/field/anim.asm: bg1/bg2 tile animation and bg3 graphics animation data.
import { decompressLzss, read24, slice } from '../common/bytes.mjs';

const BG_ANIM_PROP_PTRS = 0x0091d5;
const BG_ANIM_PROP = 0x0091ff;
const BG_ANIM_ENTRY_SIZE = 10;
const BG_ANIM_TILES = 8;  // only the first 8 entries animate
const BG_ANIM_INITIAL_TILES = 16;
const BG_ANIM_FRAMES = 4;
const BG_ANIM_TILE_BYTES = 0x80;
const MAP_ANIM_GFX = 0x260000;
const BG3_ANIM_PROP_PTRS = 0x00979f;
const BG3_ANIM_PROP = 0x0097ad;
const BG3_ANIM_COUNT = 6;
const MAP_ANIM_GFX_BG3_PTRS = 0x26cda0;
const MAP_ANIM_GFX_BG3 = 0x26cdc0;

export const BG_ANIM_VRAM = 0x5000;

// Frame 1 of the first 16 entries is what vram holds before the animation runs; entries 8-15 read on into
// the next animation's properties, exactly like the game does.
export function readBgAnim(rom, index) {
  const base = BG_ANIM_PROP + rom.readUInt16LE(BG_ANIM_PROP_PTRS + index * 2);
  const initial = Buffer.alloc(BG_ANIM_INITIAL_TILES * BG_ANIM_TILE_BYTES);
  const animated = [];
  for (let tile = 0; tile < BG_ANIM_INITIAL_TILES; tile += 1) {
    const entry = base + tile * BG_ANIM_ENTRY_SIZE;
    const frames = Array.from({ length: BG_ANIM_FRAMES }, (_, frame) => rom.readUInt16LE(entry + 2 + frame * 2));
    slice(rom, MAP_ANIM_GFX + frames[0], BG_ANIM_TILE_BYTES, 'Animated tile').copy(initial, tile * BG_ANIM_TILE_BYTES);
    if (tile < BG_ANIM_TILES) {
      animated.push({
        speed: rom.readUInt16LE(entry),
        frames: frames.map(offset => Buffer.from(slice(rom, MAP_ANIM_GFX + offset, BG_ANIM_TILE_BYTES, 'Animated tile'))),
      });
    }
  }
  return { initial, animated };
}

// index 1-6 (bits 5-7 of the map animation byte); returns null when the map has no bg3 animation.
export function readBg3Anim(rom, index) {
  if (index === 0) return null;
  if (index > BG3_ANIM_COUNT) throw new Error(`Invalid bg3 animation ${index}.`);
  const entry = BG3_ANIM_PROP + rom.readUInt16LE(BG3_ANIM_PROP_PTRS + (index - 1) * 2);
  const frames = Array.from({ length: 8 }, (_, frame) => rom.readUInt16LE(entry + 4 + frame * 2));
  const data = decompressLzss(rom, MAP_ANIM_GFX_BG3 + read24(rom, MAP_ANIM_GFX_BG3_PTRS + (index - 1) * 3), 0x4000);
  return { speed: rom.readUInt16LE(entry), size: rom.readUInt16LE(entry + 2), frames, data };
}
