import { readFileSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { parseArgs } from 'node:util';

const HELP = `FF6 ROM tools 0.1.0 (Node.js 24 or newer; no emulator)

  node ff6.mjs inspect --rom "game.smc"
  node ff6.mjs extract --rom "game.smc" --out output/ff3-us-1.1
  node ff6.mjs verify --rom "game.smc" --out output/ff3-us-1.1
  node ff6.mjs check-text --file output/ff3-us-1.1/text/dialogues.json
  node ff6.mjs build-pack --rom "game.smc" --out output/pack [--text edited/dialogues.json]
  node ff6.mjs test [--rom "game.smc"]

Only the unmodified US 1.1 ROM is supported, with or without a copier header.
Extraction and pack builds never overwrite existing files or the source ROM.
Verify checks an unmodified extraction against the ROM, not just its manifest.
Check-text validates edited dialogue JSON; it does not patch or run the game.
Build-pack writes the data that the native engine in engine/ reads: the font, sprites,
dialogue and event script, and every field map.
`;

async function main() {
  if (Number(process.versions.node.split('.')[0]) < 24) {
    throw new Error('Node.js 24 or newer is required.');
  }
  const { positionals, values } = parseArgs({
    allowPositionals: true,
    options: {
      rom: { type: 'string' }, out: { type: 'string' }, file: { type: 'string' },
      text: { type: 'string' },
      help: { type: 'boolean', short: 'h' },
    },
  });
  if (values.help || positionals.length === 0) {
    process.stdout.write(HELP);
    return;
  }
  if (positionals.length !== 1) throw new Error('Expected exactly one command. Use --help.');
  const command = positionals[0];
  const allowed = {
    inspect: ['rom'], extract: ['rom', 'out'], verify: ['rom', 'out'],
    'check-text': ['file'], test: ['rom'],
    'build-pack': ['rom', 'out', 'text'],
  };
  const required = {
    inspect: ['rom'], extract: ['rom', 'out'], verify: ['rom', 'out'],
    'check-text': ['file'], test: [], 'build-pack': ['rom', 'out'],
  };
  if (!Object.hasOwn(allowed, command)) throw new Error(`Unknown command: ${command}`);
  for (const name of Object.keys(values)) {
    if (!allowed[command].includes(name)) throw new Error(`--${name} is not valid for ${command}.`);
  }
  for (const name of required[command]) {
    if (!values[name]) throw new Error(`Missing --${name} for ${command}.`);
  }
  const { inspectRom } = await import('./tools/rom.mjs');
  const { writeAssetFiles, verifyAssetFiles } = await import('./tools/extract.mjs');
  const { validateDialogues } = await import('./tools/text.mjs');
  let result;
  if (command === 'test') {
    const tools = new URL('./tools/', import.meta.url);
    const tests = readdirSync(tools).filter(name => name.endsWith('.test.mjs')).sort()
      .map(name => fileURLToPath(new URL(name, tools)));
    const env = { ...process.env };
    if (values.rom) env.FF6_ROM = resolve(values.rom);
    const child = spawnSync(process.execPath, ['--test', ...tests], { env, stdio: 'inherit' });
    if (child.error) throw child.error;
    process.exitCode = child.status ?? 1;
    return;
  }
  if (command === 'check-text') {
    const document = JSON.parse(readFileSync(values.file, 'utf8'));
    result = { valid: true, dialogues: validateDialogues(document) };
  } else {
    const input = readFileSync(values.rom);
    if (command === 'inspect') {
      result = inspectRom(input).metadata;
      if (!result.supported) process.exitCode = 1;
    } else if (command === 'build-pack') {
      const { requireSupportedRom } = await import('./tools/rom.mjs');
      const { writePack } = await import('./tools/pack.mjs');
      const destination = resolve(values.out);
      const pack = writePack(requireSupportedRom(input).bytes, destination, { textFile: values.text });
      result = { output: destination, maps: pack.written.length, skipped: pack.skipped };
    } else if (command === 'extract') {
      const extraction = writeAssetFiles(input, values.out);
      result = {
        output: extraction.destination, files: extraction.fileCount,
        ...extraction.manifest.scope,
      };
    } else {
      result = verifyAssetFiles(input, values.out);
    }
  }
  process.stdout.write(`${JSON.stringify(result, null, 2)}\n`);
}

try {
  await main();
} catch (error) {
  process.stderr.write(`Error: ${error.message}\n`);
  process.exitCode = 1;
}