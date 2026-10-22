// src/sound/song_script.asm: the music scripts (Akao sequences), one per song.
import { read24 } from '../common/bytes.mjs';
import { SONG_SCRIPT_PTRS, readSongCount } from './sample_brr.mjs';

const romOffset = address => address & 0x3fffff;

// The bytes that the sound program receives: they start with the header (base, unused pointer, channel pointers).
export function readSongScript(rom, song) {
  if (song < 0 || song >= readSongCount(rom)) throw new Error(`Song ${song} does not exist.`);
  const start = romOffset(read24(rom, SONG_SCRIPT_PTRS + song * 3));
  const size = rom.readUInt16LE(start);
  // The transfer to the sound chip copies size + 2 bytes.
  return rom.subarray(start + 2, start + 2 + size + 2);
}
