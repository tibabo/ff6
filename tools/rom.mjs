import { createHash } from 'node:crypto';
import { crc32 } from 'node:zlib';

export const ROM_SIZE = 0x300000;
export const COPIER_HEADER_SIZE = 512;
export const SUPPORTED_ROM = Object.freeze({
  id: 'ff3-us-1.1',
  title: 'FINAL FANTASY 3',
  crc32: 'C0FA0464',
  sha256: '10eccc5d2fab81346dd759f6be478dcb682eef981e8d3d662da176e1f9a996bc',
});

export function hex(value, width = 6) {
  return value.toString(16).toUpperCase().padStart(width, '0');
}

export function sha256(bytes) {
  return createHash('sha256').update(bytes).digest('hex');
}

export function snesChecksum(bytes) {
  if (bytes.length !== ROM_SIZE) {
    throw new Error('The checksum implementation requires a 3 MiB ROM.');
  }
  let sum = 0;
  for (let offset = 0; offset < bytes.length; offset += 1) {
    sum = (sum + bytes[offset] * (offset >= 0x200000 ? 2 : 1)) & 0xffff;
  }
  return sum;
}

export function inspectRom(input) {
  if (!Buffer.isBuffer(input)) {
    throw new TypeError('ROM input must be a Buffer.');
  }
  if (input.length !== ROM_SIZE && input.length !== ROM_SIZE + COPIER_HEADER_SIZE) {
    throw new Error(`Expected 3145728 or 3146240 bytes; received ${input.length}.`);
  }
  const copierHeaderBytes = input.length - ROM_SIZE;
  const bytes = input.subarray(copierHeaderBytes);
  const header = 0xffc0;
  const checksum = bytes.readUInt16LE(header + 0x1e);
  const complement = bytes.readUInt16LE(header + 0x1c);
  const computed = snesChecksum(bytes);
  const fingerprint = sha256(bytes);
  const title = bytes.toString('ascii', header, header + 21).trimEnd();
  const issues = [];

  if (title !== SUPPORTED_ROM.title) issues.push('Unexpected SNES title.');
  if (bytes[header + 0x15] !== 0x31) issues.push('Expected Fast HiROM mapping (0x31).');
  if (bytes[header + 0x19] !== 1) issues.push('Expected the US region.');
  if (bytes[header + 0x1b] !== 1) issues.push('Only US revision 1.1 is currently supported.');
  if ((checksum ^ complement) !== 0xffff) issues.push('Invalid checksum complement.');
  if (checksum !== computed) issues.push('SNES checksum mismatch.');
  if (fingerprint !== SUPPORTED_ROM.sha256) {
    issues.push('Unrecognized SHA-256: modified ROMs and other revisions are not supported.');
  }

  return {
    bytes,
    metadata: {
      schemaVersion: 1,
      profile: fingerprint === SUPPORTED_ROM.sha256 ? SUPPORTED_ROM.id : null,
      fileBytes: input.length,
      copierHeaderBytes,
      romBytes: bytes.length,
      title,
      mapping: hex(bytes[header + 0x15], 2),
      region: bytes[header + 0x19],
      revision: bytes[header + 0x1b],
      crc32: hex(crc32(bytes), 8),
      sha256: fingerprint,
      checksum: {
        stored: hex(checksum, 4),
        complement: hex(complement, 4),
        computed: hex(computed, 4),
        valid: checksum === computed && (checksum ^ complement) === 0xffff,
      },
      supported: issues.length === 0,
      issues,
    },
  };
}

export function requireSupportedRom(input) {
  const result = inspectRom(input);
  if (!result.metadata.supported) {
    throw new Error(`ROM validation failed:\n${result.metadata.issues.join('\n')}`);
  }
  return result;
}