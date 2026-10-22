#include "game.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

#include "field/bg.hpp"
#include "field/color.hpp"
#include "field/sprite_data.hpp"

namespace ff6 {
namespace {

const Rect kScreen{0, 0, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT};

void blendPixel(std::vector<uint8_t>& image, int x, int y, const uint8_t* rgba) {
    if (x < 0 || y < 0 || x >= FF6_SCREEN_WIDTH || y >= FF6_SCREEN_HEIGHT || rgba[FF6_ALPHA_OFFSET] == 0) return;
    std::copy(rgba, rgba + FF6_BYTES_PER_PIXEL,
              image.begin() + (static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL);
}

int windowTile(int col, int row) {
    const bool top = row == 0, bottom = row == FF6_WINDOW_ROWS - 1;
    const bool left = col == 0, right = col == FF6_WINDOW_COLS - 1;
    const bool oddColumn = (col & 1) != 0, oddRow = (row & 1) != 0;
    if (top) {
        return left ? FF6_WINDOW_TILE_TOP_LEFT : right ? FF6_WINDOW_TILE_TOP_RIGHT
               : oddColumn ? FF6_WINDOW_TILE_TOP_ODD : FF6_WINDOW_TILE_TOP_EVEN;
    }
    if (bottom) {
        return left ? FF6_WINDOW_TILE_BOTTOM_LEFT : right ? FF6_WINDOW_TILE_BOTTOM_RIGHT
               : oddColumn ? FF6_WINDOW_TILE_BOTTOM_ODD : FF6_WINDOW_TILE_BOTTOM_EVEN;
    }
    if (left) return oddRow ? FF6_WINDOW_TILE_LEFT_ODD : FF6_WINDOW_TILE_LEFT_EVEN;
    if (right) return oddRow ? FF6_WINDOW_TILE_RIGHT_ODD : FF6_WINDOW_TILE_RIGHT_EVEN;
    return ((row - 1) % FF6_WINDOW_FILL_SIZE) * FF6_WINDOW_FILL_SIZE + (col - 1) % FF6_WINDOW_FILL_SIZE;
}

struct Drawable {
    int sortY;
    int strip;  // index into Game::strips_
    int frame;
    int screenX, screenY;
    bool topAbove, bottomAbove;
};

}  // namespace

Game::Game(CommonData common, std::string packDir, uint32_t startMap, Renderer& renderer, Sound& sound)
    : common_(std::move(common)), packDir_(std::move(packDir)), renderer_(renderer), sound_(sound), event_(common_, bits_) {
    bits_.init(common_);
    const size_t screenBytes = static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL;
    for (auto& layer : layerPixels_) {
        for (auto& pixels : layer) pixels.assign(screenBytes, 0);
    }
    for (auto& pixels : spritePixels_) pixels.assign(screenBytes, 0);
    screenPixels_.assign(screenBytes, 0);
    overlayPixels_.assign(screenBytes, 0);
    screen_ = renderer_.createTexture(FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT, nullptr, TextureUsage::Dynamic);
    overlay_ = renderer_.createTexture(FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT, nullptr, TextureUsage::Dynamic);
    objects_.resetCharacters();
    if (!loadMapNow(startMap, -1, -1, Down, true)) throw std::runtime_error("the start map is not in the data pack");
}

int Game::stripFor(int gfx, int palette) {
    const std::pair<int, int> key(gfx, palette);
    const auto found = stripIndex_.find(key);
    if (found != stripIndex_.end()) return found->second;
    strips_.push_back(buildSpriteStrip(common_, gfx, palette));
    stripIndex_[key] = static_cast<int>(strips_.size() - 1);
    return stripIndex_[key];
}

int Game::stripOf(Npc& object) {
    if (object.strip < 0) {
        const size_t slot = static_cast<size_t>(object.prop.palette) & (FF6_SPRITE_PALETTE_SLOTS - 1);
        object.strip = stripFor(object.prop.gfx, spritePalette_[slot]);
    }
    return object.strip;
}

void Game::invalidateStrips() {
    for (Npc& npc : objects_.npcs()) npc.strip = -1;
    for (Npc& character : objects_.characters()) character.strip = -1;
}

// The lead object takes the place of the player; the player moves it while the party is under the user's control.
void Game::copyPlayerToLead(Npc& lead) const {
    lead.posX = player_.pixelX() * FF6_OBJ_SUBPIXELS;
    lead.posY = player_.pixelY() * FF6_OBJ_SUBPIXELS;
    lead.facing = player_.facing();
    lead.frame = player_.frameIndex();
    lead.topAbove = player_.topAbove();
    lead.bottomAbove = player_.bottomAbove();
}

// Where the screen is: on the party, or on the camera object once an event has locked it.
void Game::followScroll() {
    const Npc& camera = objects_.camera();
    if (screenLocked_) {
        scroll_.update(map_, camera.pixelX(), camera.pixelY());
    } else {
        scroll_.update(map_, player_.pixelX(), player_.pixelY());
    }
}

// Starting an event takes the party out of the user's hands until it is over, when the lead gets its movement back.
void Game::startEvent(uint32_t offset) {
    if (Npc* lead = objects_.lead()) {
        if (!event_.running() && !restorePending_) leadSavedMovement_ = lead->movement;
        lead->movement = FF6_OBJ_MOVE_ACTIVATED;
    }
    restorePending_ = false;
    eventWasRunning_ = true;
    event_.start(offset);
}

bool Game::loadMapNow(uint32_t mapId, int x, int y, int facing, bool startupEvent) {
    if (mapId < FF6_FIRST_FIELD_MAP || !mapExists(packDir_, mapId)) {
        std::fprintf(stderr, "map %u is not available in the data pack\n", static_cast<unsigned>(mapId));
        return false;
    }
    MapData next = loadMap(packDir_, mapId);
    const bool inEvent = event_.running();  // an event that loads a map goes on once it is loaded
    map_ = std::move(next);
    anim_.init();
    scroll_.init();
    objects_.init(map_, bits_);
    if (x < 0) {
        x = map_.startX;
        y = map_.startY;
        facing = map_.startFacing;
    }
    player_.place(map_, x, y, facing);
    objects_.enterMap(static_cast<int>(mapId), x, y, facing);
    if (Npc* lead = objects_.lead()) copyPlayerToLead(*lead);
    Npc& camera = objects_.camera();
    camera = Npc();
    camera.enabled = true;
    camera.posX = player_.pixelX() * FF6_OBJ_SUBPIXELS;
    camera.posY = player_.pixelY() * FF6_OBJ_SUBPIXELS;
    screenLocked_ = false;
    if (!inEvent) {
        leadSavedMovement_ = FF6_OBJ_MOVE_USER;
        restorePending_ = false;
        eventWasRunning_ = false;
    } else if (Npc* lead = objects_.lead()) {
        lead->movement = FF6_OBJ_MOVE_ACTIVATED;
    }
    lastTileX_ = x;
    lastTileY_ = y;
    armed_ = false;
    dialogue_.open({}, common_.fontWidths, false);
    if (!inEvent) event_.stop();
    followScroll();
    // The startup event, which sets the music among other things, runs with the party out of the user's hands; in an
    // event, it runs before the rest of that event.
    const std::vector<uint8_t>& script = common_.eventScript;
    if (startupEvent && map_.startupEvent < script.size() && script[map_.startupEvent] != FF6_EVENT_RETURN) {
        if (inEvent) {
            event_.call(map_.startupEvent);
        } else {
            startEvent(map_.startupEvent);
        }
    }
    return true;
}

void Game::openDialogue(uint32_t id, uint16_t flags) {
    dialogue_.open(common_.dialogue(id), common_.fontWidths, (flags & FF6_DLG_FLAG_BOTTOM) != 0);
}

void Game::changeMap(uint32_t map, int x, int y, int facing, bool startupEvent) {
    loadMapNow(map, x, y, facing, startupEvent);
}

// Characters, npcs, the camera and the party's places all run scripts, if the object is there to run it.
bool Game::startObjectScript(int object, uint32_t offset) {
    Npc* npc = objects_.object(object);
    if (!npc || !objects_.acts(*npc)) {
        if (warnedObjects_.insert(object).second) {
            std::fprintf(stderr, "event: object 0x%02X is not there, its script is skipped\n", object);
        }
        return false;
    }
    objects_.startScript(*npc, offset);
    return true;
}

// The event waits for the script of an object, not for the movement that the object does by itself.
bool Game::objectBusy(int object) const {
    const Npc* npc = objects_.object(object);
    return npc && npc->movement == FF6_OBJ_MOVE_SCRIPT && objects_.acts(*npc);
}

void Game::objectCommand(uint8_t op, int object, uint32_t arg) {
    Npc* npc = objects_.object(object);
    if (!npc) return;
    objects_.command(op, *npc, arg);
    if (op == FF6_EVENT_OBJ_GFX || op == FF6_EVENT_OBJ_PAL) npc->strip = -1;  // the sprite is built again when drawn
}

void Game::fieldCommand(uint8_t op, const uint8_t* bytes, size_t size) {
    const auto byteAt = [&](size_t n) { return n < size ? static_cast<int>(bytes[n]) : 0; };
    switch (op) {
        case FF6_EVENT_LOCK_SCREEN:
            screenLocked_ = true;
            break;
        case FF6_EVENT_UNLOCK_SCREEN:
            screenLocked_ = false;
            break;
        case FF6_EVENT_PARTY_USER:  // what the lead does once the event is over
            leadSavedMovement_ = FF6_OBJ_MOVE_USER;
            break;
        case FF6_EVENT_PARTY_SCRIPT:
            leadSavedMovement_ = FF6_OBJ_MOVE_NONE;
            break;
        case FF6_EVENT_SET_PARTY: {
            std::array<int, FF6_PARTY_SIZE> slots;
            for (size_t i = 0; i < slots.size(); ++i) slots[i] = byteAt(i) < FF6_CHAR_COUNT ? byteAt(i) : -1;
            objects_.setPartySlots(slots);
            break;
        }
        case FF6_EVENT_ADD_CHAR:
            objects_.addToParty(byteAt(0), byteAt(1) & (FF6_PARTY_COUNT - 1));
            break;
        case FF6_EVENT_ACTIVATE_PARTY:
            objects_.activateParty(byteAt(0) & (FF6_PARTY_COUNT - 1));
            break;
        case FF6_EVENT_UPDATE_PARTY:
            objects_.updateParty();
            break;
        case FF6_EVENT_SET_PALETTE:
            if (static_cast<size_t>(byteAt(1)) < common_.spritePaletteCount()) {
                spritePalette_[static_cast<size_t>(byteAt(0)) & (FF6_SPRITE_PALETTE_SLOTS - 1)] = byteAt(1);
                invalidateStrips();
            }
            break;
        case FF6_EVENT_PARTY_MAP:
            objects_.setPartyMap(byteAt(0) & (FF6_PARTY_COUNT - 1),
                                 (byteAt(1) | (byteAt(2) << 8)) & FF6_ENTRANCE_MAP_MASK);
            break;
        case FF6_EVENT_MOVE_PARTY:
            placeParty(byteAt(0), byteAt(1));
            break;
        case FF6_EVENT_MAP_DATA_NOW:
        case FF6_EVENT_MAP_DATA:  // x, layer and y, width, height, then the tiles
            changeMapData(bytes, size);
            break;
        default:
            break;
    }
}

// Event commands 0x73 and 0x74: tiles written into a layer of the map (an opened door, a moved block).
void Game::changeMapData(const uint8_t* bytes, size_t size) {
    if (size < FF6_MAP_DATA_HEADER_BYTES) return;
    const int x = bytes[0], y = bytes[1] & FF6_MAP_DATA_Y_MASK, width = bytes[2], height = bytes[3];
    const int layerBits = bytes[1] >> FF6_MAP_DATA_LAYER_SHIFT;  // 0 is bg1, 1 is bg2, 2 and 3 are bg3
    if (size < FF6_MAP_DATA_HEADER_BYTES + static_cast<size_t>(width) * height) return;
    MapLayout& layout = map_.layers[static_cast<size_t>(layerBits == 0 ? FF6_BG1 : layerBits == 1 ? FF6_BG2 : FF6_BG3)];
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            if (x + column >= layout.width || y + row >= layout.height) continue;
            layout.cells[static_cast<size_t>(y + row) * layout.width + x + column] =
                bytes[FF6_MAP_DATA_HEADER_BYTES + static_cast<size_t>(row) * width + column];
        }
    }
}

