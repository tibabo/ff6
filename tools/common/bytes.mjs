// Shared byte helpers and the LZSS decompressor (Decompress in the disassembly).

export function read24(rom, offset) {
  return rom[offset] | (rom[offset + 1] << 8) | (rom[offset + 2] << 16);
}

export function slice(rom, offset, length, label) {
  if (!Number.isInteger(offset) || offset < 0 || offset + length > rom.length) {
    throw new Error(`${label} is outside the ROM (0x${offset.toString(16)}).`);
  }
  return rom.subarray(offset, offset + length);
}

export function decompressLzss(rom, offset, maxOutput = 0x10000) {
  if (offset < 0 || offset + 2 > rom.length) throw new Error('LZSS header is outside the ROM.');
  const length = rom.readUInt16LE(offset);
  if (length < 2 || offset + length > rom.length) throw new Error('Invalid LZSS block length.');
  const src = rom.subarray(offset, offset + length);
  const ring = new Uint8Array(0x800);
  let ringPos = 0x800 - 34;
  const out = [];
  let s = 2;
  while (s < length) {
    let header = src[s++];
    for (let bit = 0; bit < 8 && s < length; bit += 1, header >>= 1) {
      if (header & 1) {
        const value = src[s++];
        ring[ringPos] = value;
        ringPos = (ringPos + 1) & 0x7ff;
        out.push(value);
      } else {
        if (s + 1 >= length) throw new Error('Truncated LZSS reference.');
        const word = src[s] | (src[s + 1] << 8);
        s += 2;
        const run = (word >> 11) + 3;
        for (let i = 0; i < run; i += 1) {
          const value = ring[(word + i) & 0x7ff];
          ring[ringPos] = value;
          ringPos = (ringPos + 1) & 0x7ff;
          out.push(value);
        }
      }
      if (out.length > maxOutput) throw new Error('LZSS output exceeds the expected size.');
    }
  }
  return Buffer.from(out);
}

// Pointer tables hold offsets relative to the table start; entry i spans ptr[i]..ptr[i+1].
export function readPointedItems(rom, tableOffset, itemSize, index) {
  const start = tableOffset + rom.readUInt16LE(tableOffset + index * 2);
  const end = tableOffset + rom.readUInt16LE(tableOffset + index * 2 + 2);
  if (end < start || (end - start) % itemSize !== 0) throw new Error(`Bad pointer table entry ${index} at 0x${tableOffset.toString(16)}.`);
  return slice(rom, start, end - start, 'Pointed items');
}
