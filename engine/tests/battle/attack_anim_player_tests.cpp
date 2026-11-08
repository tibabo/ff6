#include <cstdlib>

#include "../check.hpp"
#include "../tests.hpp"
#include "battle/attack_anim_player.hpp"

namespace {

ff6::AnimBody monster(int slot, int centerX, int bottomY) {
    ff6::AnimBody body;
    body.monster = true;
    body.slot = slot;
    body.width = 48;
    body.centerX = centerX;
    body.bottomY = bottomY;
    body.topY = bottomY - 48;
    body.centerY = bottomY - 24;
    return body;
}

}  // namespace

// With the real pack: fire is three flames, one after the other, around the bottom of each target.
void testAttackAnimPlayer() {
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::AttackAnim anim = ff6::loadAttackAnim(pack);
    ff6::AttackAnimPlayer player(anim);
    ff6::AnimBody attacker;
    attacker.centerX = 200;
    attacker.centerY = 100;
    attacker.topY = 88;
    attacker.bottomY = 112;

    // The attacks with a circle (the Wave Cannon, the Meteor, Ultima, a blob) show a layer of background only inside it.
    for (const int circle : {183, 19, 20, 18}) {
        player.start(circle, attacker, {monster(0, 70, 84)}, false);
        bool inside = false, outside = false;
        for (int frame = 0; frame < 90 && player.tick(); ++frame) {
            for (int y = 0; y < 160; y += 16) {
                for (int x = 8; x < 248; x += 16) {
                    for (const int layer : {1, 2}) {
                        if (!player.layerWindowed(layer)) continue;
                        (player.layerVisible(layer, x, y) ? inside : outside) = true;
                    }
                }
            }
        }
        CHECK(inside && outside);
    }
    // The summons and the attacks that fill the screen switch to mode 7, with a plane and a matrix that the scripts move.
    struct Mode7Case {
        int attack;
        ff6::AnimPlane::Kind kind;
    };
    for (const Mode7Case& entry : {Mode7Case{65, ff6::AnimPlane::kEsper}, Mode7Case{66, ff6::AnimPlane::kEsper},
                                   Mode7Case{69, ff6::AnimPlane::kEsper}, Mode7Case{128, ff6::AnimPlane::kTiles},
                                   Mode7Case{204, ff6::AnimPlane::kTiles}, Mode7Case{220, ff6::AnimPlane::kTiles}}) {
        player.start(entry.attack, attacker, {monster(0, 70, 84)}, false);
        CHECK(player.plane().kind == entry.kind && !player.mode7().on);
        bool on = false, moved = false, shown = false;
        for (int frame = 0; frame < 900 && player.tick(); ++frame) {
            on = on || player.mode7().on;
            moved = moved || player.mode7().a != 0x100 || player.mode7().b != 0;
            shown = shown || (player.mode7().on && player.bg1Shown());
        }
        CHECK(on && moved && shown);
    }
    {
        // Odin hides the party, then puts a blue gradient on the backdrop and the field with the fixed color.
        player.start(65, attacker, {monster(0, 70, 84)}, false);
        bool hidden = false, gradient = false;
        for (int frame = 0; frame < 900 && player.tick(); ++frame) {
            hidden = hidden || player.shift(false, 0).hidden;
            for (int line = 0; line < 151; ++line) gradient = gradient || ((player.fixedMath() & 0x22) && player.fixedColor(line).b > 0);
        }
        CHECK(hidden && gradient);
    }
    {
        // Overcast turns: the matrix is a rotation. The plane of H-Bomb holds pixels.
        player.start(220, attacker, {monster(0, 70, 84)}, false);
        bool turned = false;
        for (int frame = 0; frame < 900 && player.tick(); ++frame) turned = turned || (player.mode7().b != 0 && player.mode7().c == -player.mode7().b);
        CHECK(turned);
        const ff6::BattleImage bomb = anim.mode7Plane(2, 2, 184);
        bool solid = false;
        for (size_t at = FF6_ALPHA_OFFSET; at < bomb.rgba.size(); at += FF6_BYTES_PER_PIXEL) solid = solid || bomb.rgba[at] != 0;
        CHECK(bomb.width == FF6_MODE7_SIDE && solid);
    }
    // The palette commands tint the groups of colors, and the move commands shift the target, in some of the attacks.
    bool tinted = false, shifted = false;
    for (int attack = 0; attack < FF6_ATTACK_PROP_COUNT; ++attack) {
        player.start(attack, attacker, {monster(0, 70, 84)}, false);
        for (int frame = 0; frame < 600 && player.tick(); ++frame) {
            for (int group = 0; group < ff6::kTintGroups; ++group) tinted = tinted || player.tint(group).active;
            shifted = shifted || player.shift(true, 0).x != 0 || player.shift(true, 0).y != 0;
        }
    }
    CHECK(tinted && shifted);

    player.start(-1, attacker, {monster(0, 70, 84)}, false);
    CHECK(player.done() && !player.tick());  // no attack, no animation

    player.start(0, attacker, {monster(0, 70, 84)}, false);
    CHECK(!player.done() && player.sprite() == 0xC1 && player.palette() == 56);
    CHECK(player.tick());
    // The first flame moves back 26 pixels (to the right of a monster hit from the right), aligned with the bottom.
    CHECK(player.sprites().size() == 1);
    const ff6::AnimSprite first = player.sprites()[0];
    CHECK(first.script == 0xC1 && first.frame == 0 && first.layer == 0 && !first.flipped);
    CHECK(first.x == 70 + 26 && first.y == 84 + 8 - 24);
    CHECK(player.takeSounds().size() == 1 && player.takeSounds().empty());
    // Eight frames later the second flame (the copy) starts at the center.
    for (int frame = 0; frame < 8; ++frame) CHECK(player.tick());
    CHECK(player.sprites().size() == 2);
    CHECK(player.sprites()[1].x == 70);
    int frames = 9;
    while (player.tick()) ++frames;
    CHECK(frames > 30 && frames < 90);
    CHECK(player.done() && player.sprites().empty());

    // Facing the other way the flames go the other way, and are mirrored.
    player.start(0, attacker, {monster(0, 70, 84)}, true);
    CHECK(player.tick());
    CHECK(player.sprites()[0].x == 70 - 26 && player.sprites()[0].flipped);

    // The next target starts a while later: the delay of the attack (16 frames).
    player.start(0, attacker, {monster(0, 70, 84), monster(1, 70, 140)}, false);
    int count = 0;
    for (int frame = 0; frame < 16; ++frame) {
        player.tick();
        count = static_cast<int>(player.sprites().size());
    }
    CHECK(count >= 1);
    CHECK(player.tick());
    bool second = false;
    for (const ff6::AnimSprite& sprite : player.sprites()) second = second || sprite.y > 100;
    CHECK(second);

    // The layer of the background: ice has a column of its own over the target, as a frame of its first script.
    player.start(1, attacker, {monster(0, 70, 84)}, false);
    bool layered = false;
    while (player.tick()) {
        for (const ff6::AnimSprite& sprite : player.sprites()) layered = layered || sprite.layer != 0;
    }
    CHECK(layered);
}