void Game::leaveThroughEntrance(const EntranceTarget& target) {
    if (target.toParent) {
        if (!hasParent_) return;
        const int facing = (parentFacing_ + 2) % FF6_DIRECTION_COUNT;  // step back out of the door
        if (loadMapNow(parentMap_, parentX_ + kDeltaX[facing], parentY_ + kDeltaY[facing], facing, true)) hasParent_ = false;
        return;
    }
    const bool hadParent = hasParent_;
    const uint32_t previousMap = parentMap_;
    const int previousX = parentX_, previousY = parentY_, previousFacing = parentFacing_;
    if (target.setParent) {
        hasParent_ = true;
        parentMap_ = map_.id;
        parentX_ = player_.tileX();
        parentY_ = player_.tileY();
        parentFacing_ = player_.facing();
    }
    if (!loadMapNow(target.map, target.x, target.y, target.facing, true) && target.setParent) {
        hasParent_ = hadParent;
        parentMap_ = previousMap;
        parentX_ = previousX;
        parentY_ = previousY;
        parentFacing_ = previousFacing;
    }
}

// Port of CheckEventTriggers (once per arrival on a tile) and CheckEntrances (every frame once armed, so that an
// entrance still fires after an event that did not leave the map).
void Game::checkArrival() {
    const int x = player_.tileX(), y = player_.tileY();
    if (x != lastTileX_ || y != lastTileY_) {
        lastTileX_ = x;
        lastTileY_ = y;
        armed_ = true;
        for (size_t i = 0; i + FF6_EVENT_TRIGGER_SIZE <= map_.eventTriggers.size(); i += FF6_EVENT_TRIGGER_SIZE) {
            const EventTrigger trigger = decodeEventTrigger(&map_.eventTriggers[i]);
            if (trigger.x == x && trigger.y == y) {
                startEvent(trigger.offset);
                return;
            }
        }
    }
    EntranceTarget target;
    if (armed_ && checkEntrances(map_, x, y, &target)) leaveThroughEntrance(target);
}

