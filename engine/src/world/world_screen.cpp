#include "world/world_screen.hpp"

#include <algorithm>
#include <cstdio>

#include "field/map.hpp"
#include "field/sprite_data.hpp"

namespace ff6 {
namespace {

constexpr int kTilesPerWindow = 64;  // the video memory holds 64 x 64 tiles of the map
constexpr int kVramMinimap = 0x7000;
constexpr int kVramMisc = 0x7400;
constexpr int kVramAnimGfx = 0x6000;  // the sprites that move by themselves, then the airship
constexpr int kVramAirshipGfx = 0x6400;
constexpr int kVramCharacter = 0x6F00;  // where the six tiles of the character go
constexpr int kVramWaterA = 0x1100, kVramWaterB = 0x1500;  // the animated water, 2 characters each
constexpr int kMinimapFirstSprite = 4;
constexpr int kCharacterFirstSprite = 20;
constexpr int kMinimapX = 0xAF, kMinimapY = 0x9B;
constexpr int kZoomTable[FF6_WORLD_ZOOM_STEPS] = {
    0x0F85, 0x0F70, 0x0F5C, 0x0F3A, 0x0F2D, 0x0F23, 0x0F19, 0x0F18, 0x0F1B, 0x0F29, 0x0F32,
    0x0F3D, 0x0F53, 0x0F5C, 0x0F64, 0x0F6D, 0x0F6D, 0x0F6C, 0x0F69, 0x0F68, 0x0F65, 0x0F5F,
    0x0F5C, 0x0F58, 0x0D4E, 0x0C49, 0x0B44, 0x0938, 0x0831, 0x072B, 0x051D, 0x0416, 0x030F, 0x0100};
constexpr int kWaterRows = 16;
constexpr uint8_t kWaterStart[kWaterRows] = {0x80, 0x00, 0x40, 0x10, 0xA0, 0x60, 0x30, 0xB0,
                                             0xF0, 0x20, 0x70, 0xC0, 0xD0, 0x50, 0x90, 0xE0};

uint16_t word(const std::vector<uint8_t>& bytes, size_t at) {
    return static_cast<uint16_t>(bytes[at] | (bytes[at + 1] << 8));
}

}  // namespace

WorldScreen::WorldScreen(const WorldData& data, const CommonData& common, Sound& sound, WorldHooks hooks)
    : data_(data), common_(common), sound_(sound), hooks_(std::move(hooks)) {}

uint16_t WorldScreen::propAt(int x, int y) const { return map().tileProps[static_cast<size_t>(tileAt(x, y))]; }

void WorldScreen::enter(int world, int tileX, int tileY, int facing, const Lead& lead) {
    world_ = world == FF6_TRENCH_MAP ? world : (world & 1);
    lead_ = lead;
    dfx_ = static_cast<uint16_t>(tileX << 8);
    dfy_ = static_cast<uint16_t>(tileY << 8);
    facing_ = facing & 3;
    speedX_ = speedY_ = 0;
    walkSpeed_ = 0x10;
    forest_ = stepStarted_ = battleChecked_ = menuRequested_ = menuLocked_ = eventRunning_ = inCommand_ = hidden_ = false;
    entranceReady_ = battleReady_ = false;
    zoom_ = -1;
    zoomHigh_ = FF6_WORLD_SCALE_TOP >> 8;
    sectorX_ = tileX;
    sectorY_ = tileY;
    phase_ = phaseCounter_ = 0;
    mode_ = Mode::Walk;
    vehicle_ = Vehicle::Airship;
    pivot_ = 0xD0;
    pivotX_ = 0x80;
    vehicleHidden_ = arrowsShown_ = arrowsInput_ = scriptBattleReady_ = false;
    deck_ = keepFlight_ = doomGazeReady_ = storyScript_ = resumeValid_ = false;
    autoHeld_ = 0;
    brightness_ = targetBrightness_ = 15;
    windowX_ = windowY_ = -1;
    paletteCounter_ = 0;
    drawWorldChar();
    applyModifications();
    loadGraphics();
    buildWindow();
    setCharacterSprites();
    playMusic();
}

void WorldScreen::setLead(const Lead& lead) {
    lead_ = lead;
    sectorX_ = tileX();  // the game loads the map again after the menu, which moves the sector of the battles
    sectorY_ = tileY();
    setCharacterSprites();
}

void WorldScreen::playMusic() {
    if (hooks_.bit && hooks_.bit(FF6_EVENT_BIT_KEEP_SONG)) return;
    int song = world_ == 0 ? FF6_SONG_TERRA : FF6_SONG_DARK_WORLD;
    if (hooks_.bit && hooks_.bit(FF6_EVENT_BIT_VELDT)) {
        song = FF6_SONG_VELDT;
    } else if (hooks_.bit && hooks_.bit(FF6_EVENT_BIT_WORLD_ALT_SONG)) {
        song = world_ == 0 ? FF6_SONG_VELDT : FF6_SONG_SEARCHING_FOR_FRIENDS;
    }
    sound_.command(FF6_SOUND_PLAY_SONG, static_cast<uint8_t>(song), FF6_SOUND_FULL_VOLUME);
}

// ModifyMap: the map of the file, with the rectangles of the events that happened.
void WorldScreen::applyModifications() {
    const WorldMapData& map = this->map();
    tilemap_ = map.tilemap;
    for (const WorldMod& mod : map.mods) {
        if (!hooks_.bit || !hooks_.bit(mod.bit & 0x7FFF)) continue;
        for (int row = 0; row < mod.height; ++row) {
            for (int column = 0; column < mod.width; ++column) {
                const size_t at = static_cast<size_t>(((mod.y + row) & 255) * FF6_WORLD_SIZE + ((mod.x + column) & 255));
                tilemap_[at] = mod.tiles[static_cast<size_t>(row * mod.width + column)];
            }
        }
    }
}

// The memory of the picture processor as LoadWorld fills it: the characters of the map, the sprites of the mini-map, the
// palettes, and the water that is going to move.
void WorldScreen::loadGraphics() {
    const WorldMapData& map = this->map();
    ppu_.clearVram();
    for (size_t i = 0; i < map.chr.size(); ++i) ppu_.vram[i] = static_cast<uint16_t>(map.chr[i] << 8);
    for (size_t i = 0; i + 1 < map.minimap.size(); i += 2) ppu_.vram[kVramMinimap + i / 2] = word(map.minimap, i);
    for (size_t i = 0; i + 1 < data_.miscSprites.size(); i += 2) ppu_.vram[kVramMisc + i / 2] = word(data_.miscSprites, i);
    for (size_t i = 0; i + 1 < data_.animGfx.size(); i += 2) ppu_.vram[kVramAnimGfx + i / 2] = word(data_.animGfx, i);
    const std::vector<uint8_t>& airship = data_.airshipGfx[static_cast<size_t>(world_ & 1)];
    for (size_t i = 0; i + 1 < airship.size(); i += 2) ppu_.vram[kVramAirshipGfx + i / 2] = word(airship, i);
    for (size_t i = 0; i < FF6_WORLD_PALETTE_COLORS; ++i) {
        ppu_.cgram[i] = map.palette[i];
        ppu_.cgram[FF6_WORLD_PALETTE_COLORS + i] = map.spritePalette[i];
    }
    // The second palette of the sprites starts as a copy of the first.
    for (size_t i = 0; i < 16; ++i) ppu_.cgram[144 + i] = ppu_.cgram[128 + i];
    // FixMinimap: the floating continent is not on the mini-map once it has gone up.
    if (world_ == 0 && hooks_.bit && hooks_.bit(FF6_EVENT_BIT_FLOATING_LIFTED)) {
        for (size_t i = 86; i <= 88; ++i) ppu_.cgram[128 + i] = ppu_.cgram[128 + 81];
    }
    // InitWaterGfx: the characters 0x44, 0x45, 0x54 and 0x55 are kept apart to be turned.
    for (int tile = 0; tile < 4; ++tile) {
        const int source = (tile < 2 ? kVramWaterA : kVramWaterB) / 64 + (tile & 1);
        for (int i = 0; i < 64; ++i) water_[static_cast<size_t>(tile * 64 + i)] = static_cast<uint8_t>(ppu_.vram[static_cast<size_t>(source * 64 + i)] >> 8);
    }
    for (int i = 0; i < kWaterRows; ++i) waterCounter_[static_cast<size_t>(i)] = kWaterStart[i];
    ppu_.objsel = 3;  // sprites of 8 and 16 pixels, their characters from 0x6000
    ppu_.tm = 0x13;
    ppu_.ts = 0x10;
    ppu_.forceBlank = false;
    ppu_.sprites.assign(FF6_WORLD_SPRITE_COUNT, PpuSprite());
    for (PpuSprite& sprite : ppu_.sprites) sprite.y = 0xE0;
}

// UpdateTilemap256 and InitTilemap256: the video memory has the 64 x 64 tiles of the map that start 32 tiles to the
// left of and above the party, each tile at the place that its coordinates give modulo 64.
void WorldScreen::buildWindow() {
    const int x0 = (((pixelX() - 0x200) & (mapSize() * 16 - 1)) >> 4), y0 = (((pixelY() - 0x200) & (mapSize() * 16 - 1)) >> 4);
    if (x0 == windowX_ && y0 == windowY_) return;
    windowX_ = x0;
    windowY_ = y0;
    const WorldMapData& map = this->map();
    for (int dy = 0; dy < kTilesPerWindow; ++dy) {
        for (int dx = 0; dx < kTilesPerWindow; ++dx) {
            const int wx = (x0 + dx) & (mapSize() - 1), wy = (y0 + dy) & (mapSize() - 1);
            const uint8_t* characters = &map.formation[static_cast<size_t>(tileAt(wx, wy)) * 4];
            const size_t at = static_cast<size_t>(((wy * 2) & 127) * 128 + ((wx * 2) & 127));
            // The low byte of a word of the video memory is the map, the high byte the graphics of the characters.
            const auto put = [&](size_t index, uint8_t character) {
                ppu_.vram[index] = static_cast<uint16_t>((ppu_.vram[index] & 0xFF00) | character);
            };
            put(at, characters[0]);
            put(at + 1, characters[1]);
            put(at + 128, characters[2]);
            put(at + 129, characters[3]);
        }
    }
}

// UpdateMode7Vars: the matrix of every line scales the map less and less going down, which gives the perspective.
void WorldScreen::updateMode7Vars() {
    const int scale = zoomHigh_ << 8;
    int divisor = FF6_WORLD_DIVISOR_START;
    const int step = (FF6_WORLD_DIVISOR_END - FF6_WORLD_DIVISOR_START) / FF6_WORLD_MAP_LINES;
    for (int y = 0; y < FF6_WORLD_MAP_LINES; ++y) {
        PpuLine& line = ppu_.lines[static_cast<size_t>(y)];
        const int quotient = scale / std::max(1, divisor >> 8);
        divisor = (divisor + step) & 0xFFFF;
        line.bgMode = 7;
        line.m7a = line.m7d = static_cast<int16_t>(quotient);
        line.m7b = line.m7c = 0;
        line.cgwsel = 2;
        line.cgadsub = 0x63;
        line.fixed = {0, 0, 0};
    }
    ppu_.m7x = static_cast<uint16_t>((pixelX() + 8) & 0xFFF);
    ppu_.m7y = static_cast<uint16_t>((pixelY() - 2) & 0xFFF);
    ppu_.m7hofs = static_cast<uint16_t>((ppu_.m7x - 0x80) & 0x1FFF);
    ppu_.m7vofs = static_cast<uint16_t>((ppu_.m7y - 0x70) & 0x1FFF);
}

bool WorldScreen::takeMenuRequest() {
    const bool requested = menuRequested_;
    menuRequested_ = false;
    return requested;
}

bool WorldScreen::takeEntrance(EntranceTarget* target) {
    if (!entranceReady_) return false;
    entranceReady_ = false;
    if (mode_ == Mode::Fly && !deck_ && !keepFlight_) {  // a landing that the story takes over: the airship stays where it is
        airshipX_ = flight_.x >> 12;
        airshipY_ = flight_.y >> 12;
    }
    *target = entrance_;
    return true;
}

bool WorldScreen::takeBattle(int* group, int* background) {
    if (!battleReady_) return false;
    battleReady_ = false;
    *group = battleGroup_;
    *background = battleBackground_;
    return true;
}

// BattleZoom, then the walk: one step of the program for each frame.
void WorldScreen::tick(const InputState& input) {
    if (mode_ != Mode::Walk) {
        tickFlight(input);
        return;
    }
    if (zoom_ >= 0) {  // BattleZoom: nothing else moves
        zoomHigh_ = kZoomTable[zoom_] & 0xFF;
        if (++zoom_ == FF6_WORLD_ZOOM_STEPS) {
            zoom_ = -1;
            battleReady_ = true;
            zoomHigh_ = FF6_WORLD_SCALE_TOP >> 8;
        }
        return;
    }
    if (input.start && !eventRunning_) minimapShown_ = !minimapShown_;
    confirm_ = input.confirm;
    movePlayer(input);
    updateSpriteAnim();
    updateWater();
    buildWindow();
    if (brightness_ < targetBrightness_) ++brightness_;
    if (brightness_ > targetBrightness_) --brightness_;
}

void WorldScreen::movePlayer(const InputState& input) {
    if (aligned()) {
        speedX_ = speedY_ = 0;  // a step is over; the input or the event decides about the next one
        if (propAt(tileX(), tileY()) & FF6_WORLD_TILE_FOREST) forest_ = true;
        if (stepStarted_) {
            stepStarted_ = false;
            if (!eventRunning_) {
                checkEvent();
                checkEntrance();
                if (!battleChecked_) {
                    battleChecked_ = true;
                    if (!(hooks_.bit && hooks_.bit(FF6_EVENT_BIT_NO_ENCOUNTERS)) && (propAt(tileX(), tileY()) & FF6_WORLD_TILE_BATTLE)) checkBattle();
                }
            }
        }
        if (battleReady_ || zoom_ >= 0) return;
        if (eventRunning_) execWorldCmd();
        if (!eventRunning_ && !entranceReady_) getPlayerInput(input);
    }
    dfx_ = static_cast<uint16_t>(dfx_ + speedX_);
    dfy_ = static_cast<uint16_t>(dfy_ + speedY_);
}

// GetPlayerInput: a direction turns the party and starts a step when the tile that it leads to can be walked on.
void WorldScreen::getPlayerInput(const InputState& input) {
    speedX_ = speedY_ = 0;
    if (input.confirm && tileX() == airshipX_ && tileY() == airshipY_ && hooks_.bit && hooks_.bit(FF6_EVENT_BIT_AIRSHIP_VISIBLE)) {
        boardAirship();
        return;
    }
    const int direction = input.direction;
    if (direction >= 0 && direction < FF6_DIRECTION_COUNT) {
        facing_ = direction;
        drawWorldChar();
        const uint16_t prop = propAt(tileX() + kDeltaX[direction], tileY() + kDeltaY[direction]);
        if (!(prop & FF6_WORLD_TILE_BLOCKED)) {
            if (!(prop & FF6_WORLD_TILE_FOREST)) forest_ = false;
            stepStarted_ = true;
            speedX_ = kDeltaX[direction] * kPixelsPerStep;
            speedY_ = kDeltaY[direction] * kPixelsPerStep;
            battleChecked_ = false;
            if (hooks_.step) hooks_.step();
        }
    }
    walkSpeed_ = 0x10;
    if (!moving() && input.menu && !menuLocked_) menuRequested_ = true;
}

// CheckEvent: a tile with an event runs its script.
void WorldScreen::checkEvent() {
    const std::vector<uint8_t>& triggers = map().eventTriggers;
    for (size_t i = 0; i + FF6_WORLD_EVENT_TRIGGER_SIZE <= triggers.size(); i += FF6_WORLD_EVENT_TRIGGER_SIZE) {
        if (triggers[i] != tileX() || triggers[i + 1] != tileY()) continue;
        startEvent(static_cast<uint32_t>(triggers[i + 2] | (triggers[i + 3] << 8) | (triggers[i + 4] << 16)));
        battleChecked_ = true;
        menuLocked_ = true;
        return;
    }
}

// CheckEntrance: a tile that leads to another map, where the party is taken once the screen has faded out.
bool WorldScreen::checkEntrance() {
    const std::vector<uint8_t>& entrances = map().shortEntrances;
    for (size_t i = 0; i + FF6_WORLD_SHORT_ENTRANCE_SIZE <= entrances.size(); i += FF6_WORLD_SHORT_ENTRANCE_SIZE) {
        if (entrances[i] != tileX() || entrances[i + 1] != tileY()) continue;
        entrance_ = makeEntranceTarget(word(entrances, i + 2), entrances[i + 4], entrances[i + 5]);
        entranceReady_ = true;
        battleChecked_ = true;
        menuLocked_ = true;
        return true;
    }
    return false;
}

// CheckBattleWorld: the table of the sector that the party was in when the map was loaded (the game has the same fault).
void WorldScreen::checkBattle() {
    int background = 0;
    const int group = hooks_.encounter ? hooks_.encounter(world_, (propAt(tileX(), tileY()) >> 8) & 7, sectorX_, sectorY_, &background) : -1;
    if (group < 0) return;
    battleGroup_ = group;
    battleBackground_ = background;
    sectorX_ = tileX();
    sectorY_ = tileY();
    sound_.command(FF6_SOUND_GAME_SFX, FF6_WORLD_BATTLE_SFX, FF6_SFX_PAN_CENTER);
    zoom_ = 0;
}

void WorldScreen::startEvent(uint32_t offset) {
    scriptPtr_ = offset;
    eventRunning_ = true;
    inCommand_ = false;
    commandCount_ = 0;
}

uint8_t WorldScreen::scriptByte() {
    const std::vector<uint8_t>& script = common_.eventScript;
    return scriptPtr_ < script.size() ? script[scriptPtr_++] : static_cast<uint8_t>(0xFF);
}

int WorldScreen::scriptWord() {
    const int low = scriptByte();
    return low | (scriptByte() << 8);
}

void WorldScreen::jumpTo(uint32_t offset) { scriptPtr_ = offset; }

// The conditions of the jumps: an event bit, which has to be clear when the high bit of the word is set.
bool WorldScreen::conditionMet(int value) const {
    const bool set = hooks_.bit && hooks_.bit(value & 0x7FFF);
    return (value & 0x8000) ? !set : set;
}

// ExecWorldCmd: one command, or more when they do not take any time.
void WorldScreen::execWorldCmd() {
    speedX_ = speedY_ = 0;
    for (int guard = 0; guard < 0x10000 && eventRunning_; ++guard) {
        const int cmd = inCommand_ ? command_ : scriptByte();
        command_ = cmd;
        if (cmd < 0x80) {  // a frame of the character
            frame_ = actionFrame(cmd);
            continue;
        }
        if (cmd < 0xA0) {  // walk a number of tiles
            if (commandCount_ == 0) {
                commandCount_ = ((cmd >> 2) & 7) + 1;
                inCommand_ = true;
            }
            const int direction = (cmd & 3) == 0 ? Up : (cmd & 3) == 1 ? Right : (cmd & 3) == 2 ? Down : Left;
            facing_ = direction;
            speedX_ = kDeltaX[direction] * walkSpeed_;
            speedY_ = kDeltaY[direction] * walkSpeed_;
            if (!(propAt(tileX() + kDeltaX[direction], tileY() + kDeltaY[direction]) & FF6_WORLD_TILE_FOREST)) forest_ = false;
            if (--commandCount_ == 0) inCommand_ = false;
            return;
        }
        if (cmd >= 0xA0 && cmd <= 0xAB) {  // walk along a diagonal: right or left by 1 or 2 tiles, up or down by 1 or 2
            static constexpr int kDiagonals[12][3] = {{1, -1, Up},  {1, 1, Down},  {-1, 1, Down}, {-1, -1, Up}, {1, -2, Up},   {2, -1, Right},
                                                      {2, 1, Right}, {1, 2, Down}, {-1, 2, Down}, {-2, 1, Left}, {-2, -1, Left}, {-1, -2, Up}};
            const int* diagonal = kDiagonals[cmd - 0xA0];
            speedX_ = diagonal[0] * walkSpeed_;
            speedY_ = diagonal[1] * walkSpeed_;
            facing_ = diagonal[2];
            if (!(propAt(tileX() + diagonal[0], tileY() + diagonal[1]) & FF6_WORLD_TILE_FOREST)) forest_ = false;
            return;
        }
        if (cmd >= 0xB0 && cmd < 0xC0) {  // conditional jump: the bits that count are listed after the command
            const int count = (cmd & 7) + 1;
            std::vector<int> conditions;
            for (int i = 0; i < count; ++i) conditions.push_back(scriptWord());
            const uint32_t target = static_cast<uint32_t>(scriptByte() | (scriptByte() << 8) | (scriptByte() << 16));
            bool jump = false;
            if (cmd < 0xB8) {  // or
                for (const int condition : conditions) jump = jump || conditionMet(condition);
            } else {  // and
                jump = true;
                for (const int condition : conditions) jump = jump && conditionMet(condition);
            }
            if (jump) jumpTo(target);
            continue;
        }
        switch (cmd) {
            case 0xC0: walkSpeed_ = 0x04; break;
            case 0xC1: walkSpeed_ = 0x08; break;
            case 0xC2: walkSpeed_ = 0x10; break;
            case 0xC3: walkSpeed_ = 0x20; break;
            case 0xC4: walkSpeed_ = 0x40; break;
            case 0xC5:
            case 0xC6:
            case 0xC7: {  // where the airship is parked
                airshipX_ = scriptByte();
                airshipY_ = scriptByte();
                break;
            }
            case 0xC8:
            case 0xC9: {
                const int bit = scriptWord();
                if (hooks_.setBit) hooks_.setBit(bit & 0x7FFF, cmd == 0xC8);
                break;
            }
            case 0xCA:
            case 0xCB:
            case 0xCC: facing_ = Up; drawWorldChar(); break;
            case 0xCD: facing_ = Right; drawWorldChar(); break;
            case 0xCE: facing_ = Down; drawWorldChar(); break;
            case 0xCF: facing_ = Left; drawWorldChar(); break;
            case 0xD0: hidden_ = false; break;
            case 0xD1: hidden_ = true; break;
            case 0xD2: loadMapCommand(); return;
            case 0xD3: loadMapCommand(); return;
            case 0xD4: {  // jump when the confirm button is not pressed
                const uint32_t target = static_cast<uint32_t>(scriptByte() | (scriptByte() << 8) | (scriptByte() << 16));
                if (!confirm_) jumpTo(target);
                break;
            }
            case 0xD5: {  // jump when the party does not face the direction
                const int direction = scriptByte();
                const uint32_t target = static_cast<uint32_t>(scriptByte() | (scriptByte() << 8) | (scriptByte() << 16));
                if (direction != facing_) jumpTo(target);
                break;
            }
            case 0xD6:
            case 0xD7:
            case 0xD8: targetBrightness_ = 15; break;
            case 0xD9: targetBrightness_ = 0; break;
            case 0xDA:
            case 0xDB:
            case 0xDC:
            case 0xDD: minimapShown_ = false; break;
            case 0xDE:
            case 0xDF: minimapShown_ = true; break;
            case 0xE0: {  // pause: the first call reads the duration
                if (commandCount_ == 0) {
                    commandCount_ = scriptByte();
                    pauseCounter_ = 15;
                    inCommand_ = true;
                }
                if (--pauseCounter_ == 0) {
                    if (--commandCount_ == 0) {
                        inCommand_ = false;
                    } else {
                        pauseCounter_ = 4;
                    }
                }
                return;
            }
            case 0xFF:
                eventRunning_ = false;
                storyScript_ = false;
                menuLocked_ = false;
                return;
            default:
                std::fprintf(stderr, "world event: unsupported command 0x%02X, event ended\n", cmd);
                eventRunning_ = false;
                storyScript_ = false;
                menuLocked_ = false;
                return;
        }
    }
}

// Command 0xD2: another map, which takes the party away from the world map.
void WorldScreen::loadMapCommand() {
    const int mapWord = scriptWord();
    const int x = scriptByte();
    const int y = scriptByte();
    keepFlight_ = mode_ == Mode::Fly && (scriptByte() & FF6_EVENT_MAP_AIRSHIP);
    entrance_ = makeEntranceTarget(static_cast<uint16_t>(mapWord), x, y);
    entranceReady_ = true;
    eventRunning_ = false;
    resumeValid_ = storyScript_;
    resumeOffset_ = scriptPtr_;
}

// DrawWorldChar: the frame of the sprite sheet for the way that the party faces and the step of the walk.
void WorldScreen::drawWorldChar() { frame_ = moveFrame(facing_, phase_ << FF6_WALK_POSE_SHIFT); }

// UpdateSpriteAnim_03: while the party walks, the step changes every eight frames.
void WorldScreen::updateSpriteAnim() {
    if (moving()) {
        phaseCounter_ += walkSpeed_;
        if (phaseCounter_ >= 0x80) {
            phase_ = (phase_ + 1) & 3;
            phaseCounter_ = 0;
        }
        drawWorldChar();
    }
    setCharacterSprites();
}

// The six characters of the sprites that the walking character is made of, from the sheet of the lead, and the palette.
void WorldScreen::setCharacterSprites() {
    if (lead_.gfx < 0 || static_cast<size_t>(lead_.gfx) >= common_.spriteGfxCount()) return;
    const uint8_t* sheet = &common_.spriteGfx[static_cast<size_t>(lead_.gfx) * FF6_SPRITE_SHEET_BYTES + static_cast<size_t>(frame_) * FF6_FRAME_WIDTH * FF6_FRAME_HEIGHT];
    for (int tile = 0; tile < 6; ++tile) {
        const int column = tile & 1, row = tile >> 1;
        const size_t base = static_cast<size_t>(kVramCharacter + tile * 16);
        for (int line = 0; line < 8; ++line) {
            uint16_t low = 0, high = 0;
            for (int pixel = 0; pixel < 8; ++pixel) {
                const int index = sheet[(row * 8 + line) * FF6_FRAME_WIDTH + column * 8 + pixel] & 15;
                const int bit = 7 - pixel;
                low = static_cast<uint16_t>(low | ((index & 1) << bit) | (((index >> 1) & 1) << (8 + bit)));
                high = static_cast<uint16_t>(high | (((index >> 2) & 1) << bit) | (((index >> 3) & 1) << (8 + bit)));
            }
            ppu_.vram[base + static_cast<size_t>(line)] = low;
            ppu_.vram[base + 8 + static_cast<size_t>(line)] = high;
        }
    }
    if (lead_.palette >= 0 && static_cast<size_t>(lead_.palette) < common_.spritePaletteCount()) {
        for (size_t i = 0; i < FF6_SPRITE_PAL_COLORS; ++i) {
            const uint16_t color = common_.spritePalettes[static_cast<size_t>(lead_.palette) * FF6_SPRITE_PAL_COLORS + i];
            ppu_.cgram[128 + i] = color;
            ppu_.cgram[192 + i] = color;
        }
    }
}

// UpdateWaterAnim: every 16 frames each row of the animated characters moves a pixel, the rows one after the other, and
// the colors of the water of the first world turn.
void WorldScreen::updateWater() {
    for (int x = 0; x < kWaterRows; ++x) {
        uint8_t& counter = waterCounter_[static_cast<size_t>(x)];
        const int sum = counter + 0x10;
        counter = static_cast<uint8_t>(sum);
        if (sum < 0x100) continue;
        const int row = (x < 8 ? x : x + 8) * 8;
        uint8_t* first = &water_[static_cast<size_t>(row)];
        uint8_t* second = first + 0x40;
        const uint8_t kept = first[0];
        for (int i = 0; i < 7; ++i) first[i] = first[i + 1];
        first[7] = second[0];
        for (int i = 0; i < 7; ++i) second[i] = second[i + 1];
        second[7] = kept;
    }
    for (int tile = 0; tile < 4; ++tile) {
        const int target = (tile < 2 ? kVramWaterA : kVramWaterB) / 64 + (tile & 1);
        for (int i = 0; i < 64; ++i) {
            uint16_t& cell = ppu_.vram[static_cast<size_t>(target * 64 + i)];
            cell = static_cast<uint16_t>((cell & 0xFF) | (water_[static_cast<size_t>(tile * 64 + i)] << 8));
        }
    }
    if (world_ == 0) {
        paletteCounter_ = (paletteCounter_ - 1) & 0x0F;
        if (paletteCounter_ == 0) {
            const uint16_t first = ppu_.cgram[109], second = ppu_.cgram[110], third = ppu_.cgram[111];
            ppu_.cgram[109] = second;
            ppu_.cgram[110] = third;
            ppu_.cgram[111] = first;
            paletteCounter_ = 0x10;
        }
    }
}

// ShowMinimap and DrawMinimapPos: sixteen sprites of 16 x 16 for the picture of the map, and one for where the party is.
void WorldScreen::drawMinimap() {
    for (size_t i = 0; i < kCharacterFirstSprite; ++i) ppu_.sprites[i].y = 0xE0;
    if (!minimapShown_) return;
    int tile = 0;
    for (int k = 0; k < 16; ++k) {
        PpuSprite& sprite = ppu_.sprites[static_cast<size_t>(kMinimapFirstSprite + k)];
        sprite.x = 0xB0 + 16 * (k & 3);
        sprite.y = 0x9C + 16 * (k >> 2);
        sprite.tile = tile | 0x100;
        tile = (tile + 0x12) & 0x2E;
        sprite.attr = 5 << 1;
        sprite.large = true;
    }
    PpuSprite& mark = ppu_.sprites[0];
    mark.x = ((pixelX() >> 6) + kMinimapX) & 0xFF;
    mark.y = ((pixelY() >> 6) + kMinimapY) & 0xFF;
    mark.tile = 0x180;
    mark.attr = (5 << 1) | (1 << 4);
    mark.large = true;
}

void WorldScreen::draw(uint8_t* rgba) {
    if (mode_ == Mode::TweenIn || mode_ == Mode::Fly || mode_ == Mode::TweenOut || mode_ == Mode::Dismount) {
        renderFlight();
        ppu_.brightness = static_cast<uint8_t>(brightness_);
    } else {
        updateMode7Vars();
        drawMinimap();
        // The character is six sprites of 8 x 8 pixels; the lower ones show through the trees and are behind the map then.
        for (int tile = 0; tile < 6; ++tile) {
            PpuSprite& sprite = ppu_.sprites[static_cast<size_t>(kCharacterFirstSprite + tile)];
            sprite.x = 120 + 8 * (tile & 1);
            sprite.y = hidden_ ? 0xE0 : 104 + 8 * (tile >> 1);
            sprite.tile = 0xF0 + tile;
            sprite.large = false;
            const bool lower = tile >= 4;
            sprite.attr = lower && forest_ ? static_cast<uint8_t>(4 << 1) : static_cast<uint8_t>(1 << 4);
        }
        drawParkedAirship();
        ppu_.tm = zoom_ >= 0 ? 0x03 : 0x13;
        ppu_.ts = zoom_ >= 0 ? 0x00 : 0x10;
        ppu_.brightness = static_cast<uint8_t>(zoom_ >= 0 ? kZoomTable[std::min(zoom_, FF6_WORLD_ZOOM_STEPS - 1)] >> 8 : brightness_);
    }
    ppu_.render(rgba);
    // The window of the game hides the columns at the sides of the screen.
    uint8_t backdrop[FF6_BYTES_PER_PIXEL];
    colorToRgba(ppu_.cgram[0], backdrop);
    for (int c = 0; c < FF6_ALPHA_OFFSET; ++c) backdrop[c] = static_cast<uint8_t>(backdrop[c] * ppu_.brightness / 15);
    backdrop[FF6_ALPHA_OFFSET] = 255;
    for (int y = 0; y < FF6_SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
            if (x >= FF6_WORLD_BORDER && x < FF6_SCREEN_WIDTH - FF6_WORLD_BORDER) continue;
            std::copy(backdrop, backdrop + FF6_BYTES_PER_PIXEL, rgba + (static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL);
        }
    }
}

}  // namespace ff6
