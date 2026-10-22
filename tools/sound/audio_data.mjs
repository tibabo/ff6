// audio.ff6data: the samples and the music scripts, raw as the sound program receives them.
import { SAMPLE_COUNT, readSample, readSongCount, readSongSamples, SONG_SAMPLE_SLOTS } from './sample_brr.mjs';
import { readSongScript } from './song_script.mjs';
import { readSpcBlocks } from './spc_blocks.mjs';

export const AUDIO_MAGIC = 'FF6AUDIO';

// `version` is the version of the whole pack. After the songs come the blocks of the sound memory: address, size, bytes.
export function buildAudioData(rom, version) {
  const parts = [Buffer.from(AUDIO_MAGIC, 'ascii')];
  const u32 = value => {
    const part = Buffer.alloc(4);
    part.writeUInt32LE(value);
    parts.push(part);
  };
  const u16 = value => {
    const part = Buffer.alloc(2);
    part.writeUInt16LE(value);
    parts.push(part);
  };
  u32(version);
  u32(SAMPLE_COUNT);
  for (let index = 0; index < SAMPLE_COUNT; index += 1) {
    const sample = readSample(rom, index);
    u16(sample.loopStart);
    u16(sample.freqMult);
    parts.push(Buffer.from([sample.adsr1, sample.adsr2]));
    u32(sample.brr.length);
    parts.push(sample.brr);
  }
  const songCount = readSongCount(rom);
  u32(songCount);
  for (let song = 0; song < songCount; song += 1) {
    const samples = readSongSamples(rom, song);
    if (samples.length !== SONG_SAMPLE_SLOTS) throw new Error('Unexpected number of song instruments.');
    for (const sample of samples) u16(sample + 1);  // 0 for an empty instrument, like the game
    const script = readSongScript(rom, song);
    u32(script.length);
    parts.push(script);
  }
  const blocks = readSpcBlocks(rom);
  u32(blocks.length);
  for (const block of blocks) {
    u16(block.address);
    u32(block.data.length);
    parts.push(block.data);
  }
  return Buffer.concat(parts);
}