// Port of CheckNPCs: the confirm button talks to the npc that the party faces.
void Game::talkToNpc() {
    Npc* npc = objects_.npcFacing(map_, player_.tileX(), player_.tileY(), player_.facing());
    if (!npc) return;
    objects_.activate(*npc, player_.facing());
    startEvent(npc->eventOffset);
}

// Port of CheckDoor: pressing up or down in front of a closed door opens it (the party does not move that frame) and
// the door stays open. A small door is one tile wide and two tall, the big one three wide.
bool Game::openDoor(int direction) {
    struct Door {
        uint8_t closed;
        int width, offsetX;
        std::array<std::array<uint8_t, 3>, 2> open;
    };
    static constexpr Door kDoors[] = {{FF6_DOOR_SMALL_1, 1, 0, {{{0x04}, {0x14}}}},
                                      {FF6_DOOR_SMALL_2, 1, 0, {{{0x06}, {0x16}}}},
                                      {FF6_DOOR_BIG, 3, -1, {{{0x08, 0x09, 0x0A}, {0x18, 0x19, 0x1A}}}}};
    if ((direction != Up && direction != Down) || player_.moving()) return false;
    player_.face(direction);
    MapLayout& layout = map_.layers[FF6_BG1];
    const int x = player_.tileX(), y = player_.tileY();
    const int doorY = direction == Up ? y - 2 : y;
    const int frontY = direction == Up ? y - 1 : y + 1;
    if (x < 0 || x >= layout.width || frontY < 0 || frontY >= layout.height || doorY < 0) return false;
    const uint8_t tile = layout.cells[static_cast<size_t>(frontY) * layout.width + x];
    for (const Door& door : kDoors) {
        if (tile != door.closed) continue;
        const uint8_t props = map_.tileProps[tile];
        if (props == FF6_TILE_DOOR_SOLID || (props & FF6_TILE_DOOR) == 0) return false;
        const int left = x + door.offsetX;
        if (left < 0 || left + door.width > layout.width || doorY + 1 >= layout.height) return false;
        if (objects_.occupied(x, doorY + 1)) return false;
        for (int row = 0; row < 2; ++row) {
            for (int column = 0; column < door.width; ++column) {
                layout.cells[static_cast<size_t>(doorY + row) * layout.width + left + column] =
                    door.open[static_cast<size_t>(row)][static_cast<size_t>(column)];
            }
        }
        sound_.command(FF6_SOUND_GAME_SFX, FF6_SFX_DOOR_OPEN, FF6_SFX_PAN_CENTER);
        return true;
    }
    return false;
}

