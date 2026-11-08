#include <cstdlib>

#include "../check.hpp"
#include "../tests.hpp"
#include "data/attack_anim.hpp"

// With the real pack: the properties of fire, its script and the frames of its sheet.
void testAttackAnim() {
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::AttackAnim anim = ff6::loadAttackAnim(pack);

    const ff6::AttackAnimProp fire = anim.prop(0);
    CHECK(fire.sprite == 0xC1 && fire.bg1 == FF6_ATTACK_NO_SCRIPT && fire.spritePal == 56 && fire.init == 37 && fire.delay == 16);
    CHECK(anim.prop(-1).sprite == FF6_ATTACK_NO_SCRIPT && anim.prop(FF6_ATTACK_PROP_COUNT).sprite == FF6_ATTACK_NO_SCRIPT);

    size_t length = 0;
    const uint8_t* script = anim.script(fire.sprite, &length);
    CHECK(script && length > 8 && script[0] == 0x40 && script[1] == 0x00 && script[4] == 0xEB);  // speed 5, bottom, jump per thread
    CHECK(anim.script(-1) == nullptr && anim.script(FF6_ATTACK_SCRIPT_COUNT) == nullptr);

    // The sheet of fire: frames that are drawn around their center, and mirrored on the other side of it.
    const ff6::AttackGfxProp gfx = anim.gfx(fire.sprite);
    CHECK(gfx.frames > 0 && !gfx.twoBits && gfx.width > 0 && gfx.height > 0);
    int lit = 0;
    for (int index = 0; index < gfx.frames; ++index) {
        const ff6::AttackFrame& frame = anim.frame(fire.sprite, index, fire.spritePal, false);
        CHECK(frame.image.rgba.size() == static_cast<size_t>(frame.image.width) * frame.image.height * FF6_BYTES_PER_PIXEL);
        for (size_t i = FF6_ALPHA_OFFSET; i < frame.image.rgba.size(); i += FF6_BYTES_PER_PIXEL) lit += frame.image.rgba[i] != 0;
        const ff6::AttackFrame& flipped = anim.frame(fire.sprite, index, fire.spritePal, true);
        CHECK(flipped.image.width == frame.image.width && flipped.x == -(frame.x + frame.image.width));
    }
    CHECK(lit > 100);
    CHECK(&anim.frame(fire.sprite, 0, fire.spritePal, false) == &anim.frame(fire.sprite, 0, fire.spritePal, false));  // decoded once
    CHECK(anim.frame(fire.sprite, gfx.frames, fire.spritePal, false).image.rgba.empty());  // past the last one
    CHECK(anim.frame(-1, 0, 0, false).image.rgba.empty());
}
