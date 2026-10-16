// src/field/scroll.asm: the color math table that InitColorMath reads (bg2/bg3 add/sub mode).
import { slice } from '../common/bytes.mjs';

const MAP_COLOR_MATH = 0x00fe00;
export const MAP_COLOR_MATH_SIZE = 3;
export const MAP_COLOR_MATH_COUNT = 19;  // the rest of the table is unused padding

// color math designation ($2131), main screen layers ($212c), sub screen layers ($212d).
// An index outside the table means no color math.
export function readMapColorMath(rom, index) {
  const entry = index < MAP_COLOR_MATH_COUNT ? index : 0;
  return Buffer.from(slice(rom, MAP_COLOR_MATH + entry * MAP_COLOR_MATH_SIZE, MAP_COLOR_MATH_SIZE, 'Map color math'));
}
