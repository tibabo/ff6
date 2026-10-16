#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"

void testMap() {
    ff6::MapData map = test::makeMap(16, 32);
    CHECK(map.propAt(3, 4) == 0x0F01);
    CHECK(map.propAt(3 + 16, 4 + 32) == map.propAt(3, 4));  // the map wraps around
    CHECK(map.propAt(-1, -1) == map.propAt(15, 31));
    test::setProp(map, 2, 2, 0xFFF7);
    CHECK(map.propAt(2, 2) == 0xFFF7);
    map.prop[FF6_MAP_PROP_ANIMATION] = 3 | (2 << FF6_BG3_ANIM_SHIFT);
    CHECK(map.bgAnimEnabled() && map.bg3AnimIndex() == 2);
    map.prop[FF6_MAP_PROP_FLAGS] = FF6_BG3_FOREGROUND_FLAG;
    CHECK(map.bg3Foreground());
    map.prop[FF6_MAP_PROP_BG2_OFFSET_X] = 5;
    map.prop[FF6_MAP_PROP_BG3_OFFSET_Y] = 7;
    CHECK(map.layerOffsetX(FF6_BG2) == 5 && map.layerOffsetY(FF6_BG3) == 7);

    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "ff6-map-test";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory / FF6_MAP_DIR);
    std::ofstream(directory / FF6_MAP_DIR / "003" FF6_MAP_FILE_EXTENSION, std::ios::binary) << "not a map";
    CHECK(ff6::mapExists(directory.string(), 3));
    CHECK(!ff6::mapExists(directory.string(), 4));
    CHECK(test::throws([&] { ff6::loadMap(directory.string(), 3); }));
    CHECK(test::throws([&] { ff6::loadMap(directory.string(), 4); }));
    std::filesystem::remove_all(directory);

    if (const char* pack = std::getenv("FF6_PACK")) {
        const ff6::MapData inn = ff6::loadMap(pack, 0x1C);
        CHECK(inn.id == 0x1C && inn.startX == 8 && inn.startY == 45);
        CHECK(inn.npcProps.size() == FF6_NPC_PROP_SIZE);
        CHECK(inn.eventTriggers.size() == FF6_EVENT_TRIGGER_SIZE);
        CHECK(inn.layers[FF6_BG1].cells.size() == static_cast<size_t>(inn.layers[FF6_BG1].width) * inn.layers[FF6_BG1].height);
        CHECK(inn.parallax[4] == FF6_PARALLAX_ONE);  // no parallax: every layer follows the camera
        const ff6::MapData narshe = ff6::loadMap(pack, 20);
        CHECK(narshe.bg3AnimIndex() != 0 && !narshe.bg3Anim.data.empty());
        CHECK(narshe.colorMath[0] == 0x04 && narshe.colorMath[1] == 0x17 && narshe.colorMath[2] == 0x13);  // bg3 is added
    }
}
