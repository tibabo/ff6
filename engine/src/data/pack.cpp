#include "data/pack.hpp"

#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace ff6 {

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open " + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void ByteReader::need(size_t count) const {
    if (count > data_.size() - pos_) throw std::runtime_error("data file is truncated");
}

uint8_t ByteReader::u8() {
    need(1);
    return data_[pos_++];
}

uint16_t ByteReader::u16() {
    need(2);
    const uint16_t value = static_cast<uint16_t>(data_[pos_] | (data_[pos_ + 1] << 8));
    pos_ += 2;
    return value;
}

uint32_t ByteReader::u32() {
    need(4);
    uint32_t value = 0;
    for (int i = 3; i >= 0; --i) value = (value << 8) | data_[pos_ + static_cast<size_t>(i)];
    pos_ += 4;
    return value;
}

std::vector<uint8_t> ByteReader::bytes(size_t count) {
    need(count);
    std::vector<uint8_t> out(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                             data_.begin() + static_cast<std::ptrdiff_t>(pos_ + count));
    pos_ += count;
    return out;
}

std::vector<uint16_t> ByteReader::words(size_t count) {
    need(count * 2);
    std::vector<uint16_t> out(count);
    for (size_t i = 0; i < count; ++i) out[i] = u16();
    return out;
}

void ByteReader::readInto(uint8_t* destination, size_t count) {
    need(count);
    std::memcpy(destination, data_.data() + pos_, count);
    pos_ += count;
}

void ByteReader::expectMagic(const char* magic, const char* what) {
    need(FF6_PACK_MAGIC_SIZE);
    if (std::memcmp(data_.data() + pos_, magic, FF6_PACK_MAGIC_SIZE) != 0) {
        throw std::runtime_error(std::string("not an FF6 ") + what + " file");
    }
    pos_ += FF6_PACK_MAGIC_SIZE;
    if (u32() != FF6_PACK_VERSION) throw std::runtime_error(std::string("unsupported ") + what + " file version");
}

DialogueText CommonData::dialogue(uint32_t id) const {
    if (static_cast<size_t>(id) + 1 >= dialogueOffsets.size()) return {};
    const uint32_t start = dialogueOffsets[id];
    return {dialogueCodes.data() + start, dialogueOffsets[id + 1] - start};
}

CommonData loadCommonData(const std::string& packDir) {
    const std::vector<uint8_t> data = readFile(packDir + "/" FF6_COMMON_FILE);
    ByteReader reader(data);
    reader.expectMagic(FF6_COMMON_MAGIC, "common data");
    CommonData common;
    reader.readInto(common.fontWidths.data(), common.fontWidths.size());
    common.fontRows = reader.words(static_cast<size_t>(FF6_GLYPH_COUNT) * FF6_GLYPH_ROWS);
    common.windowTiles = reader.bytes(static_cast<size_t>(FF6_WINDOW_TILE_COUNT) * FF6_WINDOW_TILE_BYTES);
    const uint32_t paletteCount = reader.u32();
    if (paletteCount > FF6_MAX_PACK_ITEMS) throw std::runtime_error("invalid sprite palette count");
    common.spritePalettes = reader.words(static_cast<size_t>(paletteCount) * FF6_SPRITE_PAL_COLORS);
    const uint32_t gfxCount = reader.u32();
    if (gfxCount > FF6_MAX_PACK_ITEMS) throw std::runtime_error("invalid sprite graphics count");
    common.spriteGfx = reader.bytes(static_cast<size_t>(gfxCount) * FF6_SPRITE_SHEET_BYTES);
    const uint32_t dialogueCount = reader.u32();
    if (dialogueCount > FF6_MAX_PACK_ITEMS) throw std::runtime_error("invalid dialogue count");
    common.dialogueOffsets.resize(static_cast<size_t>(dialogueCount) + 1);
    for (uint32_t& offset : common.dialogueOffsets) offset = reader.u32();
    for (size_t i = 1; i < common.dialogueOffsets.size(); ++i) {
        if (common.dialogueOffsets[i] < common.dialogueOffsets[i - 1]) throw std::runtime_error("invalid dialogue offsets");
    }
    common.dialogueCodes = reader.bytes(common.dialogueOffsets.back());
    common.npcSwitchDefaults = reader.bytes(FF6_NPC_SWITCH_BYTES);
    common.eventScript = reader.bytes(reader.u32());
    common.vehicleGfx = reader.bytes(FF6_VEHICLE_GFX_BYTES);
    if (reader.remaining() != 0) throw std::runtime_error("common data has trailing bytes");
    return common;
}

void colorToRgba(uint16_t color, uint8_t* rgba) {
    const int mask = (1 << FF6_COLOR_BITS) - 1;
    const auto expand = [](int value) { return static_cast<uint8_t>((value << 3) | (value >> 2)); };
    rgba[0] = expand(color & mask);
    rgba[1] = expand((color >> FF6_COLOR_BITS) & mask);
    rgba[2] = expand((color >> (2 * FF6_COLOR_BITS)) & mask);
    rgba[3] = 255;
}

}  // namespace ff6