void Game::tick(const InputState& input) {
    ++frame_;
    anim_.update(map_, frame_);
    // The press that closes a dialogue or ends an event must not also start a new conversation.
    const bool wasBusy = dialogue_.active() || event_.running();
    if (dialogue_.active()) dialogue_.update(input.confirm, input.direction);
    event_.update(*this);
    Npc* lead = objects_.lead();
    const bool eventRunning = event_.running();
    if (eventWasRunning_ && !eventRunning) restorePending_ = true;
    eventWasRunning_ = eventRunning;
    // Once the event is over and the lead has finished its own script, it gets its movement back.
    if (restorePending_ && lead && lead->movement != FF6_OBJ_MOVE_SCRIPT && lead->aligned() && !player_.moving()) {
        lead->movement = leadSavedMovement_;
        restorePending_ = false;
        player_.place(map_, lead->tileX(), lead->tileY(), lead->facing);  // settles the priority of the tile
    }
    const bool userLead = !lead || lead->movement == FF6_OBJ_MOVE_USER;
    const bool playerActs = (!eventRunning && !dialogue_.active() && userLead) || player_.moving();
    if (!eventRunning && !dialogue_.active() && userLead) {
        if (input.confirm && !wasBusy && !player_.moving()) {
            talkToNpc();
        } else if (!openDoor(input.direction)) {
            player_.update(map_, input.direction, objects_);
        }
        if (!player_.moving()) checkArrival();
    } else if (player_.moving()) {
        player_.update(map_, FF6_NO_DIRECTION, objects_);  // finish the step before the event takes over
    }
    // The player moves the lead while it is under the user's control, the lead moves the player otherwise.
    if (lead && playerActs) copyPlayerToLead(*lead);
    Npc& camera = objects_.camera();
    if (!screenLocked_) {
        camera.posX = player_.pixelX() * FF6_OBJ_SUBPIXELS;
        camera.posY = player_.pixelY() * FF6_OBJ_SUBPIXELS;
    }
    // The objects act whatever the party does, also during a dialogue.
    objects_.update(map_, common_.eventScript, bits_, frame_, event_.running());
    uint32_t requested = 0;
    if (objects_.takeEventRequest(&requested)) startEvent(requested);
    lead = objects_.lead();
    if (lead && lead->movement != FF6_OBJ_MOVE_USER && !player_.moving()) {
        player_.follow(lead->pixelX(), lead->pixelY(), lead->facing);
    }
    followScroll();
}

