// src/field/entrance.asm: map entrance triggers.
import { readPointedItems } from '../common/bytes.mjs';

const SHORT_ENTRANCE_PTRS = 0x1fbb00;
const SHORT_ENTRANCE_SIZE = 6;
const LONG_ENTRANCE_PTRS = 0x2df480;
const LONG_ENTRANCE_SIZE = 7;

export const MAP_ID_MASK = 0x1ff;

// Raw 6-byte items: x, y, map word, destination x, destination y.
export function readShortEntrances(rom, mapId) {
  return readPointedItems(rom, SHORT_ENTRANCE_PTRS, SHORT_ENTRANCE_SIZE, mapId);
}

// Raw 7-byte items: x, y, length (bit 7 = vertical), map word, destination x, destination y.
export function readLongEntrances(rom, mapId) {
  return readPointedItems(rom, LONG_ENTRANCE_PTRS, LONG_ENTRANCE_SIZE, mapId);
}

export { SHORT_ENTRANCE_SIZE, LONG_ENTRANCE_SIZE };
