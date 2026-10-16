import {
  lstatSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, renameSync, rmSync, writeFileSync,
} from 'node:fs';
import { basename, dirname, join, resolve } from 'node:path';
import { extractGraphics } from './graphics.mjs';
import { requireSupportedRom, sha256 } from './rom.mjs';
import { extractDialogues, validateDialogues } from './text.mjs';

export function createAssetFiles(input) {
  const { bytes, metadata } = requireSupportedRom(input);
  const dialogues = extractDialogues(bytes);
  validateDialogues(dialogues);
  const { assets, files } = extractGraphics(bytes);
  files.set('text/dialogues.json', Buffer.from(`${JSON.stringify(dialogues, null, 2)}\n`));
  const manifest = {
    schemaVersion: 1,
    kind: 'ff6-extracted-assets',
    toolVersion: '0.1.0',
    source: {
      profile: metadata.profile,
      romBytes: metadata.romBytes,
      crc32: metadata.crc32,
      sha256: metadata.sha256,
    },
    scope: {
      fieldDialogues: dialogues.entries.length,
      portraits: 19,
      characterTileAtlases: 14,
      spriteAnimationsAssembled: false,
      nativeRuntime: false,
    },
    dialogues: 'text/dialogues.json',
    graphics: assets,
    files: [...files.keys()].sort().map(path => ({
      path, bytes: files.get(path).length, sha256: sha256(files.get(path)),
    })),
  };
  files.set('manifest.json', Buffer.from(`${JSON.stringify(manifest, null, 2)}\n`));
  return { manifest, files };
}

export function writeAssetFiles(input, outputPath) {
  const destination = resolve(outputPath);
  const { manifest, files } = createAssetFiles(input);
  mkdirSync(dirname(destination), { recursive: true });
  try {
    mkdirSync(destination);
  } catch (error) {
    if (error.code === 'EEXIST') {
      throw new Error(`Output already exists; nothing was overwritten: ${destination}`);
    }
    throw error;
  }
  let staging;
  let published = false;
  try {
    staging = mkdtempSync(join(dirname(destination), `.${basename(destination)}-extract-`));
    for (const [relativePath, content] of files) {
      const target = join(staging, ...relativePath.split('/'));
      mkdirSync(dirname(target), { recursive: true });
      writeFileSync(target, content, { flag: 'wx' });
    }
    for (const child of readdirSync(staging)) {
      if (child !== 'manifest.json') renameSync(join(staging, child), join(destination, child));
    }
    renameSync(join(staging, 'manifest.json'), join(destination, 'manifest.json'));
    published = true;
    return { destination, manifest, fileCount: files.size };
  } finally {
    if (staging) rmSync(staging, { recursive: true, force: true });
    if (!published) rmSync(destination, { recursive: true, force: true });
  }
}

export function verifyAssetFiles(input, outputPath) {
  const destination = resolve(outputPath);
  if (!lstatSync(destination).isDirectory() || lstatSync(destination).isSymbolicLink()) {
    throw new Error('The extraction root must be a real directory, not a symbolic link.');
  }
  const { files } = createAssetFiles(input);
  for (const [relativePath, expected] of files) {
    let target = destination;
    const parts = relativePath.split('/');
    for (const [index, part] of parts.entries()) {
      target = join(target, part);
      const info = lstatSync(target);
      if (info.isSymbolicLink()) throw new Error(`Symbolic links are not allowed: ${relativePath}`);
      if (index < parts.length - 1 && !info.isDirectory()) {
        throw new Error(`Expected a directory in ${relativePath}`);
      }
      if (index === parts.length - 1 && (!info.isFile() || info.size !== expected.length)) {
        throw new Error(`Missing or modified asset: ${relativePath}`);
      }
    }
    if (!readFileSync(target).equals(expected)) {
      throw new Error(`Modified asset: ${relativePath}`);
    }
  }
  return { destination, verifiedFiles: files.size };
}