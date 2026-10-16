import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { requireSupportedRom } from './rom.mjs';
import {
  DIALOGUE_COUNT, decodeDialogue, extractDialogues, readDictionary, validateDialogues,
} from './text.mjs';

const dictionary = Array.from({ length: 128 }, () => 'ab');

test('dialogue decoding expands DTE while preserving structural tokens', () => {
  const encoded = Buffer.from([0x20, 0x80, 0x7f, 2, 1, 0x11, 0, 0x71, 0]);
  assert.deepEqual(decodeDialogue(encoded, dictionary), {
    tokens: [
      'Aab ', { type: 'name', characterId: 0 },
      { type: 'control', code: 1, name: 'newline', args: [] },
      { type: 'control', code: 0x11, name: 'wait', args: [0] },
      { type: 'icon', code: 0x71, name: 'note' },
    ],
    bytesConsumed: encoded.length,
  });
});

test('unknown bytes, truncated controls and unterminated text fail explicitly', () => {
  assert.throws(() => decodeDialogue(Buffer.from([0x17, 0]), dictionary), /Unknown dialogue byte/);
  assert.throws(() => decodeDialogue(Buffer.from([0x11]), dictionary), /Truncated control/);
  assert.throws(() => decodeDialogue(Buffer.from([0x20]), dictionary), /no terminator/);
  assert.throws(() => decodeDialogue(Buffer.from([0x80, 0]), []), /Unknown dialogue byte/);
});

test('all 3084 real dialogues decode within the data region, including shared suffixes', {
  skip: !process.env.FF6_ROM,
}, () => {
  const { bytes } = requireSupportedRom(readFileSync(process.env.FF6_ROM));
  const document = extractDialogues(bytes);
  assert.equal(document.entries.length, DIALOGUE_COUNT);
  assert.equal(document.bankSwitchIndex, 1574);
  assert.equal(validateDialogues(document), DIALOGUE_COUNT);
  assert.deepEqual(document.entries[0].tokens, document.entries[1].tokens);
  assert.equal(document.entries[2125].source.overlapsNext, true);
  assert.equal(document.entries[2125].source.bytes, 66);
  assert.equal(document.entries[2126].source.bytes, 38);
  const actualDictionary = readDictionary(bytes);
  for (const entry of document.entries) {
    const encoded = Buffer.from(entry.source.encodedHex, 'hex');
    const offset = Number(entry.source.offset);
    assert.deepEqual(encoded, bytes.subarray(offset, offset + entry.source.bytes));
    assert.deepEqual(decodeDialogue(encoded, actualDictionary).tokens, entry.tokens);
    assert.ok(offset + entry.source.bytes <= 0x0eefbb);
  }
  document.entries[0].tokens = ['Texte modifie : \u00e9\u00e8\u00e0\u00e7.'];
  assert.equal(validateDialogues(document), DIALOGUE_COUNT);
  document.entries[0].tokens.push({ type: 'control', code: 0x11, name: 'wait', args: [256] });
  assert.throws(() => validateDialogues(document), /Invalid token in dialogue 0/);
});