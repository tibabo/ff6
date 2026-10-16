import { ROM_SIZE, SUPPORTED_ROM, hex } from './rom.mjs';

export const DIALOGUE_COUNT = 3084;
const POINTER_TABLE = 0x0ce602;
const DIALOGUE_START = 0x0d0000;
const DIALOGUE_END = 0x0eefbb;
const DICTIONARY_START = 0x00dfa0;
const ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
const GLYPHS = new Map([...ALPHABET].map((letter, index) => [0x20 + index, letter]));
for (const [code, text] of [
  [0x5e, '!'], [0x5f, '?'], [0x60, '/'], [0x61, ':'],
  [0x62, '"'], [0x63, "'"], [0x64, '-'], [0x65, '.'],
  [0x66, ','], [0x67, '\u2026'], [0x68, ';'], [0x69, '#'],
  [0x6a, '+'], [0x6b, '('], [0x6c, ')'], [0x6d, '%'],
  [0x6e, '~'], [0x6f, '*'], [0x70, '@'], [0x72, '='],
  [0x73, '\u201c'], [0x7f, ' '],
]) GLYPHS.set(code, text);

const ICONS = new Map([
  [0x71, 'note'], [0x76, 'holy'], [0x77, 'times'],
  [0x78, 'lightning'], [0x79, 'wind'], [0x7a, 'earth'],
  [0x7b, 'ice'], [0x7c, 'fire'], [0x7d, 'water'], [0x7e, 'poison'],
]);

export const CONTROLS = new Map([
  [0x01, { name: 'newline', arguments: 0 }],
  [0x10, { name: 'wait', arguments: 0 }],
  [0x11, { name: 'wait', arguments: 1 }],
  [0x12, { name: 'key', arguments: 0 }],
  [0x13, { name: 'page', arguments: 0 }],
  [0x14, { name: 'tab', arguments: 1 }],
  [0x15, { name: 'choice', arguments: 0 }],
  [0x16, { name: 'key', arguments: 1 }],
  [0x19, { name: 'gp', arguments: 0 }],
  [0x1a, { name: 'item', arguments: 0 }],
  [0x1b, { name: 'spell', arguments: 0 }],
]);

export function readDictionary(rom) {
  const dictionary = [];
  for (let index = 0; index < 128; index += 1) {
    const offset = DICTIONARY_START + index * 2;
    const first = GLYPHS.get(rom[offset]);
    const second = GLYPHS.get(rom[offset + 1]);
    if (first === undefined || second === undefined) {
      throw new Error(`Invalid DTE dictionary entry ${index}.`);
    }
    dictionary.push(first + second);
  }
  return dictionary;
}

export function decodeDialogue(encoded, dictionary) {
  const tokens = [];
  let position = 0;
  while (position < encoded.length) {
    const code = encoded[position++];
    if (code === 0) return { tokens, bytesConsumed: position };
    if (code >= 2 && code <= 15) {
      tokens.push({ type: 'name', characterId: code - 2 });
      continue;
    }
    const control = CONTROLS.get(code);
    if (control) {
      if (position + control.arguments > encoded.length) {
        throw new Error(`Truncated control 0x${hex(code, 2)}.`);
      }
      const args = [...encoded.subarray(position, position + control.arguments)];
      position += control.arguments;
      tokens.push({ type: 'control', code, name: control.name, args });
      continue;
    }
    if (ICONS.has(code)) {
      tokens.push({ type: 'icon', code, name: ICONS.get(code) });
      continue;
    }
    const text = code >= 0x80 ? dictionary[code - 0x80] : GLYPHS.get(code);
    if (text === undefined) {
      throw new Error(`Unknown dialogue byte 0x${hex(code, 2)} at relative offset ${position - 1}.`);
    }
    if (typeof tokens.at(-1) === 'string') tokens[tokens.length - 1] += text;
    else tokens.push(text);
  }
  throw new Error('Dialogue has no terminator within its data region.');
}

