#include <cstdlib>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

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

    {
        // The mosaic, the brightness, the vertical flip and the priorities that scripts set are noted for the screen to draw.
        ff6::AnimBody ally;
        ally.slot = 0, ally.centerX = 200, ally.centerY = 100, ally.topY = 88, ally.bottomY = 112;
        bool mosaic = false, dim = false, flipped = false, front = false, back = false;
        for (int attack = 0; attack < FF6_ATTACK_PROP_COUNT; ++attack) {
            player.start(attack, attacker, {attack == 95 ? ally : monster(0, 70, 84)}, false, 0);
            for (int frame = 0; frame < 900 && player.tick(); ++frame) {
                mosaic = mosaic || (player.mosaic() >> 4) > 0;
                dim = dim || player.dimmed() > 0;
                flipped = flipped || player.shift(false, 0).flipY;
                for (int slot = 0; slot < 4; ++slot) front = front || player.shift(false, slot).priority == FF6_ANIM_PRIORITY_FRONT;
                for (const ff6::AnimSprite& sprite : player.sprites()) back = back || sprite.back;
            }
        }
        CHECK(mosaic && dim && flipped && front && back);
    }
    if (std::getenv("FF6_SKIP_PROBE")) {  // set it to list the commands that no animation gets past, with some of the attacks
        std::map<int, std::vector<int>> found;
        for (int attack = 0; attack < FF6_ATTACK_PROP_COUNT; ++attack) {
            player.start(attack, attacker, {monster(0, 70, 84)}, false);
            for (int frame = 0; frame < 900 && player.tick(); ++frame) {}
            for (const auto& entry : player.skipped()) found[entry.first].push_back(attack);
        }
        for (const auto& entry : found) {
            std::printf("skipped %03X in %zu attacks:", entry.first, entry.second.size());
            for (size_t i = 0; i < entry.second.size() && i < 24; ++i) std::printf(" %d", entry.second[i]);
            std::printf("\n");
        }
    }
    {
        // No command is skipped by any of the 406 animations (a code over 0xFF is a command of $80).
        std::set<int> skipped;
        for (int attack = 0; attack < FF6_ATTACK_PROP_COUNT; ++attack) {
            player.start(attack, attacker, {monster(0, 70, 84)}, false);
            for (int frame = 0; frame < 900 && player.tick(); ++frame) {}
            for (const auto& entry : player.skipped()) skipped.insert(entry.first);
        }
        CHECK(skipped.empty());
    }
    {
        // The pearl scrolls bg3 line by line, the characters run along their line, and the echoes of a character are asked for.
        const auto seen = [&](std::initializer_list<int> attacks, const auto& test) {
            bool found = false;
            for (const int attack : attacks) {
                player.start(attack, attacker, {monster(0, 70, 84)}, false);
                for (int frame = 0; frame < 600 && player.tick(); ++frame) found = found || test();
            }
            return found;
        };
        CHECK(seen({14, 147}, [&] { return player.lineScroll(2).active; }));
        CHECK(seen({388, 389}, [&] { return player.shift(false, 0).angle != 0; }));
        CHECK(seen({85, 241}, [&] { return player.echo().on; }));
        CHECK(seen({196}, [&] { return player.lineScroll(1).active; }));
    }
    {
        // A die shows the face that it was given.
        player.setDice(3, 5, 1);
        player.start(380, attacker, {monster(0, 70, 84)}, false);
        bool ticked = false;
        for (int frame = 0; frame < 300 && player.tick(); ++frame) ticked = true;
        CHECK(ticked && player.skipped().empty());
    }
    {
        // The triangle is the shape of window 2 for some attacks, as the circle is for others.
        bool inside = false, outside = false;
        for (const int attack : {12, 161, 175, 191, 210, 287}) {
            player.start(attack, attacker, {monster(0, 70, 84)}, false);
            for (int frame = 0; frame < 300 && player.tick(); ++frame) {
                for (int y = 0; y < 160; y += 8) {
                    for (int x = 8; x < 248; x += 8) {
                        for (const int layer : {1, 2}) {
                            if (!player.layerWindowed(layer)) continue;
                            (player.layerVisible(layer, x, y) ? inside : outside) = true;
                        }
                    }
                }
            }
        }
        CHECK(inside && outside);
        // The jumps along a vector carry the attacker to its target and back: sonic dive style attacks move the character.
        bool jumped = false;
        for (const int attack : {63, 95, 243, 301}) {
            player.start(attack, attacker, {monster(0, 70, 84)}, false);
            for (int frame = 0; frame < 300 && player.tick(); ++frame) {
                jumped = jumped || player.shift(false, 0).vx != 0 || player.shift(false, 0).vy != 0;
            }
        }
        CHECK(jumped);
    }
    {
        // The game's tables: the lights of Chadarnook load a palette of the field, the tornado lays its sine in the scroll data of
        // bg1, and r.polarity moves a character to the other row by the steps of the table (2 by 6 for the front row of a normal
        // battle going back).
        ff6::AnimBody ally;
        ally.slot = 0;
        ally.centerX = 200;
        ally.centerY = 100;
        ally.topY = 88;
        ally.bottomY = 112;
        bool lights = false, tornado = false, row = false, noise = false;
        for (int attack = 0; attack < FF6_ATTACK_PROP_COUNT; ++attack) {
            for (const bool character : {false, true}) {
                player.start(attack, attacker, {character ? ally : monster(0, 70, 84)}, false, 0);
                for (int frame = 0; frame < 600 && player.tick(); ++frame) {
                    lights = lights || player.lightsPalette() != nullptr;
                    tornado = tornado || (player.lineScroll(1).active && player.lineScroll(1).h[64 + 16] != 0 && player.lineScroll(1).h[3] != 0);
                    row = row || player.shift(false, 0).rowAngle == 12;
                    noise = noise || (player.lineScroll(1).active && player.lineScroll(1).h[150] != 0);
                }
            }
        }
        CHECK(lights && tornado && row && noise);
    }

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

    // A beam of the magitek armor: the armor walks up to the monster first, and the glow and the flare wait until it fires.
    ff6::AnimBody armor = attacker;
    armor.slot = 0;
    player.setMagitek(true);
    player.start(131, armor, {monster(0, 70, 84)}, false);
    int firstGlow = -1, frame = 0;
    for (; player.tick() && firstGlow < 0; ++frame) {
        for (const ff6::AnimSprite& sprite : player.sprites()) {
            if (sprite.layer != 1) firstGlow = frame;
        }
    }
    CHECK(firstGlow > 10);
    CHECK(player.shift(false, 0).armor == 2 && player.shift(false, 0).x < -40);
    // The init function of the beams copies the thread of the flare twice, 8 frames apart: the flares overlap.
    size_t flares = 0;
    while (player.tick()) {
        size_t now = 0;
        for (const ff6::AnimSprite& sprite : player.sprites()) now += sprite.layer == 0;
        flares = std::max(flares, now);
    }
    CHECK(flares >= 2);
    player.setMagitek(false);
}
