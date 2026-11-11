#include "data/title_data.hpp"

#include <stdexcept>

#include "data/pack.hpp"

namespace ff6 {

const uint8_t* TitleData::at(uint32_t address) const {
    if (address < FF6_TITLE_PROGRAM_BASE || address - FF6_TITLE_PROGRAM_BASE >= program.size()) {
        throw std::runtime_error("cutscene address outside of the program");
    }
    return &program[address - FF6_TITLE_PROGRAM_BASE];
}

uint8_t TitleData::byteAt(uint32_t address) const { return *at(address); }

uint16_t TitleData::wordAt(uint32_t address) const {
    const uint8_t* bytes = at(address);
    at(address + 1);
    return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

TitleData loadTitleData(const std::string& packDir) {
    const std::vector<uint8_t> file = readFile(packDir + "/" FF6_TITLE_FILE);
    ByteReader reader(file);
    reader.expectMagic(FF6_TITLE_MAGIC, "title data");
    TitleData data;
    data.program = reader.bytes(FF6_TITLE_PROGRAM_BYTES);
    data.gfx = reader.bytes(FF6_TITLE_GFX_BYTES);
    for (auto& gfx : data.mapGfx) gfx = reader.bytes(FF6_TITLE_MAP_GFX_BYTES);
    data.random = reader.bytes(FF6_TITLE_TABLE_BYTES);
    data.sine = reader.bytes(FF6_TITLE_TABLE_BYTES);
    data.ruinGfx = reader.bytes(FF6_TITLE_RUIN_GFX_BYTES);
    data.floatingGfx = reader.bytes(FF6_TITLE_FLOATING_GFX_BYTES);
    data.theEndGfx = reader.bytes(FF6_TITLE_THE_END_GFX_BYTES);
    data.theEndStars = reader.bytes(FF6_TITLE_THE_END_STARS_BYTES);
    data.theEndPal = reader.bytes(FF6_TITLE_THE_END_PAL_BYTES);
    data.credits = reader.bytes(FF6_TITLE_CREDITS_BYTES);
    data.flightGfx = reader.bytes(FF6_TITLE_FLIGHT_GFX_BYTES);
    data.flightTables = reader.bytes(FF6_TITLE_FLIGHT_TABLES_BYTES);
    data.flightPalettes = reader.bytes(FF6_TITLE_FLIGHT_PALETTES_BYTES);
    data.flightScrolls = reader.bytes(FF6_TITLE_FLIGHT_SCROLLS_BYTES);
    data.sine16 = reader.bytes(FF6_TITLE_SINE_16_BYTES);
    data.skyGfx = reader.bytes(FF6_TITLE_SKY_GFX_BYTES);
    data.skyMap = reader.bytes(FF6_TITLE_SKY_MAP_BYTES);
    data.skyPal = reader.bytes(FF6_TITLE_SKY_PAL_BYTES);
    data.endingAnim = reader.bytes(FF6_TITLE_ENDING_ANIM_BYTES);
    data.endingFont = reader.bytes(FF6_TITLE_ENDING_FONT_BYTES);
    data.endingGfx1 = reader.bytes(FF6_TITLE_ENDING_GFX_1_BYTES);
    data.endingAnim2 = reader.bytes(FF6_TITLE_ENDING_ANIM_2_BYTES);
    data.endingGfx2 = reader.bytes(FF6_TITLE_ENDING_GFX_2_BYTES);
    data.endingGfx3 = reader.bytes(FF6_TITLE_ENDING_GFX_3_BYTES);
    data.endingGfx4 = reader.bytes(FF6_TITLE_ENDING_GFX_4_BYTES);
    data.endingGfx5 = reader.bytes(FF6_TITLE_ENDING_GFX_5_BYTES);
    data.endingCoin = reader.bytes(FF6_TITLE_ENDING_COIN_BYTES);
    data.endingPhase = reader.bytes(FF6_TITLE_ENDING_PHASE_BYTES);
    data.endingAirshipPal = reader.bytes(FF6_TITLE_ENDING_AIRSHIP_PAL_BYTES);
    data.falconFrames = reader.bytes(FF6_TITLE_FALCON_FRAMES_BYTES);
    data.creditsSmallFont = reader.bytes(FF6_TITLE_CREDITS_SMALL_FONT_BYTES);
    if (reader.remaining() != 0) throw std::runtime_error("title data has trailing bytes");
    return data;
}

}  // namespace ff6
