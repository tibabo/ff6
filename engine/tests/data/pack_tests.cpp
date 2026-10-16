#include <cstdlib>
#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"

void testPack() {
    const std::vector<uint8_t> bytes = {1, 2, 3, 4, 5};
    ff6::ByteReader reader(bytes);
    CHECK(reader.u8() == 1);
    CHECK(reader.u16() == 0x0302);
    CHECK(test::throws([&] { reader.u32(); }));  // only two bytes are left
    CHECK(test::throws([&] { reader.bytes(3); }));

    uint8_t rgba[4];
    ff6::colorToRgba(0x7FFF, rgba);
    CHECK(rgba[0] == 255 && rgba[1] == 255 && rgba[2] == 255 && rgba[3] == 255);
    ff6::colorToRgba(0x001F, rgba);
    CHECK(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0);
    ff6::colorToRgba(0x7C00, rgba);
    CHECK(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 255);

    ff6::CommonData common = test::makeCommon();
    common.dialogueOffsets = {0, 2, 2, 5};
    common.dialogueCodes = {10, 11, 20, 21, 22};
    CHECK(common.dialogue(0).length == 2 && common.dialogue(0).codes[1] == 11);
    CHECK(common.dialogue(1).length == 0);
    CHECK(common.dialogue(2).length == 3 && common.dialogue(2).codes[0] == 20);
    CHECK(common.dialogue(3).codes == nullptr && common.dialogue(99).codes == nullptr);

    CHECK(test::throws([] { ff6::loadCommonData("missing-pack-directory"); }));

    if (const char* pack = std::getenv("FF6_PACK")) {
        const ff6::CommonData real = ff6::loadCommonData(pack);
        CHECK(real.spriteGfxCount() == 165);
        CHECK(real.dialogueOffsets.size() == 3085);
        CHECK(real.npcSwitchDefaults.size() == FF6_NPC_SWITCH_BYTES);
        CHECK(real.dialogue(604).length > 1);
        CHECK(!real.eventScript.empty());
    }
}
