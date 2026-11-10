import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import {
  AI_PTR_BYTES, AI_SCRIPT_BYTES, ATTACK_NAME_BYTES, ATTACK_NAME_COUNT, BATTLE_COUNT, BATTLE_MAGIC, BATTLE_MONSTER_BYTES,
  BATTLE_PROP_BYTES, CONTROL_BYTES, GENJU_ATTACK_COUNT, MAP_COUNT, MONSTER_COUNT, MONSTER_ITEM_BYTES,
  MONSTER_NAME_BYTES, MONSTER_PROP_BYTES, RNG_BYTES, SKETCH_BYTES, SUB_RATE_BYTES, buildBattleData, readAiPtrs, readAiScript,
  readAttackNames, readBattleMonsters, readControl, readMonsterItems, readSketch,
  readMonsterNames, readMonsterProp, readRngTable, readSubBattleGroups, readSubBattleRates,
  DANCE_BACKGROUND_BYTES, DANCE_BG_BYTES, DANCE_BYTES, MAGIC_POINT_BYTES, MONSTER_DLG_COUNT, MONSTER_DLG_TEXT_BYTES, RAGE_BYTES,
  BATTLE_DLG_COUNT, BATTLE_DLG_TEXT_BYTES, METAMORPH_BYTES, readBattleDlg, readBattleDlgPtrs, readMetamorph,
  CHAR_AI_BYTES, CHAR_AI_COUNT, COLOSSEUM_BYTES, readCharAi, readColosseum,
  readBattleBgDance, readDanceBg, readDanceProp, readMagicPoints, readMonsterDlg, readMonsterDlgPtrs, readMonsterRage,
} from './battle/battle_data.mjs';
import { requireSupportedRom } from './rom.mjs';

const romPath = process.env.FF6_ROM;
const skip = !romPath;
const loadRom = () => requireSupportedRom(readFileSync(romPath)).bytes;

test('the first monster is the guard, with its points and its name', { skip }, () => {
  const rom = loadRom();
  const guard = readMonsterProp(rom).subarray(0, MONSTER_PROP_BYTES);
  assert.equal(guard[0], 30);  // speed
  assert.equal(guard[1], 16);  // attack
  assert.equal(guard[8] | (guard[9] << 8), 40);  // hp
  assert.deepEqual([...readMonsterNames(rom).subarray(0, 5)], [0x86, 0xae, 0x9a, 0xab, 0x9d]);  // Guard
  assert.equal(readMonsterItems(rom).length, MONSTER_COUNT * MONSTER_ITEM_BYTES);
});

test('the first battle has one monster in its first place, and the other places are empty', { skip }, () => {
  const battle = readBattleMonsters(loadRom()).subarray(0, BATTLE_MONSTER_BYTES);
  assert.equal(battle[2], 0x19);  // the id of the monster, then five empty places
  assert.deepEqual([...battle.subarray(3, 8)], [0xff, 0xff, 0xff, 0xff, 0xff]);
  assert.equal(battle[14] & 1, 0);  // the ninth bit of the first id is clear, the others are set: no monster
  assert.equal(battle[14] & 0x3e, 0x3e);
});

test('battle data holds the tables one after the other', { skip }, () => {
  const data = buildBattleData(loadRom(), 5);
  assert.equal(data.subarray(0, 8).toString('ascii'), BATTLE_MAGIC);
  assert.equal(data.readUInt32LE(8), 5);
  assert.equal(data.length, 12 + MONSTER_COUNT * (MONSTER_PROP_BYTES + MONSTER_ITEM_BYTES + MONSTER_NAME_BYTES)
    + BATTLE_COUNT * (BATTLE_PROP_BYTES + BATTLE_MONSTER_BYTES) + 256 * 8 + 256 * 4 + MAP_COUNT + SUB_RATE_BYTES + RNG_BYTES
    + AI_PTR_BYTES + AI_SCRIPT_BYTES + (GENJU_ATTACK_COUNT + ATTACK_NAME_COUNT) * ATTACK_NAME_BYTES
    + MONSTER_COUNT * (SKETCH_BYTES + CONTROL_BYTES) + MAGIC_POINT_BYTES + RAGE_BYTES + DANCE_BYTES + DANCE_BG_BYTES
    + DANCE_BACKGROUND_BYTES + MONSTER_DLG_COUNT * 2 + MONSTER_DLG_TEXT_BYTES + BATTLE_DLG_COUNT * 2 + BATTLE_DLG_TEXT_BYTES + METAMORPH_BYTES + CHAR_AI_COUNT * CHAR_AI_BYTES + COLOSSEUM_BYTES);
});

