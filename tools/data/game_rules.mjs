// src/field/char_prop.asm, src/item/item_prop.asm and src/data/level_up.asm: what the characters and the items are.
export const RULES_MAGIC = 'FF6GAME1';

export const CHAR_PROP_COUNT = 48;  // the characters, then the ones of the scenes: Wedge, Vicks, the Kefkas...
export const CHAR_PROP_BYTES = 22;
const CHAR_PROP = 0x2d7ca0;  // ED/7CA0

export const ITEM_PROP_COUNT = 256;
export const ITEM_PROP_BYTES = 30;
const ITEM_PROP = 0x185000;  // D8/5000

export const LEVEL_COUNT = 98;  // the steps from level 1 to 99
const LEVEL_UP_EXP = 0x2d8220;  // ED/8220, words
const LEVEL_UP_HP = 0x26f4a0;  // E6/F4A0
const LEVEL_UP_MP = 0x26f502;  // E6/F502

export const SHOP_COUNT = 128;
export const SHOP_PROP_BYTES = 9;  // the kind of shop and its price modifier, then 8 items (0xFF for none)
const SHOP_PROP = 0x047ac0;  // C4/7AC0

export function readShopProp(rom) {
  return Buffer.from(rom.subarray(SHOP_PROP, SHOP_PROP + SHOP_COUNT * SHOP_PROP_BYTES));
}

export const NATURAL_MAGIC_BYTES = 64;  // Terra and Celes: sixteen pairs of a spell and the level that teaches it
export const TECH_LEVEL_COUNT = 8;
const NATURAL_MAGIC = 0x2ce3c0;  // EC/E3C0
const BUSHIDO_LEVELS = 0x26f490;  // E6/F490, then the levels of the blitzes

export function readAbilityLevels(rom) {
  return Buffer.concat([
    rom.subarray(NATURAL_MAGIC, NATURAL_MAGIC + NATURAL_MAGIC_BYTES),
    rom.subarray(BUSHIDO_LEVELS, BUSHIDO_LEVELS + TECH_LEVEL_COUNT * 2),
  ]);
}

export const MAGIC_PROP_COUNT = 256;  // the attacks: the first 54 are the spells
export const MAGIC_PROP_BYTES = 14;  // targeting, element, three bytes of flags, mp cost, power, flags, hit rate, effect, statuses
const MAGIC_PROP = 0x046ac0;  // C4/6AC0

export function readMagicProp(rom) {
  return Buffer.from(rom.subarray(MAGIC_PROP, MAGIC_PROP + MAGIC_PROP_COUNT * MAGIC_PROP_BYTES));
}

export const GENJU_COUNT = 27;
export const GENJU_PROP_BYTES = 11;  // five pairs of a learn rate and a spell, then the bonus at each level
const GENJU_PROP = 0x186e00;  // D8/6E00

export function readGenjuProp(rom) {
  return Buffer.from(rom.subarray(GENJU_PROP, GENJU_PROP + GENJU_COUNT * GENJU_PROP_BYTES));
}

export function readCharProp(rom) {
  return Buffer.from(rom.subarray(CHAR_PROP, CHAR_PROP + CHAR_PROP_COUNT * CHAR_PROP_BYTES));
}

export function readItemProp(rom) {
  return Buffer.from(rom.subarray(ITEM_PROP, ITEM_PROP + ITEM_PROP_COUNT * ITEM_PROP_BYTES));
}

export function readLevelUp(rom) {
  return Buffer.concat([
    rom.subarray(LEVEL_UP_EXP, LEVEL_UP_EXP + LEVEL_COUNT * 2),
    rom.subarray(LEVEL_UP_HP, LEVEL_UP_HP + LEVEL_COUNT),
    rom.subarray(LEVEL_UP_MP, LEVEL_UP_MP + LEVEL_COUNT),
  ]);
}

// `version` is the version of the whole pack.
export function buildRulesData(rom, version) {
  const header = Buffer.alloc(12);
  header.write(RULES_MAGIC, 0, 8, 'ascii');
  header.writeUInt32LE(version, 8);
  return Buffer.concat([header, readCharProp(rom), readItemProp(rom), readLevelUp(rom), readShopProp(rom), readAbilityLevels(rom), readMagicProp(rom), readGenjuProp(rom)]);
}