void Game::placeParty(int tileX, int tileY) {
    player_.place(map_, tileX, tileY, player_.facing());
    objects_.placeParty(tileX, tileY);
    if (Npc* lead = objects_.lead()) copyPlayerToLead(*lead);
    lastTileX_ = tileX;
    lastTileY_ = tileY;
    armed_ = false;
    followScroll();
}

void Game::composeDialogue() {
    std::vector<uint8_t>& image = overlayPixels_;
    std::fill(image.begin(), image.end(), 0);
    const int windowY = dialogue_.atBottom() ? FF6_WINDOW_BOTTOM_Y : FF6_WINDOW_Y;
    for (int row = 0; row < FF6_WINDOW_ROWS; ++row) {
        for (int col = 0; col < FF6_WINDOW_COLS; ++col) {
            const uint8_t* tile = &common_.windowTiles[static_cast<size_t>(windowTile(col, row)) * FF6_WINDOW_TILE_BYTES];
            for (int y = 0; y < FF6_WINDOW_TILE_SIZE; ++y) {
                for (int x = 0; x < FF6_WINDOW_TILE_SIZE; ++x) {
                    blendPixel(image, FF6_WINDOW_X + col * FF6_WINDOW_TILE_SIZE + x,
                               windowY + row * FF6_WINDOW_TILE_SIZE + y,
                               tile + (y * FF6_WINDOW_TILE_SIZE + x) * FF6_BYTES_PER_PIXEL);
                }
            }
        }
    }
    const int originX = FF6_TEXT_ORIGIN_X;
    const int originY = windowY + (FF6_TEXT_ORIGIN_Y - FF6_WINDOW_Y);
    const uint8_t shadow[FF6_BYTES_PER_PIXEL] = FF6_TEXT_SHADOW_RGBA;
    const uint8_t ink[FF6_BYTES_PER_PIXEL] = FF6_TEXT_INK_RGBA;
    for (const uint8_t* color : {shadow, ink}) {
        const int offset = color == shadow ? FF6_TEXT_SHADOW_OFFSET : 0;
        for (const PlacedGlyph& glyph : dialogue_.glyphs()) {
            const size_t base = static_cast<size_t>(glyph.code - FF6_FIRST_GLYPH_CODE) * FF6_GLYPH_ROWS;
            for (int row = 0; row < FF6_GLYPH_ROWS; ++row) {
                const uint16_t bits = common_.fontRows[base + static_cast<size_t>(row)];
                for (int col = 0; col < FF6_GLYPH_ROW_BITS; ++col) {
                    if (bits & (FF6_GLYPH_LEFT_BIT >> col)) {
                        blendPixel(image, originX + glyph.x + col + offset,
                                   originY + glyph.line * FF6_TEXT_LINE_HEIGHT + FF6_TEXT_BASELINE_OFFSET + row + offset,
                                   color);
                    }
                }
            }
        }
        if (dialogue_.choosing()) {  // a right-pointing triangle on the selected choice
            const ChoiceMark& mark = dialogue_.choiceMarks()[static_cast<size_t>(dialogue_.cursor())];
            for (int row = 0; row < FF6_CURSOR_SIZE; ++row) {
                const int width = row <= FF6_CURSOR_SIZE / 2 ? row + 1 : FF6_CURSOR_SIZE - row;
                for (int col = 0; col < width; ++col) {
                    blendPixel(image, originX + mark.x + FF6_CURSOR_X_OFFSET + col + offset,
                               originY + mark.line * FF6_TEXT_LINE_HEIGHT + FF6_CURSOR_Y_OFFSET + row + offset, color);
                }
            }
        }
    }
}

