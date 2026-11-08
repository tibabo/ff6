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
    if (reader.remaining() != 0) throw std::runtime_error("title data has trailing bytes");
    return data;
}

}  // namespace ff6
