#include "../check.hpp"
#include "../tests.hpp"
#include "field/color.hpp"
#include "field/stage.hpp"

namespace {

uint16_t rgb(int r, int g, int b) { return static_cast<uint16_t>(r | (g << 5) | (b << 10)); }

}  // namespace

void testStage() {
    // The color functions: red raised to 8 one step at a time, lowered back to the colors that the palette had, restored.
    uint16_t base[FF6_MAP_PALETTE_COLORS] = {}, active[FF6_MAP_PALETTE_COLORS] = {};
    base[5] = active[5] = rgb(3, 10, 20);
    const int raiseRed = (1 << 5) | 0x10 | 1;  // mmmrgbii: raise, red, intensity 8
    for (int i = 0; i < 4; ++i) ff6::modifyColors(raiseRed, active, base, 4, 8);
    CHECK(active[5] == rgb(7, 10, 20) && active[4] == rgb(4, 0, 0));
    for (int i = 0; i < 9; ++i) ff6::modifyColors(raiseRed, active, base, 4, 8);
    CHECK(active[5] == rgb(8, 10, 20) && active[6] == rgb(8, 0, 0));  // it stops at the target
    const int lowerBack = (3 << 5);
    for (int i = 0; i < 9; ++i) ff6::modifyColors(lowerBack, active, base, 4, 8);
    CHECK(active[5] == base[5] && active[6] == base[6]);
    ff6::modifyColors(raiseRed, active, base, 0, 128);
    ff6::modifyColors(7 << 5, active, base, 0, 128);
    CHECK(active[5] == base[5] && active[0] == 0);
    ff6::modifyColors(2 << 5, active, base, 5, 6);  // add: white
    CHECK(active[5] == rgb(31, 31, 31));

    // The fixed color fades in to its intensity, a second command fades it out, and a flash starts from full.
    ff6::FieldStage stage;
    stage.fixedColor(false, 0x20 | 0x07);  // red, the slowest speed, the full intensity
    CHECK(stage.fixedActive());
    int color[3];
    int most = 0;
    for (int frame = 0; frame < 400; ++frame) {
        stage.update(frame);
        stage.fixedRgb(color);
        most = color[0] > most ? color[0] : most;
        CHECK(color[1] == 0 && color[2] == 0);
    }
    CHECK(most == 31 * 8 && stage.fixedActive());
    stage.fixedColor(false, 0x20 | 0x07);
    for (int frame = 0; frame < 400; ++frame) stage.update(frame);
    CHECK(!stage.fixedActive());
    stage.flash(0xE0);
    stage.update(0);
    stage.fixedRgb(color);
    CHECK(stage.fixedActive() && color[0] >= 29 * 8 && color[0] == color[1] && color[1] == color[2]);
    for (int frame = 0; frame < 100; ++frame) stage.update(frame);
    CHECK(!stage.fixedActive());

    // The mosaic grows and goes away; the flashlight opens by two pixels a frame and closes at zero; the shake is vertical.
    stage.mosaic(3);
    CHECK(stage.mosaicSize() == 2);
    int biggest = 0;
    for (int frame = 0; frame < 200; ++frame) {
        stage.update(frame);
        biggest = stage.mosaicSize() > biggest ? stage.mosaicSize() : biggest;
    }
    CHECK(biggest == 16 && stage.mosaicSize() == 1);
    stage.flashlight(10);
    for (int frame = 0; frame < 3; ++frame) stage.update(frame);
    CHECK(stage.flashlightRadius() == 6);
    for (int frame = 0; frame < 30; ++frame) stage.update(frame);
    CHECK(stage.flashlightRadius() == 20);
    stage.flashlight(0);
    for (int frame = 0; frame < 30; ++frame) stage.update(frame);
    CHECK(stage.flashlightRadius() == -1);
    stage.shake(0x10 | 0x80 | 3);  // bg1 and the objects, the strongest, once
    stage.update(0);
    CHECK(stage.shakeOffset(0) == 12 && stage.shakeOffset(3) == 12 && stage.shakeOffset(1) == 0);
    stage.update(1);
    CHECK(stage.shakeOffset(0) == 0);

    // A fixed color is added to a layer of bg where the sub screen is empty.
    ff6::ColorMath math;
    math.designation = FF6_SCREEN_BG1;
    math.fixed[0] = 100;
    std::vector<uint8_t> layer(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL, 0);
    layer[0] = 50;
    layer[1] = 60;
    layer[2] = 70;
    layer[3] = 255;
    const uint8_t backdrop[FF6_COLOR_CHANNELS] = {0, 0, 0};
    std::vector<uint8_t> out(layer.size());
    const ff6::ScreenLayer layers[] = {{FF6_SCREEN_BG1, layer.data()}};
    ff6::composeScreen(math, backdrop, layers, 1, out.data());
    CHECK(out[0] == 150 && out[1] == 60 && out[2] == 70);
}