export function extractDialogues(rom) {
  if (rom.length !== ROM_SIZE) throw new Error('Expected a normalized 3 MiB ROM.');
  const dictionary = readDictionary(rom);
  const bankSwitchIndex = rom.readUInt16LE(POINTER_TABLE - 2);
  if (bankSwitchIndex > DIALOGUE_COUNT) throw new Error('Invalid dialogue bank switch.');
  const offsets = Array.from({ length: DIALOGUE_COUNT }, (_, index) => (
    DIALOGUE_START + rom.readUInt16LE(POINTER_TABLE + index * 2)
    + (index >= bankSwitchIndex ? 0x10000 : 0)
  ));
  for (let index = 0; index < offsets.length; index += 1) {
    if (offsets[index] >= DIALOGUE_END || (index > 0 && offsets[index] < offsets[index - 1])) {
      throw new Error(`Invalid pointer for dialogue ${index}.`);
    }
  }
  const uniqueOffsets = [...new Set(offsets)];
  const boundaries = new Map(uniqueOffsets.map((offset, index) => [
    offset, uniqueOffsets[index + 1] ?? DIALOGUE_END,
  ]));
  const entries = offsets.map((offset, id) => {
    const remaining = rom.subarray(offset, DIALOGUE_END);
    let decoded;
    try {
      decoded = decodeDialogue(remaining, dictionary);
    } catch (error) {
      throw new Error(`Dialogue ${id} at 0x${hex(offset)}: ${error.message}`, { cause: error });
    }
    return {
      id,
      source: {
        offset: `0x${hex(offset)}`,
        bytes: decoded.bytesConsumed,
        nextPointerOffset: `0x${hex(boundaries.get(offset))}`,
        overlapsNext: offset + decoded.bytesConsumed > boundaries.get(offset),
        encodedHex: remaining.subarray(0, decoded.bytesConsumed).toString('hex'),
      },
      tokens: decoded.tokens,
    };
  });
  return {
    schemaVersion: 1,
    kind: 'ff6-field-dialogues',
    sourceRom: SUPPORTED_ROM.id,
    sourceSha256: SUPPORTED_ROM.sha256,
    bankSwitchIndex,
    entries,
  };
}

export function validateDialogues(document) {
  if (!document || document.schemaVersion !== 1 || document.kind !== 'ff6-field-dialogues'
      || document.sourceRom !== SUPPORTED_ROM.id || document.sourceSha256 !== SUPPORTED_ROM.sha256
      || !Array.isArray(document.entries) || document.entries.length !== DIALOGUE_COUNT) {
    throw new Error('Invalid field-dialogue document or unsupported source ROM.');
  }
  for (const [index, entry] of document.entries.entries()) {
    if (!entry || entry.id !== index || !Array.isArray(entry.tokens)) {
      throw new Error(`Invalid dialogue ID or tokens at entry ${index}.`);
    }
    const source = entry.source;
    if (!source || !/^0x[0-9A-F]{6}$/.test(source.offset)
        || !Number.isInteger(source.bytes) || source.bytes < 1
        || !/^0x[0-9A-F]{6}$/.test(source.nextPointerOffset)
        || Number(source.offset) < DIALOGUE_START || Number(source.offset) + source.bytes > DIALOGUE_END
        || Number(source.nextPointerOffset) <= Number(source.offset)
        || Number(source.nextPointerOffset) > DIALOGUE_END
        || source.overlapsNext !== (Number(source.offset) + source.bytes > Number(source.nextPointerOffset))
        || typeof source.encodedHex !== 'string' || !/^(?:[0-9a-f]{2})+$/.test(source.encodedHex)
        || source.encodedHex.length !== source.bytes * 2) {
      throw new Error(`Invalid source metadata in dialogue ${index}.`);
    }
    for (const token of entry.tokens) {
      if (typeof token === 'string') continue;
      if (token?.type === 'name' && Number.isInteger(token.characterId)
          && token.characterId >= 0 && token.characterId < 14) continue;
      if (token?.type === 'icon' && ICONS.get(token.code) === token.name
          && ICONS.has(token.code)) continue;
      const control = token?.type === 'control' ? CONTROLS.get(token.code) : undefined;
      if (control && token.name === control.name && Array.isArray(token.args)
          && token.args.length === control.arguments
          && token.args.every(value => Number.isInteger(value) && value >= 0 && value <= 255)) continue;
      throw new Error(`Invalid token in dialogue ${index}: ${JSON.stringify(token)}.`);
    }
  }
  return document.entries.length;
}