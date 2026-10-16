import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import {
  COPIER_HEADER_SIZE,
  ROM_SIZE,
  SUPPORTED_ROM,
  inspectRom,
  requireSupportedRom,
  snesChecksum,
} from './rom.mjs';

function syntheticRom() {
  const bytes = Buffer.alloc(ROM_SIZE);
  bytes.write('FINAL FANTASY 3       ', 0xffc0, 'ascii');
  bytes[0xffd5] = 0x31;
  bytes[0xffd9] = 1;
  bytes[0xffdb] = 1;
  bytes.writeUInt16LE(0xffff, 0xffdc);
  const checksum = snesChecksum(bytes);
  bytes.writeUInt16LE(checksum ^ 0xffff, 0xffdc);
  bytes.writeUInt16LE(checksum, 0xffde);
  return bytes;
}

test('copier headers do not change the normalized identity', () => {
  const payload = syntheticRom();
  const plain = inspectRom(payload);
  const headered = inspectRom(Buffer.concat([Buffer.alloc(COPIER_HEADER_SIZE, 0xa5), payload]));
  assert.equal(plain.metadata.sha256, headered.metadata.sha256);
  assert.equal(plain.metadata.crc32, headered.metadata.crc32);
  assert.equal(plain.metadata.copierHeaderBytes, 0);
  assert.equal(headered.metadata.copierHeaderBytes, 512);
  assert.deepEqual(plain.bytes, headered.bytes);
});

test('checksum mirrors the final MiB of a 3 MiB ROM', () => {
  const bytes = Buffer.alloc(ROM_SIZE);
  bytes[0x100] = 3;
  bytes[0x200100] = 7;
  assert.equal(snesChecksum(bytes), 17);
});

test('a plausible header and checksum do not authorize an unknown ROM', () => {
  const bytes = syntheticRom();
  const { metadata } = inspectRom(bytes);
  assert.equal(metadata.checksum.valid, true);
  assert.equal(metadata.profile, null);
  assert.equal(metadata.supported, false);
  assert.equal(metadata.issues.length, 1);
  assert.throws(() => requireSupportedRom(bytes), /Unrecognized SHA-256/);
});

test('damaged data and checksum complements are reported', () => {
  const bytes = syntheticRom();
  bytes[0x100] ^= 1;
  assert.ok(inspectRom(bytes).metadata.issues.includes('SNES checksum mismatch.'));
  bytes[0xffdc] ^= 1;
  assert.ok(inspectRom(bytes).metadata.issues.includes('Invalid checksum complement.'));
});

test('wrong sizes and wrong input types are rejected', () => {
  for (const size of [0, 512, ROM_SIZE - 1, ROM_SIZE + 1, ROM_SIZE + 1024]) {
    assert.throws(() => inspectRom(Buffer.alloc(size)), /Expected 3145728 or 3146240/);
  }
  assert.throws(() => inspectRom(new Uint8Array(ROM_SIZE)), TypeError);
  assert.throws(() => snesChecksum(Buffer.alloc(1)), /requires a 3 MiB ROM/);
});

test('inspection does not modify the source buffer', () => {
  const input = syntheticRom();
  const before = Buffer.from(input);
  inspectRom(input);
  assert.deepEqual(input, before);
});

test('the user-supplied ROM matches the supported reference', {
  skip: !process.env.FF6_ROM,
}, () => {
  const input = readFileSync(process.env.FF6_ROM);
  const { bytes, metadata } = requireSupportedRom(input);
  assert.equal(metadata.profile, SUPPORTED_ROM.id);
  assert.equal(metadata.crc32, SUPPORTED_ROM.crc32);
  assert.equal(metadata.checksum.stored, '8A60');
  assert.equal(requireSupportedRom(bytes).metadata.sha256, metadata.sha256);
  const damaged = Buffer.from(input);
  damaged[metadata.copierHeaderBytes + 0x100] ^= 1;
  assert.throws(() => requireSupportedRom(damaged), /ROM validation failed/);
});