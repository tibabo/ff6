// src/data/monster_prop.asm, monster_items.asm, battle_group.asm and src/battle/init_2.asm: the monsters and the battles.
export const BATTLE_MAGIC = 'FF6BTL01';

export const MONSTER_COUNT = 384;
export const MONSTER_PROP_BYTES = 32;
const MONSTER_PROP = 0x0f0000;  // CF/0000
export const MONSTER_ITEM_BYTES = 4;  // rare steal, common steal, rare drop, common drop
const MONSTER_ITEMS = 0x0f3000;  // CF/3000
export const MONSTER_NAME_BYTES = 10;
const MONSTER_NAMES = 0x0fc050;  // CF/C050, in the codes of the small font

export const BATTLE_COUNT = 576;
export const BATTLE_PROP_BYTES = 4;
const BATTLE_PROP = 0x0f5900;  // CF/5900
export const BATTLE_MONSTER_BYTES = 15;  // flags (word), six monster ids, six positions, the ninth bits of the ids
const BATTLE_MONSTERS = 0x0f6200;  // CF/6200

export const RAND_GROUP_COUNT = 256;  // groups of four battles for the maps, of two for the events; each battle a word
const RAND_BATTLE_GROUP = 0x0f4800;  // CF/4800, 4 x 2 bytes each
const EVENT_BATTLE_GROUP = 0x0f5000;  // CF/5000, 2 x 2 bytes each

export const MAP_COUNT = 512;
const SUB_BATTLE_GROUP = 0x0f5600;  // CF/5600: for each map, a group of RAND_BATTLE_GROUP
export const SUB_RATE_BYTES = 128;
const SUB_BATTLE_RATE = 0x0f5880;  // CF/5880: for each map, 2 bits, from the low ones: normal, low, high, very high
export const RNG_BYTES = 256;
const RNG_TABLE = 0x00fd00;  // C0/FD00
export const AI_PTR_BYTES = MONSTER_COUNT * 2;
const AI_PTRS = 0x0f8400;  // CF/8400: where the script of each monster starts in AI_SCRIPT
export const AI_SCRIPT_BYTES = 0x3950;
const AI_SCRIPT = 0x0f8700;  // CF/8700
export const ATTACK_NAME_BYTES = 10;
export const GENJU_ATTACK_COUNT = 27;  // the attacks 54 to 80
const GENJU_ATTACK_NAMES = 0x26fe8f;  // E6/FE8F
export const ATTACK_NAME_COUNT = 175;  // the attacks 81 to 255
const ATTACK_NAMES = 0x26f7b9;  // E6/F7B9
export const SKETCH_BYTES = 2;  // the two attacks that Sketch gives of each monster
const SKETCH = 0x0f4300;  // CF/4300
export const CONTROL_BYTES = 4;  // the attacks that Control gives, the first one the plain blow
const CONTROL = 0x0f3d00;  // CF/3D00

const table = (rom, start, count, size) => Buffer.from(rom.subarray(start, start + count * size));

export const readMonsterProp = rom => table(rom, MONSTER_PROP, MONSTER_COUNT, MONSTER_PROP_BYTES);
export const readMonsterItems = rom => table(rom, MONSTER_ITEMS, MONSTER_COUNT, MONSTER_ITEM_BYTES);
export const readMonsterNames = rom => table(rom, MONSTER_NAMES, MONSTER_COUNT, MONSTER_NAME_BYTES);
export const readBattleProp = rom => table(rom, BATTLE_PROP, BATTLE_COUNT, BATTLE_PROP_BYTES);
export const readBattleMonsters = rom => table(rom, BATTLE_MONSTERS, BATTLE_COUNT, BATTLE_MONSTER_BYTES);
export const readRandBattleGroups = rom => table(rom, RAND_BATTLE_GROUP, RAND_GROUP_COUNT, 8);
export const readEventBattleGroups = rom => table(rom, EVENT_BATTLE_GROUP, RAND_GROUP_COUNT, 4);
export const readSubBattleGroups = rom => table(rom, SUB_BATTLE_GROUP, MAP_COUNT, 1);
export const readSubBattleRates = rom => table(rom, SUB_BATTLE_RATE, SUB_RATE_BYTES, 1);
export const readRngTable = rom => table(rom, RNG_TABLE, RNG_BYTES, 1);
export const readAiPtrs = rom => table(rom, AI_PTRS, MONSTER_COUNT, 2);
export const readAiScript = rom => table(rom, AI_SCRIPT, AI_SCRIPT_BYTES, 1);
export const readGenjuAttackNames = rom => table(rom, GENJU_ATTACK_NAMES, GENJU_ATTACK_COUNT, ATTACK_NAME_BYTES);
export const readAttackNames = rom => table(rom, ATTACK_NAMES, ATTACK_NAME_COUNT, ATTACK_NAME_BYTES);
export const readSketch = rom => table(rom, SKETCH, MONSTER_COUNT, SKETCH_BYTES);
export const readControl = rom => table(rom, CONTROL, MONSTER_COUNT, CONTROL_BYTES);

// `version` is the version of the whole pack.
export function buildBattleData(rom, version) {
  const header = Buffer.alloc(12);
  header.write(BATTLE_MAGIC, 0, 8, 'ascii');
  header.writeUInt32LE(version, 8);
  return Buffer.concat([header, readMonsterProp(rom), readMonsterItems(rom), readMonsterNames(rom), readBattleProp(rom),
    readBattleMonsters(rom), readRandBattleGroups(rom), readEventBattleGroups(rom), readSubBattleGroups(rom),
    readSubBattleRates(rom), readRngTable(rom), readAiPtrs(rom), readAiScript(rom), readGenjuAttackNames(rom),
    readAttackNames(rom), readSketch(rom), readControl(rom)]);
}
