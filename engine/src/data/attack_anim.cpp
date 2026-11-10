#include "data/attack_anim.hpp"

#include <algorithm>
#include <stdexcept>

#include "data/pack.hpp"

namespace ff6 {

namespace {

int word(const std::vector<uint8_t>& data, size_t at) { return data[at] | (data[at + 1] << 8); }

// The 8 colors of a palette of the animations; `cycle` moves some of them up round (steps * 256 + first color * 16 + count).
void paletteColors(const std::vector<uint8_t>& pal, int palette, int cycle, uint8_t colors[8][FF6_BYTES_PER_PIXEL]) {
    const size_t first = static_cast<size_t>(palette) * FF6_ATTACK_PAL_BYTES_EACH;
    for (size_t i = 0; i < 8; ++i) {
        const size_t at = first + i * 2;
        if (at + 1 >= pal.size()) break;
        colorToRgba(static_cast<uint16_t>(pal[at] | (pal[at + 1] << 8)), colors[i]);
    }
    const int turned = cycle >> 8, firstColor = (cycle >> 4) & 0x0F, colorCount = cycle & 0x0F;
    if (turned > 0 && colorCount > 1 && firstColor + colorCount <= 8) {
        uint8_t old[8][FF6_BYTES_PER_PIXEL];
        std::copy(&colors[0][0], &colors[0][0] + 8 * FF6_BYTES_PER_PIXEL, &old[0][0]);
        for (int i = 0; i < colorCount; ++i) {
            std::copy(old[firstColor + i], old[firstColor + i] + FF6_BYTES_PER_PIXEL, colors[firstColor + (i + turned) % colorCount]);
        }
    }
}

}  // namespace

AttackAnimProp AttackAnim::prop(int attack) const {
    AttackAnimProp result;
    if (attack < 0 || attack >= FF6_ATTACK_PROP_COUNT) return result;
    const size_t at = static_cast<size_t>(attack) * FF6_ATTACK_PROP_BYTES;
    result.sprite = word(props, at);
    result.bg1 = word(props, at + 2);
    result.bg3 = word(props, at + 4);
    result.spritePal = props[at + 6];
    result.bg1Pal = props[at + 7];
    result.bg3Pal = props[at + 8];
    result.sfx = props[at + 9];
    result.init = props[at + 10];
    result.extra = word(props, at + 11);
    result.delay = props[at + 13];
    return result;
}

std::array<uint16_t, 8> AttackAnim::paletteWords(int palette) const {
    std::array<uint16_t, 8> colors{};
    const size_t first = static_cast<size_t>(std::max(palette, 0)) * FF6_ATTACK_PAL_BYTES_EACH;
    for (size_t i = 0; i < colors.size(); ++i) {
        const size_t at = first + i * 2;
        if (at + 1 < pal.size()) colors[i] = static_cast<uint16_t>(pal[at] | (pal[at + 1] << 8));
    }
    return colors;
}

WeaponAnimProp AttackAnim::weapon(int index) const {
    WeaponAnimProp result;
    if (index < 0 || index >= FF6_ATTACK_WEAPON_PROP_COUNT) return result;
    const uint8_t* at = &weapons[static_cast<size_t>(index) * FF6_ATTACK_WEAPON_PROP_BYTES];
    result.rightScript = at[0];
    result.leftScript = at[1];
    result.weaponPal = at[2];
    result.hitScript = at[3];
    result.hitPal = at[4];
    result.init = at[5] & ~FF6_ATTACK_WEAPON_HIT_SPRITE;
    result.hitIsSprite = (at[5] & FF6_ATTACK_WEAPON_HIT_SPRITE) != 0;
    result.sfx = at[6];
    return result;
}

AttackGfxProp AttackAnim::gfx(int script) const {
    AttackGfxProp result;
    if (script < 0 || script >= FF6_ATTACK_SCRIPT_COUNT) return result;
    const size_t at = static_cast<size_t>(script) * FF6_ATTACK_GFX_PROP_BYTES;
    result.frames = gfxProps[at] & FF6_ATTACK_FRAMES_MASK;
    result.twoBits = (gfxProps[at] & FF6_ATTACK_2BPP_FLAG) != 0;
    result.tileOffset = gfxProps[at + 1] | ((gfxProps[at] & FF6_ATTACK_SET_FLAG) ? 0x100 : 0);
    result.firstFrame = word(gfxProps, at + 2);
    result.width = gfxProps[at + 4];
    result.height = gfxProps[at + 5];
    return result;
}

const uint8_t* AttackAnim::script(int id, size_t* length) const {
    if (id < 0 || id >= FF6_ATTACK_SCRIPT_COUNT) return nullptr;
    const size_t at = static_cast<size_t>(word(scriptPtrs, static_cast<size_t>(id) * 2));
    if (at >= scripts.size()) return nullptr;
    if (length) *length = scripts.size() - at;
    return &scripts[at];
}

const AttackFrame& AttackAnim::frame(int script, int index, int palette, bool mirrored, int cycle, const BattleImage* esper) const {
    const long key = (((static_cast<long>(script) * 128 + index) * 2048 + palette) * 2 + (mirrored ? 1 : 0)) * 4096 + cycle;
    const auto found = cache_.find(key);
    if (found != cache_.end()) return found->second;
    AttackFrame& result = cache_[key];
    const AttackGfxProp prop = gfx(script);
    if (index < 0 || index >= prop.frames) return result;
    const int number = prop.firstFrame + index;
    if (number + 1 >= FF6_ATTACK_FRAME_PTR_COUNT) return result;
    const int begin = word(framePtrs, static_cast<size_t>(number) * 2) - FF6_ATTACK_FRAME_FIRST;
    const int next = word(framePtrs, static_cast<size_t>(number + 1) * 2) - FF6_ATTACK_FRAME_FIRST;
    const int size = static_cast<int>(frames.size());
    const int end = next > begin && next <= size ? next : size;  // a frame is followed by the next, or ends with two 0xFF
    if (begin < 0 || begin >= size) return result;

    // The sprites of the frame: their corners relative to the center, the cell of the sheet and the flips.
    struct Sprite {
        int x, y, column, row, flips;
    };
    std::vector<Sprite> sprites;
    for (int at = begin; at + 1 < end; at += 2) {
        const int position = frames[static_cast<size_t>(at)], tile = frames[static_cast<size_t>(at) + 1];
        if (position == FF6_ATTACK_FRAME_END_MARK && tile == FF6_ATTACK_FRAME_END_MARK) break;
        sprites.push_back({(position & 0xF0) - prop.width * FF6_ATTACK_CELL / 2,
                           ((position & 0x0F) << 4) - prop.height * FF6_ATTACK_CELL / 2, tile & 7, (tile >> 3) & 7,
                           tile & (FF6_ATTACK_SPRITE_HFLIP | FF6_ATTACK_SPRITE_VFLIP)});
    }
    if (sprites.empty()) return result;
    int left = sprites[0].x, top = sprites[0].y, right = left, bottom = top;
    for (const Sprite& sprite : sprites) {
        left = std::min(left, sprite.x);
        top = std::min(top, sprite.y);
        right = std::max(right, sprite.x);
        bottom = std::max(bottom, sprite.y);
    }
    right += FF6_ATTACK_CELL;
    bottom += FF6_ATTACK_CELL;

    uint8_t colors[8][FF6_BYTES_PER_PIXEL] = {};
    paletteColors(pal, palette, cycle, colors);
    // The tiles of the sheet, each with its own flips.
    const size_t table = static_cast<size_t>(prop.tileOffset & 0xFF) * FF6_ATTACK_TILE_UNITS +
                         ((prop.tileOffset & 0x100) ? FF6_ATTACK_TILE_SET_BYTES : 0);
    const std::vector<uint8_t>& tiles = prop.twoBits ? tiles2 : tiles3;
    const auto color = [&](int x, int y) {  // of the pixel (x, y) of the sheet, a palette index
        const size_t entryAt = table + static_cast<size_t>((y / 8) * FF6_ATTACK_SHEET_COLUMNS + x / 8) * 2;
        if (entryAt + 1 >= tiles.size()) return 0;
        const int entry = word(tiles, entryAt), tile = entry & FF6_ATTACK_TILE_MASK;
        const int row = (entry & FF6_ATTACK_TILE_VFLIP) ? 7 - y % 8 : y % 8;
        const int shift = (entry & FF6_ATTACK_TILE_HFLIP) ? x % 8 : 7 - x % 8;
        if (prop.twoBits) {
            const size_t at = static_cast<size_t>(tile) * 16 + static_cast<size_t>(row) * 2;
            if (at + 1 >= gfx2.size()) return 0;
            return ((gfx2[at] >> shift) & 1) | (((gfx2[at + 1] >> shift) & 1) << 1);
        }
        const size_t base = static_cast<size_t>(tile) * FF6_TILE_3BPP_BYTES;
        if (base + FF6_TILE_3BPP_BYTES > gfx3.size()) return 0;
        return ((gfx3[base + static_cast<size_t>(row) * 2] >> shift) & 1) |
               (((gfx3[base + static_cast<size_t>(row) * 2 + 1] >> shift) & 1) << 1) |
               (((gfx3[base + FF6_TILE_PLANE_STRIDE + static_cast<size_t>(row)] >> shift) & 1) << 2);
    };

    const int width = right - left, height = bottom - top;
    result.image.width = width;
    result.image.height = height;
    result.image.rgba.assign(static_cast<size_t>(width) * height * FF6_BYTES_PER_PIXEL, 0);
    for (auto sprite = sprites.rbegin(); sprite != sprites.rend(); ++sprite) {  // the first sprite is on top
        for (int y = 0; y < FF6_ATTACK_CELL; ++y) {
            for (int x = 0; x < FF6_ATTACK_CELL; ++x) {
                const int sourceX = sprite->column * FF6_ATTACK_CELL + ((sprite->flips & FF6_ATTACK_SPRITE_HFLIP) ? FF6_ATTACK_CELL - 1 - x : x);
                const int sourceY = sprite->row * FF6_ATTACK_CELL + ((sprite->flips & FF6_ATTACK_SPRITE_VFLIP) ? FF6_ATTACK_CELL - 1 - y : y);
                const uint8_t* from = nullptr;
                if (esper) {  // the sheet of an esper is its picture, tile for tile
                    if (sourceX < esper->width && sourceY < esper->height) {
                        from = &esper->rgba[(static_cast<size_t>(sourceY) * esper->width + static_cast<size_t>(sourceX)) * FF6_BYTES_PER_PIXEL];
                        if (from[FF6_ALPHA_OFFSET] == 0) from = nullptr;
                    }
                } else if (const int index = color(sourceX, sourceY)) {
                    from = colors[index];
                }
                if (!from) continue;
                const int targetX = sprite->x - left + x, targetY = sprite->y - top + y;
                std::copy(from, from + FF6_BYTES_PER_PIXEL,
                          result.image.rgba.begin() + (static_cast<std::ptrdiff_t>(targetY) * width + targetX) * FF6_BYTES_PER_PIXEL);
            }
        }
    }
    result.x = left;
    result.y = top;
    if (mirrored) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width / 2; ++x) {
                for (int c = 0; c < FF6_BYTES_PER_PIXEL; ++c) {
                    std::swap(result.image.rgba[(static_cast<size_t>(y) * width + x) * FF6_BYTES_PER_PIXEL + c],
                              result.image.rgba[(static_cast<size_t>(y) * width + width - 1 - x) * FF6_BYTES_PER_PIXEL + c]);
                }
            }
        }
        result.x = -right;
    }
    return result;
}

