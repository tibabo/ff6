#include <vector>

#include "../check.hpp"
#include "../tests.hpp"
#include "field/event_cmd.hpp"

void testEventCmd() {
    const auto length = [](std::vector<uint8_t> script) { return ff6::eventCommandLength(script, 0); };
    // Fixed sizes, from the command macros of event.asm.
    CHECK(length({0x4B, 0, 0}) == 3 && length({0x92}) == 1 && length({0xB4, 30}) == 2 && length({0x37, 1, 2}) == 3);
    CHECK(length({0x7A, 1, 2, 3, 4}) == 5 && length({0x79, 1, 2, 3}) == 4 && length({0xA0, 1, 2, 3, 4, 5}) == 6);
    CHECK(length({0x44, 1, 2}) == 3 && length({0x6A, 1, 2, 3, 4, 5}) == 6 && length({0xB3, 1, 2, 3, 4}) == 5);
    // An object script is two bytes and its body; the top bit of the length byte is the wait flag, not a length.
    CHECK(length({0x10, 0x03, 0xFF, 0xFF, 0xFF}) == 5 && length({0x10, 0x83, 0xFF, 0xFF, 0xFF}) == 5);
    CHECK(length({0x10, 0x00}) == 2);
    // Conditions: an opcode, 2 bytes for each switch and an address.
    CHECK(length({0xC0, 0, 0, 0, 0, 0}) == 6 && length({0xC1, 0, 0, 0, 0, 0, 0, 0}) == 8);
    CHECK(length({0xCF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}) == 20);
    // Map data changes carry their tiles; the case command carries its cases.
    CHECK(length({0x73, 0, 0, 2, 3, 1, 2, 3, 4, 5, 6}) == 11 && length({0x74, 0, 0, 0, 0}) == 5);
    CHECK(length({0xBE, 2, 0, 0, 0, 0, 0, 0}) == 8);
    // The size of a choice depends on the dialogue, unused commands are unknown, and cut scripts have no size.
    CHECK(length({0xB6, 0, 0, 0}) == 0 && length({0x66}) == 0 && length({0xFF}) == 0);
    CHECK(length({0x4B, 0}) == 0 && length({0x10}) == 0 && length({0xC1, 0, 0}) == 0 && length({0x73, 0, 0, 2, 3, 1}) == 0);
    CHECK(ff6::eventCommandLength({0xFE}, 5) == 0 && ff6::eventCommandLength({0x4B, 0, 0, 0xFE}, 3) == 1);
}