// Paints the sprites that are drawn below (above = false) or above the high priority layers.
void Game::drawSprites(bool above) {
    std::vector<Drawable> drawables;
    const auto add = [&](Npc& object) {
        const int layer = object.prop.scrollBg2 ? FF6_BG2 : FF6_BG1;
        drawables.push_back({object.pixelY(), stripOf(object), object.frame, object.pixelX() - scroll_.x(layer),
                             object.pixelY() - FF6_SPRITE_Y_OFFSET - object.jumpHeight - scroll_.y(layer),
                             object.topIsAbove(), object.bottomIsAbove()});
    };
    for (Npc& character : objects_.characters()) {
        if (character.blocks() && character.map == objects_.mapId()) add(character);
    }
    for (Npc& npc : objects_.npcs()) {
        if (npc.blocks()) add(npc);  // hidden, deleted and special npcs are not drawn
    }
    std::stable_sort(drawables.begin(), drawables.end(),
                     [](const Drawable& a, const Drawable& b) { return a.sortY < b.sortY; });
    uint8_t* screen = spritePixels_[above ? 1 : 0].data();
    for (const Drawable& sprite : drawables) {
        const std::vector<uint8_t>& strip = strips_[static_cast<size_t>(sprite.strip)];
        if (sprite.topAbove == above) {
            drawSpriteRows(screen, strip, sprite.frame, 0, FF6_SPRITE_TOP_HEIGHT, sprite.screenX, sprite.screenY);
        }
        if (sprite.bottomAbove == above) {
            drawSpriteRows(screen, strip, sprite.frame, FF6_SPRITE_TOP_HEIGHT, FF6_SPRITE_BOTTOM_HEIGHT, sprite.screenX,
                           sprite.screenY + FF6_SPRITE_TOP_HEIGHT);
        }
    }
}