BattleImage AttackAnim::mode7Plane(int tileSet, int mapSet, int palette, int cycle) const {
    BattleImage plane;
    plane.width = plane.height = FF6_MODE7_SIDE;
    plane.rgba.assign(static_cast<size_t>(FF6_MODE7_SIDE) * FF6_MODE7_SIDE * FF6_BYTES_PER_PIXEL, 0);
    uint8_t colors[8][FF6_BYTES_PER_PIXEL] = {};
    paletteColors(pal, palette, cycle, colors);
    const size_t tilesAt = static_cast<size_t>(tileSet) * FF6_MODE7_SET_GFX_BYTES, mapAt = static_cast<size_t>(mapSet) * FF6_MODE7_SET_MAP_BYTES;
    if (tilesAt + FF6_MODE7_SET_GFX_BYTES > mode7Gfx.size() || mapAt + FF6_MODE7_SET_MAP_BYTES > mode7Map.size()) return plane;
    for (int row = 0; row < FF6_MODE7_SIDE / FF6_MENU_TILE_SIZE; ++row) {
        for (int column = 0; column < FF6_MODE7_TILE_COLUMNS; ++column) {
            const int tile = mode7Map[mapAt + static_cast<size_t>(row * FF6_MODE7_TILE_COLUMNS + column)];
            if (tile >= FF6_MODE7_SET_TILES) continue;
            const uint8_t* bytes = &mode7Gfx[tilesAt + static_cast<size_t>(tile) * FF6_TILE_4BPP_BYTES];
            for (int line = 0; line < FF6_MENU_TILE_SIZE; ++line) {
                for (int x = 0; x < FF6_MENU_TILE_SIZE; ++x) {
                    const int shift = FF6_MENU_TILE_SIZE - 1 - x;
                    const int index = ((bytes[line * 2] >> shift) & 1) | (((bytes[line * 2 + 1] >> shift) & 1) << 1) |
                                      (((bytes[FF6_TILE_PLANE_STRIDE + line * 2] >> shift) & 1) << 2) |
                                      (((bytes[FF6_TILE_PLANE_STRIDE + line * 2 + 1] >> shift) & 1) << 3);
                    if (index == 0) continue;
                    const size_t at = (static_cast<size_t>(row * FF6_MENU_TILE_SIZE + line) * FF6_MODE7_SIDE +
                                       static_cast<size_t>(column * FF6_MENU_TILE_SIZE + x)) * FF6_BYTES_PER_PIXEL;
                    std::copy(colors[index & 7], colors[index & 7] + FF6_BYTES_PER_PIXEL, plane.rgba.begin() + static_cast<std::ptrdiff_t>(at));
                }
            }
        }
    }
    return plane;
}

