#include "field/event_cmd.hpp"

#include <array>

#include "constants.hpp"

namespace ff6 {
namespace {

struct Range {
    uint8_t first, last, length;
};

// Fixed command sizes: the number that each command passes to IncEventPtrContinue or IncEventPtrReturn.
constexpr Range kRanges[] = {
    {0x35, 0x36, 2}, {0x37, 0x37, 3}, {0x38, 0x3B, 1}, {0x3C, 0x3C, 5}, {0x3D, 0x3E, 2}, {0x3F, 0x40, 3},
    {0x41, 0x42, 2}, {0x43, 0x44, 3}, {0x45, 0x45, 1}, {0x46, 0x46, 2}, {0x47, 0x47, 1}, {0x48, 0x48, 3},
    {0x49, 0x4A, 1}, {0x4B, 0x4D, 3}, {0x4E, 0x4F, 1}, {0x50, 0x50, 2}, {0x51, 0x51, 4}, {0x52, 0x52, 2},
    {0x53, 0x53, 4}, {0x54, 0x54, 1}, {0x55, 0x5A, 2}, {0x5B, 0x5C, 1}, {0x5D, 0x60, 3}, {0x61, 0x61, 4},
    {0x62, 0x63, 2}, {0x64, 0x65, 3}, {0x6A, 0x6C, 6}, {0x70, 0x72, 3}, {0x75, 0x75, 1}, {0x77, 0x78, 2},
    {0x79, 0x79, 4}, {0x7A, 0x7A, 5}, {0x7B, 0x7B, 1}, {0x7C, 0x7D, 2}, {0x7E, 0x7F, 3}, {0x80, 0x81, 2},
    {0x82, 0x82, 1}, {0x84, 0x85, 3}, {0x86, 0x87, 2}, {0x88, 0x8A, 4}, {0x8B, 0x8C, 3}, {0x8D, 0x8D, 2},
    {0x8E, 0x97, 1}, {0x98, 0x98, 2}, {0x99, 0x99, 4}, {0x9A, 0x9A, 1}, {0x9B, 0x9C, 2}, {0x9D, 0x9D, 1},
    {0xA0, 0xA0, 6}, {0xA1, 0xA1, 2}, {0xA2, 0xA2, 1}, {0xA6, 0xA6, 1}, {0xA7, 0xA7, 2}, {0xA8, 0xAF, 1},
    {0xB0, 0xB0, 2}, {0xB1, 0xB1, 1}, {0xB2, 0xB2, 4}, {0xB3, 0xB3, 5}, {0xB4, 0xB5, 2}, {0xB7, 0xB7, 5},
    {0xB8, 0xBA, 2}, {0xBB, 0xBB, 1}, {0xBC, 0xBC, 3}, {0xBD, 0xBD, 4}, {0xBF, 0xBF, 1}, {0xD0, 0xDD, 2},
    {0xDE, 0xE4, 1}, {0xE7, 0xE7, 2}, {0xE8, 0xEB, 4}, {0xEF, 0xEF, 3}, {0xF0, 0xF0, 2}, {0xF1, 0xF1, 3},
    {0xF2, 0xF4, 2}, {0xF5, 0xF6, 4}, {0xF7, 0xF8, 1}, {0xF9, 0xF9, 2}, {0xFA, 0xFB, 1}, {0xFD, 0xFE, 1}};

const std::array<uint8_t, 256>& fixedLengths() {
    static const std::array<uint8_t, 256> table = [] {
        std::array<uint8_t, 256> lengths{};
        for (const Range& range : kRanges) {
            for (int op = range.first; op <= range.last; ++op) lengths[static_cast<size_t>(op)] = range.length;
        }
        return lengths;
    }();
    return table;
}

}  // namespace

size_t eventCommandLength(const std::vector<uint8_t>& script, size_t at) {
    if (at >= script.size()) return 0;
    const uint8_t op = script[at];
    size_t length = 0;
    const auto have = [&](size_t count) { return at + count <= script.size(); };
    if (op <= FF6_EVENT_MAX_OBJ_OP) {  // object script: id, length | flags, bytes
        if (!have(2)) return 0;
        length = 2 + (script[at + 1] & FF6_EVENT_OBJ_LENGTH_MASK);
    } else if (op == FF6_EVENT_MAP_DATA_NOW || op == FF6_EVENT_MAP_DATA) {  // x, y, bbyyyyyy, width, height, tiles
        if (!have(5)) return 0;
        length = 5 + static_cast<size_t>(script[at + 3]) * script[at + 4];
    } else if (op == FF6_EVENT_CASE) {  // count, then a bit and an address for each case
        if (!have(2)) return 0;
        length = 2 + static_cast<size_t>(script[at + 1]) * 3;
    } else if (op >= FF6_EVENT_IF_ANY && op <= FF6_EVENT_IF_LAST) {  // up to 8 switches, then an address
        length = 1 + static_cast<size_t>((op & 7) + 1) * 2 + FF6_EVENT_ADDRESS_SIZE;
    } else {
        length = fixedLengths()[op];
    }
    return length != 0 && have(length) ? length : 0;
}

}  // namespace ff6