void Game::draw() {
    for (auto& layer : layerPixels_) {
        for (auto& pixels : layer) std::fill(pixels.begin(), pixels.end(), 0);
    }
    for (auto& pixels : spritePixels_) std::fill(pixels.begin(), pixels.end(), 0);
    for (int layer = 0; layer < FF6_LAYER_COUNT; ++layer) {
        auto& pixels = layerPixels_[static_cast<size_t>(layer)];
        renderBg(map_, layer, scroll_.x(layer), scroll_.y(layer), pixels[0].data(), pixels[1].data());
    }
    drawSprites(false);
    drawSprites(true);
    // Back to front, like the priorities of the PPU in mode 1.
    const ScreenLayer layers[] = {
        {FF6_SCREEN_BG3, layerPixels_[FF6_BG3][0].data()}, {FF6_SCREEN_BG2, layerPixels_[FF6_BG2][0].data()},
        {FF6_SCREEN_BG1, layerPixels_[FF6_BG1][0].data()}, {FF6_SCREEN_OBJ, spritePixels_[0].data()},
        {FF6_SCREEN_BG2, layerPixels_[FF6_BG2][1].data()}, {FF6_SCREEN_BG1, layerPixels_[FF6_BG1][1].data()},
        {FF6_SCREEN_OBJ, spritePixels_[1].data()},         {FF6_SCREEN_BG3, layerPixels_[FF6_BG3][1].data()}};
    uint8_t backdrop[FF6_BYTES_PER_PIXEL];
    colorToRgba(map_.palette[0], backdrop);
    composeScreen(decodeColorMath(map_.colorMath), backdrop, layers, sizeof(layers) / sizeof(layers[0]),
                  screenPixels_.data());
    renderer_.beginFrame();
    renderer_.updateTexture(screen_, screenPixels_.data());
    renderer_.draw(screen_, kScreen, kScreen);
    if (dialogue_.active()) {
        composeDialogue();
        renderer_.updateTexture(overlay_, overlayPixels_.data());
        renderer_.draw(overlay_, kScreen, kScreen);
    }
    renderer_.endFrame();
}

}  // namespace ff6
