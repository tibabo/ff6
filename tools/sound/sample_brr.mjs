// src/sound/sample_brr.asm: instrument samples (BRR) and their per-sample parameters, and the sample list of each song.
import { read24 } from '../common/bytes.mjs';

export const NUM_SONGS_OFFSET = 0x053c5e;  // c5/3c5e
export const SAMPLE_BRR_PTRS = 0x053c5f;  // c5/3c5f, 3-byte pointers
export const SAMPLE_LOOP_START = 0x053d1c;  // c5/3d1c, words
export const SAMPLE_FREQ_MULT = 0x053d9a;  // c5/3d9a, words
export const SAMPLE_ADSR = 0x053e18;  // c5/3e18, 2 bytes: DSP ADSR1 then ADSR2
export const SONG_SCRIPT_PTRS = 0x053e96;  // c5/3e96, 3-byte pointers; the song sample lists follow
export const SAMPLE_COUNT = (SAMPLE_LOOP_START - SAMPLE_BRR_PTRS) / 3;
export const SONG_SAMPLE_SLOTS = 16;  // instruments of a song, 2 bytes each: sample number + 1, 0 for none
export const BRR_BLOCK_BYTES = 9;

const romOffset = address => address & 0x3fffff;

export function readSongCount(rom) {
  return rom[NUM_SONGS_OFFSET];
}

// One sample: loop start (bytes into the BRR data), pitch multiplier, ADSR registers and the BRR blocks.
export function readSample(rom, index) {
  if (index < 0 || index >= SAMPLE_COUNT) throw new Error(`Sample ${index} does not exist.`);
  const start = romOffset(read24(rom, SAMPLE_BRR_PTRS + index * 3));
  const size = rom.readUInt16LE(start);
  if (size % BRR_BLOCK_BYTES !== 0) throw new Error(`Sample ${index} does not hold whole BRR blocks.`);
  return {
    loopStart: rom.readUInt16LE(SAMPLE_LOOP_START + index * 2),
    freqMult: rom.readUInt16LE(SAMPLE_FREQ_MULT + index * 2),
    adsr1: rom[SAMPLE_ADSR + index * 2],
    adsr2: rom[SAMPLE_ADSR + index * 2 + 1],
    brr: rom.subarray(start + 2, start + 2 + size),
  };
}

// The samples of a song, in the order of its instruments; -1 marks an empty instrument.
export function readSongSamples(rom, song) {
  const table = SONG_SCRIPT_PTRS + readSongCount(rom) * 3 + song * SONG_SAMPLE_SLOTS * 2;
  const samples = [];
  for (let slot = 0; slot < SONG_SAMPLE_SLOTS; slot += 1) samples.push(rom.readUInt16LE(table + slot * 2) - 1);
  return samples;
}
