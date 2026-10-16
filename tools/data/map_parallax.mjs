// src/data/map_parallax.asm: bg2/bg3 scroll speeds and multipliers.
import { slice } from '../common/bytes.mjs';

const MAP_PARALLAX = 0x00fe40;
export const MAP_PARALLAX_SIZE = 8;

// bg2 h speed, bg2 v speed, bg3 h speed, bg3 v speed (signed), bg2 h mult, bg2 v mult, bg3 h mult, bg3 v mult.
export function readMapParallax(rom, index) {
  return Buffer.from(slice(rom, MAP_PARALLAX + index * MAP_PARALLAX_SIZE, MAP_PARALLAX_SIZE, 'Map parallax'));
}
