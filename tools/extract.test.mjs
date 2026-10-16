import assert from 'node:assert/strict';
import {
  existsSync, mkdtempSync, readFileSync, rmSync, symlinkSync, writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { test } from 'node:test';
import { createAssetFiles, verifyAssetFiles, writeAssetFiles } from './extract.mjs';
import { requireSupportedRom, sha256 } from './rom.mjs';

const executable = fileURLToPath(new URL('../ff6.mjs', import.meta.url));
const cli = (...args) => spawnSync(process.execPath, [executable, ...args], { encoding: 'utf8' });

function temporaryDirectory(context) {
  const directory = mkdtempSync(join(tmpdir(), 'ff6-tools-test-'));
  context.after(() => rmSync(directory, { recursive: true, force: true }));
  return directory;
}

test('CLI help and invalid arguments are explicit', () => {
  assert.equal(cli('--help').status, 0);
  assert.match(cli('--help').stdout, /no emulator/);
  assert.equal(cli('unknown').status, 1);
  assert.match(cli('extract').stderr, /Missing --rom/);
  assert.equal(cli('inspect', '--rom', 'missing', '--out', 'unexpected').status, 1);
  assert.equal(cli('extract', '--force').status, 1);
});

test('an invalid ROM never creates an output directory', context => {
  const directory = temporaryDirectory(context);
  const source = join(directory, 'broken.smc');
  const destination = join(directory, 'assets');
  writeFileSync(source, Buffer.alloc(512));
  const result = cli('extract', '--rom', source, '--out', destination);
  assert.equal(result.status, 1);
  assert.match(result.stderr, /Expected 3145728 or 3146240/);
  assert.equal(existsSync(destination), false);
  assert.equal(readFileSync(source).length, 512);
});

test('headered and headerless ROMs generate identical asset packages', {
  skip: !process.env.FF6_ROM,
}, () => {
  const original = readFileSync(process.env.FF6_ROM);
  const { bytes } = requireSupportedRom(original);
  const first = createAssetFiles(original);
  const second = createAssetFiles(bytes);
  assert.equal(first.files.size, 101);
  assert.deepEqual(first.manifest, second.manifest);
  for (const [path, data] of first.files) assert.deepEqual(data, second.files.get(path));
  for (const file of first.manifest.files) {
    assert.equal(first.files.get(file.path).length, file.bytes);
    assert.equal(sha256(first.files.get(file.path)), file.sha256);
    assert.equal(file.path.includes('\\'), false);
  }
});

test('extraction, verification and edited text validation work without overwrites', {
  skip: !process.env.FF6_ROM,
}, context => {
  const directory = temporaryDirectory(context);
  const destination = join(directory, 'assets with spaces');
  const source = process.env.FF6_ROM;
  const original = readFileSync(source);
  const initialHash = sha256(original);
  const extraction = cli('extract', '--rom', source, '--out', destination);
  assert.equal(extraction.status, 0, extraction.stderr);
  assert.equal(JSON.parse(extraction.stdout).files, 101);
  assert.equal(cli('verify', '--rom', source, '--out', destination).status, 0);
  const textPath = join(destination, 'text', 'dialogues.json');
  const dialogue = JSON.parse(readFileSync(textPath, 'utf8'));
  dialogue.entries[0].tokens = ['Bonjour, \u00e9dition locale.'];
  writeFileSync(textPath, JSON.stringify(dialogue));
  assert.equal(cli('check-text', '--file', textPath).status, 0);
  const editedBytes = readFileSync(textPath);
  assert.equal(cli('extract', '--rom', source, '--out', destination).status, 1);
  assert.deepEqual(readFileSync(textPath), editedBytes);
  assert.equal(cli('verify', '--rom', source, '--out', destination).status, 1);
  assert.throws(() => writeAssetFiles(original, source), /Output already exists/);
  assert.equal(sha256(readFileSync(source)), initialHash);
});

test('verification trusts the ROM, not editable manifest hashes', {
  skip: !process.env.FF6_ROM,
}, context => {
  const directory = temporaryDirectory(context);
  const destination = join(directory, 'assets');
  const original = readFileSync(process.env.FF6_ROM);
  writeAssetFiles(original, destination);
  const manifestPath = join(destination, 'manifest.json');
  const manifest = JSON.parse(readFileSync(manifestPath, 'utf8'));
  manifest.files[0].sha256 = '0'.repeat(64);
  writeFileSync(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);
  assert.throws(() => verifyAssetFiles(original, destination), /Modified asset: manifest.json/);
});

test('existing symbolic-link destinations are refused', {
  skip: !process.env.FF6_ROM || process.platform === 'win32',
}, context => {
  const directory = temporaryDirectory(context);
  const destination = join(directory, 'linked');
  symlinkSync(process.env.FF6_ROM, destination);
  const original = readFileSync(process.env.FF6_ROM);
  assert.throws(() => writeAssetFiles(original, destination), /Output already exists/);
  assert.throws(() => verifyAssetFiles(original, destination), /real directory/);
});