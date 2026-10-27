// src/field/player.asm (TreasureProp): the chests of each map, 5 bytes each.
const TREASURE_PTRS = 0x2d82f4;  // ED/82F4, a word for each map and one more for the end
const TREASURE_DATA = 0x2d8634;  // ED/8634
export const TREASURE_PROP_SIZE = 5;  // x, y, a word with the treasure bit and the kind, then the contents
export const TREASURE_MAP_COUNT = 415;

export function readTreasureProps(rom, mapId) {
  if (!Number.isInteger(mapId) || mapId < 0 || mapId >= TREASURE_MAP_COUNT) throw new Error(`Invalid map ${mapId}.`);
  const start = rom.readUInt16LE(TREASURE_PTRS + mapId * 2);
  const end = rom.readUInt16LE(TREASURE_PTRS + mapId * 2 + 2);
  if (end < start || (end - start) % TREASURE_PROP_SIZE !== 0) throw new Error(`Invalid treasure data for map ${mapId}.`);
  return Buffer.from(rom.subarray(TREASURE_DATA + start, TREASURE_DATA + end));
}