test('the tables of the rages, the dances, the magic points and the dialogs of the monsters', { skip }, () => {
  const rom = loadRom();
  assert.deepEqual([...readMonsterRage(rom).subarray(0, 2)], [0xee, 0xef]);  // the guard: the blow, then the special attack
  assert.deepEqual([...readDanceProp(rom).subarray(0, 4)], [101, 102, 112, 117]);  // wind song: wind slash, sun bath, plasma, cockatrice
  assert.equal(readBattleBgDance(rom)[0], 0);  // the field teaches the wind song
  assert.equal(readDanceBg(rom).length, DANCE_BACKGROUND_BYTES);
  assert.equal(readMagicPoints(rom).length, MAGIC_POINT_BYTES);
  assert.ok(readMagicPoints(rom).some(points => points > 0));
  const pointers = readMonsterDlgPtrs(rom), text = readMonsterDlg(rom);
  assert.equal(pointers.readUInt16LE(0), 0xe1e0);  // the first dialog starts where the texts do: "System error!"
  assert.deepEqual([...text.subarray(0, 4)], [0x92, 0xb2, 0xac, 0xad]);  // Syst
  const scenes = readBattleDlgPtrs(rom), scenesText = readBattleDlg(rom);
  assert.equal(scenes.readUInt16LE(0), 0xd200);  // the scenes: the addresses count from where the texts start
  const gau = scenes.readUInt16LE(254 * 2) - 0xd200;
  assert.deepEqual([...scenesText.subarray(gau, gau + 2)], [0x02, 0x0b]);  // Gau's cry begins with his name
  assert.deepEqual([...readMetamorph(rom).subarray(8, 12)], [0xfe, 0xfe, 0xfe, 0xfe]);  // the third group gives dried meat
  assert.equal(readMetamorph(rom).length, METAMORPH_BYTES);
  const colosseum = readColosseum(rom);  // the opponent, $40, the prize and whether it is hidden: the striker hides its prize
  assert.equal(colosseum.length, COLOSSEUM_BYTES);
  assert.deepEqual([...colosseum.subarray(4 * 4, 4 * 4 + 4)], [125, 0x40, 207, 0]);  // the thiefknife against the wart puck
  assert.equal(colosseum[41 * 4 + 3], 0xff);
  const scenes2 = readCharAi(rom);  // the second scene is Terra's flashback: the party is hidden, and Terra acts by a script
  assert.equal(scenes2[2 * CHAR_AI_BYTES] & 0x80, 0x80);
  assert.equal(scenes2[2 * CHAR_AI_BYTES + 4 + 5 + 2], 0x6e);
  assert.equal(scenes2[10 * CHAR_AI_BYTES + 4 + 2], 0x72);  // Gau on the veldt has the script of the monster 0x172
});

test('Sketch and Control give the plain blow to the guard, and its other attacks to the others', { skip }, () => {
  const rom = loadRom();
  assert.deepEqual([...readSketch(rom).subarray(0, 2)], [0xee, 0xee]);  // the guard: battle, battle
  assert.equal(readControl(rom)[0], 0xee);  // the first control attack is the blow
  assert.equal(readControl(rom).length, MONSTER_COUNT * CONTROL_BYTES);
  assert.equal(readSketch(rom)[3], 0xee);  // the soldier: scan, then the blow
});

test('the script of the guard attacks, and its pointer is the first one', { skip }, () => {
  const rom = loadRom();
  const pointers = readAiPtrs(rom), script = readAiScript(rom);
  assert.equal(pointers.readUInt16LE(0), 0);
  assert.deepEqual([...script.subarray(0, 6)], [0xf0, 0xee, 0xee, 0xef, 0xff, 0xff]);  // attack battle, battle, special; end
  assert.equal(script[AI_SCRIPT_BYTES - 1] !== undefined, true);
  assert.equal(readAttackNames(rom).length, ATTACK_NAME_COUNT * ATTACK_NAME_BYTES);
});

test('the random battles of a map come from its rate and its group', { skip }, () => {
  const rom = loadRom();
  const rates = readSubBattleRates(rom), groups = readSubBattleGroups(rom);
  assert.equal((rates[41 >> 2] >> ((41 & 3) * 2)) & 3, 1);  // the Narshe cave of the intro has a low rate
  assert.ok(groups.some(group => group !== 0));
  const rng = readRngTable(rom);
  assert.equal(new Set(rng).size > 100, true);
});