AttackAnim loadAttackAnim(const std::string& packDir) {
    const std::vector<uint8_t> file = readFile(packDir + "/" FF6_ATTACK_ANIM_FILE);
    ByteReader reader(file);
    reader.expectMagic(FF6_ATTACK_ANIM_MAGIC, "attack animations");
    AttackAnim anim;
    anim.props = reader.bytes(static_cast<size_t>(FF6_ATTACK_PROP_COUNT) * FF6_ATTACK_PROP_BYTES);
    anim.gfxProps = reader.bytes(static_cast<size_t>(FF6_ATTACK_SCRIPT_COUNT) * FF6_ATTACK_GFX_PROP_BYTES);
    anim.scriptPtrs = reader.bytes(static_cast<size_t>(FF6_ATTACK_SCRIPT_COUNT) * 2);
    anim.scripts = reader.bytes(FF6_ATTACK_SCRIPT_BYTES);
    anim.framePtrs = reader.bytes(static_cast<size_t>(FF6_ATTACK_FRAME_PTR_COUNT) * 2);
    anim.frames = reader.bytes(FF6_ATTACK_FRAME_END - FF6_ATTACK_FRAME_FIRST);
    anim.tiles3 = reader.bytes(FF6_ATTACK_TILES_3BPP_BYTES);
    anim.pal = reader.bytes(FF6_ATTACK_PAL_BYTES);
    anim.tiles2 = reader.bytes(FF6_ATTACK_TILES_2BPP_BYTES);
    anim.gfx3 = reader.bytes(FF6_ATTACK_GFX_3BPP_BYTES);
    anim.gfx2 = reader.bytes(FF6_ATTACK_GFX_2BPP_BYTES);
    anim.weapons = reader.bytes(static_cast<size_t>(FF6_ATTACK_WEAPON_PROP_COUNT) * FF6_ATTACK_WEAPON_PROP_BYTES);
    anim.mode7Gfx = reader.bytes(FF6_ATTACK_MODE7_GFX_BYTES);
    anim.mode7Map = reader.bytes(FF6_ATTACK_MODE7_MAP_BYTES);
    anim.rng = reader.bytes(FF6_ATTACK_RNG_BYTES);
    anim.rowSpeeds = reader.bytes(FF6_ATTACK_ROW_SPEED_BYTES);
    anim.lights = reader.bytes(FF6_ATTACK_LIGHTS_BYTES);
    if (reader.remaining() != 0) throw std::runtime_error("attack animations have trailing bytes");
    return anim;
}

}  // namespace ff6
