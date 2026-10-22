// src/sound/sound_main.asm: the blocks of SPC data that the game sends to the sound program when it starts.
// They hold the sound program, the sound effect scripts and their instruments.

const SPC_BLOCK_SOURCES = 0x050010;  // c5/0010: addresses of the blocks, relative to c5/0000
const SPC_BLOCK_DESTINATIONS = 0x05001c;  // c5/001c: where each block goes in the sound memory
const SPC_BLOCK_BANK = 0x050000;
export const SPC_BLOCK_COUNT = 6;

// Each block starts with its size in bytes.
export function readSpcBlocks(rom) {
  const blocks = [];
  for (let i = 0; i < SPC_BLOCK_COUNT; i += 1) {
    const start = SPC_BLOCK_BANK + rom.readUInt16LE(SPC_BLOCK_SOURCES + i * 2);
    const address = rom.readUInt16LE(SPC_BLOCK_DESTINATIONS + i * 2);
    const size = rom.readUInt16LE(start);
    if (start + 2 + size > rom.length || address + size > 0x10000) throw new Error(`SPC block ${i} is outside its memory.`);
    blocks.push({ address, data: rom.subarray(start + 2, start + 2 + size) });
  }
  return blocks;
}
