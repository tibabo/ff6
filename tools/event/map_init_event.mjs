// src/event/map_init_event.asm: the event that a map runs when it is entered through an entrance.
import { read24 } from '../common/bytes.mjs';

const MAP_INIT_EVENT = 0x11fa00;  // D1/FA00, 3 bytes per map
export const MAP_INIT_EVENT_COUNT = 0x200;
const EVENT_SCRIPT_SIZE = 0x40000;

// Offset into the event script. Maps without a startup event point at a return command.
export function readMapInitEvent(rom, mapId) {
  if (!Number.isInteger(mapId) || mapId < 0 || mapId >= MAP_INIT_EVENT_COUNT) throw new Error(`Invalid map ${mapId}.`);
  const offset = read24(rom, MAP_INIT_EVENT + mapId * 3);
  if (offset >= EVENT_SCRIPT_SIZE) throw new Error(`Startup event of map ${mapId} is outside the event script.`);
  return offset;
}
