// src/event/event_trigger.asm and the event script itself (event_main.asm).
import { readPointedItems, slice } from '../common/bytes.mjs';

const EVENT_TRIGGER_PTRS = 0x040000;
export const EVENT_TRIGGER_SIZE = 5;
const EVENT_SCRIPT = 0x0a0000;  // CA/0000
export const EVENT_SCRIPT_SIZE = 0x40000;  // npc events reach banks CA-CD

// Raw 5-byte items: x, y, 24-bit offset into the event script.
export function readEventTriggers(rom, mapId) {
  return readPointedItems(rom, EVENT_TRIGGER_PTRS, EVENT_TRIGGER_SIZE, mapId);
}

export function readEventScript(rom) {
  return Buffer.from(slice(rom, EVENT_SCRIPT, EVENT_SCRIPT_SIZE, 'Event script'));
}
