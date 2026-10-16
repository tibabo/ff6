// src/event/npc_prop.asm: npc properties, 9 bytes each, plus the default npc visibility switches.
import { readPointedItems, slice } from '../common/bytes.mjs';

const NPC_PROP_PTRS = 0x041a10;
export const NPC_PROP_SIZE = 9;
const INIT_NPC_SWITCH = 0x00e0a0;
export const NPC_SWITCH_BYTES = 128;

export function readNpcProps(rom, mapId) {
  return readPointedItems(rom, NPC_PROP_PTRS, NPC_PROP_SIZE, mapId);
}

// One bit per npc switch (event bit 0x300 + n): set when the npc is visible in a new game.
export function readNpcSwitchDefaults(rom) {
  return Buffer.from(slice(rom, INIT_NPC_SWITCH, NPC_SWITCH_BYTES, 'Initial npc switches'));
}

export function decodeNpcProp(bytes) {
  const word = bytes.readUInt16LE(2);
  return {
    eventOffset: bytes[0] | (bytes[1] << 8) | ((bytes[2] & 3) << 16),
    palette: (bytes[2] >> 2) & 7,
    scrollBg2: (bytes[2] & 0x20) !== 0,
    switchId: word >> 6,
    x: bytes[4] & 0x7f,
    special: (bytes[4] & 0x80) !== 0,
    y: bytes[5] & 0x3f,
    speed: bytes[5] >> 6,
    gfx: bytes[6],
    movement: bytes[7] & 0x0f,
    direction: bytes[8] & 3,
    noTurn: (bytes[8] & 4) !== 0,
    animFrame: bytes[8] >> 5,
  };
}
