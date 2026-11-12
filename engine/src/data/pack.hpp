#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "constants.hpp"

namespace ff6 {

std::vector<uint8_t> readFile(const std::string& path);

// Bounds-checked little-endian reader; every read past the end throws std::runtime_error.
class ByteReader {
public:
    explicit ByteReader(const std::vector<uint8_t>& data) : data_(data) {}
    uint8_t u8();
    uint16_t u16();
    uint32_t u32();
    std::vector<uint8_t> bytes(size_t count);
    std::vector<uint16_t> words(size_t count);
    void readInto(uint8_t* destination, size_t count);
    void expectMagic(const char* magic, const char* what);
    size_t remaining() const { return data_.size() - pos_; }

private:
    void need(size_t count) const;
    const std::vector<uint8_t>& data_;
    size_t pos_ = 0;
};

struct DialogueText {
    const uint8_t* codes = nullptr;  // null when the dialogue does not exist
    size_t length = 0;
};

// Data shared by every map: font, window, sprites, dialogue and the event script.
struct CommonData {
    std::array<uint8_t, FF6_FONT_WIDTH_COUNT> fontWidths{};
    std::vector<uint16_t> fontRows;  // FF6_GLYPH_COUNT * FF6_GLYPH_ROWS, FF6_GLYPH_LEFT_BIT = leftmost pixel
    std::vector<uint8_t> windowTiles;  // FF6_WINDOW_TILE_COUNT RGBA tiles
    std::vector<uint16_t> spritePalettes;  // BGR555, FF6_SPRITE_PAL_COLORS per palette
    std::vector<uint8_t> spriteGfx;  // FF6_SPRITE_SHEET_BYTES of palette indices per graphic
    std::vector<uint32_t> dialogueOffsets;  // one more than the dialogue count
    std::vector<uint8_t> dialogueCodes;
    std::vector<uint8_t> npcSwitchDefaults;  // FF6_NPC_SWITCH_BYTES
    std::vector<uint8_t> eventScript;
    std::vector<uint8_t> vehicleGfx;  // FF6_VEHICLE_GFX_BYTES of 4 bits per pixel, which the game keeps from $7200 in the sprite characters
    std::vector<uint8_t> specialGfx;  // FF6_SPECIAL_GFX_BYTES of the raw 4bpp graphics of every map sprite, for the special npcs

    size_t spriteGfxCount() const { return spriteGfx.size() / FF6_SPRITE_SHEET_BYTES; }
    size_t spritePaletteCount() const { return spritePalettes.size() / FF6_SPRITE_PAL_COLORS; }
    DialogueText dialogue(uint32_t id) const;
};

CommonData loadCommonData(const std::string& packDir);

// Converts BGR555 colors to RGBA bytes; the first color of a palette is not special here.
void colorToRgba(uint16_t color, uint8_t* rgba);

}  // namespace ff6
