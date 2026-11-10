#include "game.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

#include "field/bg.hpp"
#include "field/color.hpp"
#include "field/sprite_data.hpp"
#include "field/window.hpp"
#include "menu/window_style.hpp"
#include "state/char_stats.hpp"

namespace ff6 {
namespace {

const Rect kScreen{0, 0, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT};

struct Drawable {
    int sortY;
    int strip;  // index into Game::strips_
    int frame;
    int screenX, screenY;
    bool topAbove, bottomAbove;
    bool onVehicle = false;
    VehicleLook look;  // what it rides, when it does
};

}  // namespace

Game::Game(CommonData common, std::string packDir, uint32_t startMap, Renderer& renderer, Sound& sound,
           const std::vector<int>& party)
    : common_(std::move(common)), packDir_(std::move(packDir)), renderer_(renderer), sound_(sound), event_(common_, bits_),
      menuScreen_(common_, menuData_), menu_(menuScreen_, sound_), shop_(menuScreen_, sound_), colosseum_(menuScreen_, sound_), nameMenu_(menuScreen_, sound_), finalOrderMenu_(menuScreen_, sound_), battleScreen_(menuScreen_, sound_),
      loadMenu_(menuScreen_, sound_), partyMenu_(menuScreen_, sound_) {
    initialize(startMap, party, nullptr);
}

Game::Game(CommonData common, std::string packDir, const SaveState& save, Renderer& renderer, Sound& sound)
    : common_(std::move(common)), packDir_(std::move(packDir)), renderer_(renderer), sound_(sound), event_(common_, bits_),
      menuScreen_(common_, menuData_), menu_(menuScreen_, sound_), shop_(menuScreen_, sound_), colosseum_(menuScreen_, sound_), nameMenu_(menuScreen_, sound_), finalOrderMenu_(menuScreen_, sound_), battleScreen_(menuScreen_, sound_),
      loadMenu_(menuScreen_, sound_), partyMenu_(menuScreen_, sound_) {
    initialize(static_cast<uint32_t>(save.map), {}, &save);
}

Game::Game(CommonData common, std::string packDir, NewGame, Renderer& renderer, Sound& sound)
    : common_(std::move(common)), packDir_(std::move(packDir)), renderer_(renderer), sound_(sound), event_(common_, bits_),
      menuScreen_(common_, menuData_), menu_(menuScreen_, sound_), shop_(menuScreen_, sound_), colosseum_(menuScreen_, sound_), nameMenu_(menuScreen_, sound_), finalOrderMenu_(menuScreen_, sound_), battleScreen_(menuScreen_, sound_),
      loadMenu_(menuScreen_, sound_), partyMenu_(menuScreen_, sound_) {
    initialize(FF6_GAME_START_MAP, {}, nullptr, true);
}

void Game::initialize(uint32_t startMap, const std::vector<int>& party, const SaveState* save, bool newGame) {
    bits_.init(common_);
    menuData_ = loadMenuData(packDir_);
    rules_ = loadGameRules(packDir_);
    battleData_ = loadBattleData(packDir_);
    battleGfx_ = loadBattleGfx(packDir_);
    battleScreen_.setGfx(&battleGfx_);
    attackAnim_ = loadAttackAnim(packDir_);
    resetSpriteColors();
    battleScreen_.setAttackAnim(&attackAnim_);
    for (int i = 0; i < FF6_CHAR_COUNT; ++i) setCharacterProperties(state_, rules_, i, i, 0);
    for (int i = 0; i < FF6_CHAR_COUNT; ++i) setCharacterName(i, std::string());
    config_.windowColors = menuData_.windowColors;
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
    if (!party.empty()) objects_.startParty(party);
    int x = -1, y = -1, facing = Down;
    if (save) {  // a saved game goes on where it was, and its startup event runs again like in the game
        restore(*save);
        x = save->x;
        y = save->y;
        facing = save->facing;
    }
    common_.windowTiles = buildWindowTiles(menuData_, config_);
    if (!loadMapNow(startMap, x, y, facing, !newGame, !newGame)) throw std::runtime_error("the start map is not in the data pack");
    if (newGame) startEvent(FF6_EVENT_SCRIPT_GAME_START);  // the screen stays dark: the first command shows the title
}

// The state of the story and of the characters, before the map is loaded.
void Game::restore(const SaveState& save) {
    bits_.load(save.eventBits, save.battleFlags);
    bits_.loadVariables(save.variables, save.timers);
    if (save.hasParty) state_ = save.party;
    for (int i = 0; i < FF6_CHAR_COUNT; ++i) setCharacterName(i, static_cast<size_t>(i) < save.names.size() ? save.names[static_cast<size_t>(i)] : std::string());
    std::array<Npc, FF6_CHAR_COUNT>& characters = objects_.characters();
    for (size_t i = 0; i < characters.size(); ++i) {
        const SaveCharacter& saved = save.characters[i];
        Npc& character = characters[i];
        character.enabled = saved.enabled;
        character.visible = saved.visible;
        character.party = saved.party;
        character.partySlot = saved.slot;
        character.map = saved.map;
        character.prop.gfx = saved.gfx;
        character.prop.palette = saved.palette;
        character.posX = saved.x * FF6_OBJ_TILE_SUBPIXELS;
        character.posY = saved.y * FF6_OBJ_TILE_SUBPIXELS;
        character.facing = saved.facing;
        character.frame = stopFrame(saved.facing);
    }
    objects_.restoreParty(save.activeParty);
    for (size_t i = 0; i < spritePalette_.size(); ++i) spritePalette_[i] = save.spritePalettes[i];
    resetSpriteColors();
    doomGazeCounter_ = save.doomGazeCounter;
    airshipX_ = save.airshipX;
    airshipY_ = save.airshipY;
    flightSaved_ = false;
    hasParent_ = save.hasParent;
    parentMap_ = static_cast<uint32_t>(save.parentMap);
    parentX_ = save.parentX;
    parentY_ = save.parentY;
    parentFacing_ = save.parentFacing;
    config_.battle = static_cast<uint8_t>(save.config[0]);
    config_.display = static_cast<uint8_t>(save.config[1]);
    config_.controller = static_cast<uint8_t>(save.config[2]);
    if (save.hasColors) {
        config_.fontColor = static_cast<uint16_t>(save.fontColor);
        for (size_t i = 0; i < save.windowColors.size(); ++i) {
            config_.windowColors[i / FF6_MENU_WINDOW_COLORS][i % FF6_MENU_WINDOW_COLORS] = static_cast<uint16_t>(save.windowColors[i]);
        }
    }
}

SaveState Game::saveState() const {
    SaveState save;
    save.map = world_ ? world_->world() : static_cast<int>(map_.id);
    save.x = partyTileX();
    save.y = partyTileY();
    save.facing = world_ ? world_->facing() : player_.facing();
    save.activeParty = objects_.activeParty();
    for (size_t i = 0; i < save.characters.size(); ++i) {
        const Npc& character = objects_.characters()[i];
        SaveCharacter& saved = save.characters[i];
        saved.enabled = character.enabled;
        saved.visible = character.visible;
        saved.party = character.party;
        saved.slot = character.partySlot;
        saved.map = character.map;
        saved.gfx = character.prop.gfx;
        saved.palette = character.prop.palette;
        saved.x = std::max(0, std::min(character.tileX(), FF6_MAX_TILE));
        saved.y = std::max(0, std::min(character.tileY(), FF6_MAX_TILE));
        saved.facing = character.facing;
    }
    save.eventBits = bits_.setIds();
    save.battleFlags = bits_.battleSetIds();
    save.variables = bits_.variableValues();
    save.timers = bits_.timerValues();
    for (size_t i = 0; i < spritePalette_.size(); ++i) save.spritePalettes[i] = spritePalette_[i];
    save.doomGazeCounter = doomGazeCounter_;
    save.airshipX = world_ ? world_->airshipX() : airshipX_;
    save.airshipY = world_ ? world_->airshipY() : airshipY_;
    save.hasParent = hasParent_;
    save.parentMap = static_cast<int>(parentMap_);
    save.parentX = parentX_;
    save.parentY = parentY_;
    save.parentFacing = parentFacing_;
    save.config = {config_.battle, config_.display, config_.controller};
    save.hasColors = true;
    save.hasParty = true;
    save.party = state_;
    save.fontColor = config_.fontColor;
    for (int i = 0; i < FF6_CHAR_COUNT; ++i) save.names.push_back(customCharacterName(i));
    for (size_t i = 0; i < save.windowColors.size(); ++i) {
        save.windowColors[i] = config_.windowColors[i / FF6_MENU_WINDOW_COLORS][i % FF6_MENU_WINDOW_COLORS];
    }
    return save;
}

int Game::stripFor(int gfx, int palette) {
    const std::pair<int, int> key(gfx, palette);
    const auto found = stripIndex_.find(key);
    if (found != stripIndex_.end()) return found->second;
    strips_.push_back(buildSpriteStrip(common_, gfx, palette));
    stripIndex_[key] = static_cast<int>(strips_.size() - 1);
    return stripIndex_[key];
}

const std::vector<uint8_t>& Game::vehicleSprite(int palette, uint16_t word) {
    const std::pair<int, int> key(palette, word);
    auto found = vehicleSprites_.find(key);
    if (found == vehicleSprites_.end()) found = vehicleSprites_.emplace(key, buildVehicleSprite(common_, palette, word)).first;
    return found->second;
}

int Game::stripOf(Npc& object) {
    if (object.strip < 0) {
        const size_t slot = static_cast<size_t>(object.prop.palette) & (FF6_SPRITE_PALETTE_SLOTS - 1);
        if (spriteModified_) {  // an event changed the colors: the strip is built from them
            const std::pair<int, int> key(object.prop.gfx, -1 - static_cast<int>(slot));
            auto found = stripIndex_.find(key);
            if (found == stripIndex_.end()) {
                strips_.push_back(buildSpriteStripColors(common_, object.prop.gfx, &spriteColors_[slot * FF6_SPRITE_PAL_COLORS]));
                found = stripIndex_.emplace(key, static_cast<int>(strips_.size() - 1)).first;
            }
            object.strip = found->second;
        } else {
            object.strip = stripFor(object.prop.gfx, spritePalette_[slot]);
        }
    }
    return object.strip;
}

// The eight palettes of the sprites are the ones of the slots, as they were before the events changed their colors.
void Game::resetSpriteColors() {
    for (size_t slot = 0; slot < spritePalette_.size(); ++slot) {
        for (size_t c = 0; c < FF6_SPRITE_PAL_COLORS; ++c) {
            const size_t palette = static_cast<size_t>(spritePalette_[slot]);
            spriteBase_[slot * FF6_SPRITE_PAL_COLORS + c] =
                palette < common_.spritePaletteCount() ? common_.spritePalettes[palette * FF6_SPRITE_PAL_COLORS + c] : 0;
        }
    }
    spriteColors_ = spriteBase_;
    spritePaletteChanged();
}

void Game::spritePaletteChanged() {
    spriteModified_ = spriteColors_ != spriteBase_;
    strips_.clear();
    stripIndex_.clear();
    invalidateStrips();
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

bool Game::loadMapNow(uint32_t mapId, int x, int y, int facing, bool startupEvent, bool fadeIn) {
    if (mapId < FF6_FIRST_FIELD_MAP) return enterWorld(mapId, x, y, facing, fadeIn);
    if (!mapExists(packDir_, mapId)) {
        std::fprintf(stderr, "map %u is not available in the data pack\n", static_cast<unsigned>(mapId));
        return false;
    }
    MapData next = loadMap(packDir_, mapId);
    world_.reset();
    if (resumePending_) {  // the event that the world map ran for gets back to where its script left it
        event_.setPc(resumeOffset_);
        resumePending_ = false;
    }
    worldScript_ = false;
    const bool inEvent = event_.running();  // an event that loads a map goes on once it is loaded
    map_ = std::move(next);
    bgBase_ = map_.palette;
    stage_.reset();
    showOpenedTreasures();
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
    bool startupStarted = false;
    if (startupEvent && map_.startupEvent < script.size() && script[map_.startupEvent] != FF6_EVENT_RETURN) {
        startupStarted = !inEvent;
        if (inEvent) {
            event_.call(map_.startupEvent);
        } else {
            startEvent(map_.startupEvent);
        }
    }
    // The screen is dark until the map is loaded; it fades in after the startup event, which the event that loads
    // the map does not wait for.
    brightness_ = 0;
    fade_ = 0;
    fadeInPending_ = false;
    if (fadeIn) {
        if (startupStarted) {
            fadeInPending_ = true;
        } else {
            startFade(true, FF6_BRIGHTNESS_STEP);
        }
    }
    return true;
}

// The codes of a dialogue, with the amount of gil and the name of the item that the chest event talks about.
void Game::openDialogue(uint32_t id, uint16_t flags) {
    const DialogueText text = common_.dialogue(id);
    dialogueText_.clear();
    for (size_t i = 0; text.codes && i < text.length; ++i) {
        const uint8_t code = text.codes[i];
        if (code == FF6_DLG_GP) {
            if (foundGil_ < 0) continue;
            for (const char digit : std::to_string(foundGil_)) dialogueText_.push_back(static_cast<uint8_t>(FF6_DLG_GLYPH_ZERO + digit - '0'));
        } else if (code == FF6_DLG_ITEM) {
            appendItemName(receivedItem_);
        } else {
            dialogueText_.push_back(code);
            const bool hasArgument = code == FF6_DLG_WAIT || code == FF6_DLG_TAB || code == FF6_DLG_KEY_TIMED;
            if (hasArgument && i + 1 < text.length) dialogueText_.push_back(text.codes[++i]);
        }
    }
    const DialogueText filled{text.codes ? dialogueText_.data() : nullptr, dialogueText_.size()};
    dialogue_.open(filled, common_.fontWidths, (flags & FF6_DLG_FLAG_BOTTOM) != 0);
    dialogueTextOnly_ = (flags & FF6_DLG_FLAG_TEXT_ONLY) != 0;
}

// The name of an item in the glyphs of the dialogues, without the symbol that comes first and the spaces at the end.
void Game::appendItemName(int item) {
    if (item < 0 || item >= FF6_ITEM_COUNT) return;
    const uint8_t* name = &menuData_.itemNames[static_cast<size_t>(item) * FF6_ITEM_NAME_BYTES];
    size_t end = FF6_ITEM_NAME_BYTES;
    while (end > 1 && name[end - 1] == FF6_MENU_CODE_SPACE) --end;
    for (size_t i = 1; i < end; ++i) {
        if (name[i] == FF6_MENU_CODE_SPACE) {
            dialogueText_.push_back(FF6_DLG_GLYPH_SPACE);
        } else if (name[i] >= FF6_MENU_CODE_UPPER_A && name[i] <= FF6_MENU_CODE_LAST_GLYPH) {
            dialogueText_.push_back(static_cast<uint8_t>(name[i] - FF6_DLG_FROM_MENU_CODE));
        }
    }
}

void Game::changeMap(uint32_t map, int x, int y, int facing, bool startupEvent, bool fadeIn) {
    eventLoad_ = true;
    loadMapNow(map, x, y, facing, startupEvent, fadeIn);
    eventLoad_ = false;
    pendingVehicle_ = 0;
}

// The sprite sheet and the palette of the character that leads the party, for the world map.
WorldScreen::Lead Game::worldLead() const {
    const Npc* lead = objects_.lead();
    if (!lead) return {};
    return {lead->prop.gfx, spritePalette_[static_cast<size_t>(lead->prop.palette) & (FF6_SPRITE_PALETTE_SLOTS - 1)]};
}

// LoadWorld: the party is on a world map. The event that was running does not follow it there.
bool Game::enterWorld(uint32_t mapId, int x, int y, int facing, bool fadeIn) {
    if (mapId >= FF6_WORLD_COUNT && mapId != FF6_TRENCH_MAP) {
        std::fprintf(stderr, "map %u is not available in the data pack\n", static_cast<unsigned>(mapId));
        return false;
    }
    if (!worldDataLoaded_) {
        worldData_ = loadWorldData(packDir_);
        worldDataLoaded_ = true;
    }
    if (!world_) {
        WorldHooks hooks;
        hooks.bit = [this](int id) { return bits_.get(id); };
        hooks.setBit = [this](int id, bool value) { bits_.set(id, value); };
        hooks.step = [this] { takeStep(); };
        hooks.encounter = [this](int world, int tileBackground, int sectorX, int sectorY, int* background) {
            return encounters_.stepWorld(battleData_, worldData_, world, tileBackground, sectorX, sectorY, background, &state_);
        };
        world_ = std::make_unique<WorldScreen>(worldData_, common_, sound_, hooks);
    }
    if (x < 0) {  // no place given: the middle of the map
        x = FF6_WORLD_SIZE / 2;
        y = FF6_WORLD_SIZE / 2;
    }
    // An event that loads the world map gives it the script that follows the command; the others do not follow the party.
    const bool handOver = (eventLoad_ || resumePending_) && event_.running();
    const uint32_t scriptOffset = resumePending_ ? resumeOffset_ : event_.pc();
    resumePending_ = false;
    if (!handOver) event_.stop();
    worldScript_ = handOver;
    dialogue_.open({}, common_.fontWidths, false);
    screenLocked_ = false;
    restorePending_ = false;
    eventWasRunning_ = false;
    leaving_ = false;
    const int vehicle = pendingVehicle_;
    const bool flying = vehicle == FF6_EVENT_MAP_AIRSHIP || (flightSaved_ && vehicle < 0);
    pendingVehicle_ = 0;
    world_->setAirship(airshipX_, airshipY_);
    if (mapId == FF6_TRENCH_MAP) {
        world_->enterTrench(x, y, facing, worldLead());
    } else if (vehicle == FF6_EVENT_MAP_CHOCOBO) {
        world_->enterRiding(static_cast<int>(mapId), x, y, facing, worldLead());
    } else if (flying) {
        world_->enterFlying(static_cast<int>(mapId), x, y, facing, worldLead(), flightAngle_, flightAltitude_);
        world_->setAirship(airshipX_, airshipY_);
    } else {
        world_->enter(static_cast<int>(mapId), x, y, facing, worldLead());
    }
    flightSaved_ = false;
    placeDoomGaze();
    if (handOver) world_->startStoryScript(scriptOffset);
    brightness_ = 0;
    fade_ = 0;
    fadeInPending_ = false;
    if (fadeIn) startFade(true, FF6_BRIGHTNESS_STEP);
    return true;
}

// A step of the party, on a map or on the world: the counter of the menu, and the poison.
void Game::takeStep() {
    countStep(state_);
    std::vector<int> members;
    for (const int id : objects_.partySlots()) {
        if (id >= 0) members.push_back(id);
    }
    if (poisonStep(state_, rules_, members)) sound_.command(FF6_SOUND_GAME_SFX, FF6_SFX_POISON_DAMAGE, FF6_SFX_PAN_CENTER);
}

// InitWorld for the airship: Doom Gaze waits in two squares of 4 x 4 tiles that the table of random numbers gives, which
// change each time that the map loads.
void Game::placeDoomGaze() {
    const std::vector<uint8_t>& rng = battleData_.rngTable;
    doomGazeCounter_ = (doomGazeCounter_ + FF6_DOOM_GAZE_COUNTER_STEP) & 0xFF;
    if (rng.size() < 0x100) return;
    const auto at = [&](int offset) { return rng[static_cast<size_t>((doomGazeCounter_ + offset) & 0xFF)]; };
    world_->setDoomGaze(!bits_.battle(FF6_BATTLE_FLAG_DOOM_GAZE_DEFEATED), at(0) & 0x3F, at(1) >> 2, at(1) & 0x3F, at(2) >> 2);
}

// What one frame of the world map does besides the walk: the menu, the entrances and the battles take the screen.
void Game::tickWorld(const InputState& input) {
    updateBrightness();
    bits_.tickTimers(false);  // they go off back on the field
    if (event_.running() && !worldScript_) {  // a story event that began on the world map, which can take the party elsewhere
        event_.update(*this);
        if (!world_) return;
    }
    if (leaving_) {
        if (!fading()) {
            leaving_ = false;
            leaveThroughEntrance(leaveTarget_);
        }
        return;
    }
    world_->tick(input);
    airshipX_ = world_->airshipX();
    airshipY_ = world_->airshipY();
    if (world_->takeMenuRequest()) {
        menuPhase_ = MenuPhase::FadingOut;
        startFade(false, FF6_BRIGHTNESS_STEP);
        return;
    }
    EntranceTarget target;
    const bool onDeck = world_->flying() && world_->keepsFlying();
    if (onDeck) {
        flightAngle_ = world_->flightAngle();
        flightAltitude_ = world_->flightAltitude();
    }
    if (world_->takeEntrance(&target)) {
        flightSaved_ = onDeck;
        airshipX_ = world_->airshipX();
        airshipY_ = world_->airshipY();
        resumePending_ = world_->takeResume(&resumeOffset_);
        leaving_ = true;
        leaveTarget_ = target;
        startFade(false, FF6_BRIGHTNESS_STEP);
        return;
    }
    if (worldScript_ && !world_->storyScriptRunning()) {  // the script of the story is over, and so is the event
        worldScript_ = false;
        event_.stop();
    }
    int group = 0, background = 0;
    if (world_->takeBattle(&group, &background)) launchBattle(group, background, encounters_.lastOnVeldt());
    if (world_->takeScriptBattle(&group, &background)) {
        launchBattle(battleData_.eventBattle(group, frame_ % 60 >= FF6_VEHICLE_BATTLE_SECOND_FRAME ? 1 : 0), background);
    }
    if (world_->takeDoomGaze()) {
        launchBattle(battleData_.eventBattle(FF6_DOOM_GAZE_BATTLE_GROUP, 0), FF6_BATTLE_BG_AIRSHIP_WOR);
        doomGazeBattle_ = battleOpen_;
    }
}

// FadeIn and FadeOut: a fade in starts from one step of brightness, a fade out from full brightness.
void Game::startFade(bool in, int speed) {
    fade_ = in ? speed : (speed | FF6_FADE_OUT_FLAG);
    brightness_ = in ? FF6_BRIGHTNESS_STEP : FF6_BRIGHTNESS_FULL;
}

// UpdateBrightness, every frame: the fades move the brightness, and a fade ends when it gets to its end.
void Game::updateBrightness() {
    if (fade_ & FF6_FADE_OUT_FLAG) {
        if (brightness_ == 0) {
            fade_ = 0;
        } else {
            brightness_ = (brightness_ - (fade_ & FF6_FADE_SPEED_MASK)) & 0xFF;
        }
    } else if ((brightness_ & FF6_BRIGHTNESS_FULL) == FF6_BRIGHTNESS_FULL) {
        fade_ = 0;
    } else {
        brightness_ = (brightness_ + (fade_ & FF6_FADE_SPEED_MASK)) & 0xFF;
    }
}

void Game::finishFade() {
    brightness_ = FF6_BRIGHTNESS_FULL;
    fade_ = 0;
    fadeInPending_ = false;
}

// The brightness of the PPU: the colors are scaled by the level plus one, out of 16.
// What the events do to the whole picture: the mosaic makes blocks of the pixel at their corner, and the flashlight leaves
// only a circle around the lead (outside the window of the PPU, the screen is black).
void Game::applyStage(std::vector<uint8_t>& image) {
    const int block = stage_.mosaicSize();
    if (block > 1) {
        for (int y = 0; y < FF6_SCREEN_HEIGHT; ++y) {
            for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
                const size_t from = (static_cast<size_t>(y / block * block) * FF6_SCREEN_WIDTH + static_cast<size_t>(x / block * block)) * FF6_BYTES_PER_PIXEL;
                const size_t to = (static_cast<size_t>(y) * FF6_SCREEN_WIDTH + static_cast<size_t>(x)) * FF6_BYTES_PER_PIXEL;
                for (size_t c = 0; c < FF6_BYTES_PER_PIXEL; ++c) image[to + c] = image[from + c];
            }
        }
    }
    if (stage_.pyramidOn()) {
        const Npc* object = objects_.object(stage_.pyramidObject());
        int corners[3][2];
        stage_.pyramidCorners(corners);
        const int centerX = object ? object->pixelX() - scroll_.x(FF6_BG1) : FF6_SCREEN_WIDTH / 2;
        const int centerY = object ? object->pixelY() - scroll_.y(FF6_BG1) : FF6_SCREEN_HEIGHT / 2;
        const auto side = [&](int a, int b, int x, int y) {
            return (corners[b][0] - corners[a][0]) * (y - centerY - corners[a][1]) - (corners[b][1] - corners[a][1]) * (x - centerX - corners[a][0]);
        };
        for (int y = 0; y < FF6_SCREEN_HEIGHT; ++y) {
            for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
                const int a = side(0, 1, x, y), b = side(1, 2, x, y), c = side(2, 0, x, y);
                if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) continue;
                uint8_t* pixel = &image[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + static_cast<size_t>(x)) * FF6_BYTES_PER_PIXEL];
                pixel[0] = pixel[1] = pixel[2] = 0;
            }
        }
    }
    const int radius = stage_.flashlightRadius();
    if (radius >= 0) {
        const Npc* lead = objects_.lead();
        const int centerX = (lead ? lead->pixelX() : player_.pixelX()) - scroll_.x(FF6_BG1);
        const int centerY = (lead ? lead->pixelY() : player_.pixelY()) - scroll_.y(FF6_BG1) - FF6_FLASHLIGHT_LIFT;
        for (int y = 0; y < FF6_SCREEN_HEIGHT; ++y) {
            for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
                if ((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY) <= radius * radius) continue;
                uint8_t* pixel = &image[(static_cast<size_t>(y) * FF6_SCREEN_WIDTH + static_cast<size_t>(x)) * FF6_BYTES_PER_PIXEL];
                pixel[0] = pixel[1] = pixel[2] = 0;
            }
        }
    }
}

void Game::applyBrightness(std::vector<uint8_t>& image) const {
    const int level = brightness_ >> FF6_BRIGHTNESS_SHIFT;
    if (level >= FF6_BRIGHTNESS_LEVELS - 1) return;
    for (size_t i = 0; i < image.size(); i += FF6_BYTES_PER_PIXEL) {
        for (size_t c = 0; c < FF6_ALPHA_OFFSET; ++c) image[i + c] = static_cast<uint8_t>(image[i + c] * (level + 1) / FF6_BRIGHTNESS_LEVELS);
    }
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

// Event commands 0xDE to 0xE4: which characters are where, as bits (the number of the party for 0xE4).
unsigned Game::characterMask(uint8_t op) const {
    if (op == FF6_EVENT_GET_PARTY_NUMBER) return 1u << objects_.activeParty();
    if (op == FF6_EVENT_GET_LEAD_CHAR) {
        for (const int character : objects_.partySlots()) {
            if (character >= 0) return 1u << character;
        }
        return 1;
    }
    unsigned mask = 0;
    for (size_t i = 0; i < objects_.characters().size(); ++i) {
        const Npc& character = objects_.characters()[i];
        const bool match = op == FF6_EVENT_GET_PARTY_CHARS ? character.party == objects_.activeParty()
                           : op == FF6_EVENT_GET_ENABLED_CHARS ? character.enabled
                                                               : character.party != 0;
        if (match) mask |= 1u << i;
    }
    return mask;
}

// The commands that stage a scene: the colors of the map and of the sprites, the fixed color, the shake, the scrolls, the
// filter, the mosaic, the flashlight, the animated tiles and the default party. False for the other commands.
bool Game::stageCommand(uint8_t op, const uint8_t* bytes, size_t size) {
    const auto byteAt = [&](size_t n) { return n < size ? static_cast<int>(bytes[n]) : 0; };
    // A scroll speed is a byte of 1/16 pixel for each frame; the commands 0x5D to 0x5F round a negative one the other way.
    const auto speed = [&](int value, bool exact) {
        const int signedValue = static_cast<int8_t>(value);
        return exact || signedValue >= 0 ? signedValue * 16 : (signedValue + 1) * 16 - 1;
    };
    switch (op) {
        case FF6_EVENT_BG_COLORS: modifyColors(byteAt(0), map_.palette.data(), bgBase_.data(), FF6_EVENT_BG_COLORS_FIRST, FF6_EVENT_BG_COLORS_LAST); return true;
        case FF6_EVENT_BG_COLOR_RANGE: modifyColors(byteAt(0), map_.palette.data(), bgBase_.data(), byteAt(1), byteAt(2) + 1); return true;
        case FF6_EVENT_OBJ_COLORS:
        case FF6_EVENT_OBJ_COLOR_RANGE:
            if (op == FF6_EVENT_OBJ_COLORS) {
                modifyColors(byteAt(0), spriteColors_.data(), spriteBase_.data(), 0, FF6_MAP_PALETTE_COLORS);
            } else {
                modifyColors(byteAt(0), spriteColors_.data(), spriteBase_.data(), byteAt(1), byteAt(2) + 1);
            }
            spritePaletteChanged();
            return true;
        case FF6_EVENT_FIXED_OFF: stage_.fixedOff(); return true;
        case FF6_EVENT_FLASH: stage_.flash(byteAt(0)); return true;
        case FF6_EVENT_FIXED_ADD: stage_.fixedColor(false, byteAt(0)); return true;
        case FF6_EVENT_FIXED_SUB: stage_.fixedColor(true, byteAt(0)); return true;
        case FF6_EVENT_SHAKE: stage_.shake(byteAt(0)); return true;
        case FF6_EVENT_SCROLL_BG1:
        case FF6_EVENT_SCROLL_BG2:
        case FF6_EVENT_SCROLL_BG3:
        case FF6_EVENT_SCROLL_BG1_EXACT:
        case FF6_EVENT_SCROLL_BG2_EXACT:
        case FF6_EVENT_SCROLL_BG3_EXACT: {
            const bool exact = op >= FF6_EVENT_SCROLL_BG1_EXACT;
            const int layer = (exact ? op - FF6_EVENT_SCROLL_BG1_EXACT : op - FF6_EVENT_SCROLL_BG1);
            stage_.scrollSpeed(layer, speed(byteAt(0), exact), speed(byteAt(1), exact));
            return true;
        }
        case FF6_EVENT_FILTER_COLORS: {  // the colors of a range become the gray of the colors that the filter keeps
            static constexpr uint16_t kFilters[8] = {0x0000, 0x001F, 0x03E0, 0x03FF, 0x7C00, 0x7C1F, 0x7FE0, 0x7FFF};
            for (int i = byteAt(1); i <= byteAt(2) && i < FF6_MAP_PALETTE_COLORS; ++i) {
                const uint16_t color = bgBase_[static_cast<size_t>(i)];
                const int gray = ((color & 31) + ((color >> 5) & 31) + ((color >> 10) & 31)) / 3;
                map_.palette[static_cast<size_t>(i)] = static_cast<uint16_t>((gray | (gray << 5) | (gray << 10)) & kFilters[byteAt(0) & 7]);
            }
            return true;
        }
        case FF6_EVENT_MOSAIC: stage_.mosaic(byteAt(0)); return true;
        case FF6_EVENT_FLASHLIGHT: stage_.flashlight(byteAt(0)); return true;
        case FF6_EVENT_BG_ANIM_FRAME: anim_.setFrame(static_cast<size_t>(byteAt(0)), byteAt(1)); return true;
        case FF6_EVENT_BG_ANIM_SPEED: anim_.setSpeed(static_cast<size_t>(byteAt(0)), static_cast<int8_t>(byteAt(1)) * 16); return true;
        case FF6_EVENT_PYRAMID_OFF: stage_.pyramid(false, 0); return true;
        case FF6_EVENT_PYRAMID_ON: stage_.pyramid(true, byteAt(0)); return true;
        case FF6_EVENT_OPTIMIZE_EQUIP: optimizeEquipment(state_, rules_, byteAt(0)); return true;
        case FF6_EVENT_PORTRAIT: portrait_ = byteAt(0); return true;
        case FF6_EVENT_RESET_PARTY: defaultParty_ = 1; return true;
        case FF6_EVENT_RESTORE_PARTY:
            if (objects_.activeParty() != defaultParty_) objects_.activateParty(defaultParty_);
            return true;
        default: return false;
    }
}

void Game::fieldCommand(uint8_t op, const uint8_t* bytes, size_t size) {
    if (stageCommand(op, bytes, size)) return;
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
                resetSpriteColors();
            }
            break;
        case FF6_EVENT_PARTY_MAP:
            objects_.setPartyMap(byteAt(0) & (FF6_PARTY_COUNT - 1),
                                 (byteAt(1) | (byteAt(2) << 8)) & FF6_ENTRANCE_MAP_MASK);
            break;
        case FF6_EVENT_MOVE_PARTY:
            moveParty(byteAt(0), byteAt(1));
            break;
        case FF6_EVENT_SET_PARENT_MAP: {
            const int word = byteAt(0) | (byteAt(1) << 8);
            hasParent_ = true;
            parentMap_ = static_cast<uint32_t>(word & FF6_ENTRANCE_MAP_MASK);
            parentX_ = byteAt(2);
            parentY_ = byteAt(3);
            parentFacing_ = (word >> FF6_ENTRANCE_FACING_SHIFT) & FF6_NPC_DIR_MASK;
            break;
        }
        case FF6_EVENT_SET_CHAR_PROP:
            setCharacterProperties(state_, rules_, byteAt(0), byteAt(1), availableCharacters());
            break;
        case FF6_EVENT_NORMALIZE_LEVEL:
            normalizeLevel(state_, rules_, byteAt(0), availableCharacters());
            break;
        case FF6_EVENT_GIVE_ITEM:
            giveItem(state_, byteAt(0));
            break;
        case FF6_EVENT_TAKE_ITEM:
            takeItem(state_, byteAt(0));
            break;
        case FF6_EVENT_GIVE_GIL:
            giveGil(state_, byteAt(0) | (byteAt(1) << 8));
            break;
        case FF6_EVENT_TAKE_GIL:
            bits_.set(FF6_EVENT_BIT_NOT_ENOUGH_GIL, !takeGil(state_, byteAt(0) | (byteAt(1) << 8)));
            break;
        case FF6_EVENT_AND_STATUS:
        case FF6_EVENT_SET_STATUS:
        case FF6_EVENT_TOGGLE_STATUS: {  // the word is the first status byte, then the fourth one (Float and Interceptor)
            const unsigned word = static_cast<unsigned>(byteAt(1) | (byteAt(2) << 8));
            changeStatus(state_, byteAt(0), op, (word & 0xFF) | ((word & 0xFF00) << 16));
            break;
        }
        case FF6_EVENT_CHANGE_HP:
            changeHp(state_, rules_, byteAt(0), byteAt(1));
            break;
        case FF6_EVENT_CHANGE_MP:
            changeMp(state_, rules_, byteAt(0), byteAt(1));
            break;
        case FF6_EVENT_REMOVE_EQUIPMENT:
            removeEquipment(state_, byteAt(0));
            break;
        case FF6_EVENT_SHOP:
            openShop(byteAt(0));
            break;
        case FF6_EVENT_COLOSSEUM_MENU:
            openColosseum();
            break;
        case FF6_EVENT_FINAL_ORDER: {  // the twelve that can fight at the end are the ones that have joined and are in a party
            std::vector<int> characters;
            for (int i = 0; i < FF6_FINAL_ORDER_CHARACTERS; ++i) {
                const Npc& character = objects_.characters()[static_cast<size_t>(i)];
                if (bits_.get(FF6_EVENT_BIT_CHAR_AVAILABLE + i) && character.party != 0) characters.push_back(i);
            }
            finalOrderMenu_.open(characters);
            finalOrderOpen_ = true;
            menuPhase_ = MenuPhase::FadingOut;
            startFade(false, FF6_BRIGHTNESS_STEP);
            break;
        }
        case FF6_EVENT_NAME_MENU:
            nameMenu_.open(byteAt(0), characterName(byteAt(0)));
            nameOpen_ = true;
            menuPhase_ = MenuPhase::FadingOut;
            startFade(false, FF6_BRIGHTNESS_STEP);
            break;
        case FF6_EVENT_COLOSSEUM_BATTLE:
            startColosseumBattle();
            break;
        case FF6_EVENT_PARTY_SELECT:
            openPartySelect(byteAt(0), static_cast<unsigned>(byteAt(1) | (byteAt(2) << 8)));
            break;
        case FF6_EVENT_BATTLE:
        case FF6_EVENT_BATTLE_COLLISION:
            startBattle(byteAt(0), byteAt(1));
            break;
        case FF6_EVENT_BOX_BATTLE:
            startBattle(boxGroup_, FF6_BATTLE_BG_DEFAULT);
            break;
        case FF6_EVENT_RANDOM_BATTLE:
            beginBattle(pendingBattle_, FF6_BATTLE_BG_DEFAULT);
            break;
        case FF6_EVENT_GIVE_ESPER:
            giveEsper(state_, byteAt(0));
            break;
        case FF6_EVENT_TAKE_ESPER:
            takeEsper(state_, byteAt(0));
            break;
        case FF6_EVENT_GIVE_SWDTECHS:
            state_.swordtechs = (1u << FF6_TECH_COUNT) - 1;
            break;
        case FF6_EVENT_GIVE_BUM_RUSH:
            state_.blitzes |= FF6_BUM_RUSH_BIT;
            break;
        case FF6_EVENT_MAP_DATA_NOW:
        case FF6_EVENT_MAP_DATA:  // x, layer and y, width, height, then the tiles
            changeMapData(bytes, size);
            break;
        case FF6_EVENT_FADE_IN:
            startFade(true, FF6_BRIGHTNESS_STEP);
            break;
        case FF6_EVENT_FADE_OUT:
            startFade(false, FF6_BRIGHTNESS_STEP);
            break;
        case FF6_EVENT_FADE_IN_SPEED:
            startFade(true, byteAt(0));
            break;
        case FF6_EVENT_FADE_OUT_SPEED:
            startFade(false, byteAt(0));
            break;
        case FF6_EVENT_FADE_STOP:
            fade_ = 0;
            break;
        case FF6_EVENT_TITLE_SCREEN:
        case FF6_EVENT_OPENING:
            startCutscene(op == FF6_EVENT_OPENING);
            break;
        case FF6_EVENT_TRAIN:
            startTrain();
            break;
        case FF6_EVENT_WORLD_OF_RUIN:
            if (!titleDataLoaded_) {
                titleData_ = loadTitleData(packDir_);
                titleDataLoaded_ = true;
            }
            cutscene_ = std::make_unique<RuinScene>(titleData_, sound_);
            brightness_ = 0;
            fade_ = 0;
            fadeInPending_ = false;
            break;
        case FF6_EVENT_FLOATING_CONTINENT:
            if (!titleDataLoaded_) {
                titleData_ = loadTitleData(packDir_);
                titleDataLoaded_ = true;
            }
            cutscene_ = std::make_unique<FloatingScene>(titleData_, sound_);
            brightness_ = 0;
            fade_ = 0;
            fadeInPending_ = false;
            break;
        case FF6_EVENT_ENDING: {
            std::vector<std::string> names;
            for (const int id : endingCharacters(byteAt(0))) names.push_back(characterName(id));
            const int frames = endingFrames(byteAt(0));
            if (!titleDataLoaded_) {
                titleData_ = loadTitleData(packDir_);
                titleDataLoaded_ = true;
            }
            cutscene_ = std::make_unique<EndingCard>(menuScreen_, names, frames > 0 ? frames : FF6_CARD_FRAMES,
                                                     creditsPages(creditsScene(byteAt(0)), titleData_.credits),
                                                     endingCharacters(byteAt(0)), makeFlightBackdrop(titleData_, byteAt(0)), makeEndingNames(titleData_, byteAt(0)));
            brightness_ = 0;
            fade_ = 0;
            fadeInPending_ = false;
            break;
        }
        case FF6_EVENT_ENDING_AIRSHIP:
            break;  // the loop of the Falcon scene ends at its first frame, with the screen blank
        case FF6_EVENT_THE_END:
            if (!titleDataLoaded_) {
                titleData_ = loadTitleData(packDir_);
                titleDataLoaded_ = true;
            }
            cutscene_ = std::make_unique<TheEndScene>(titleData_, sound_);
            cardToTitle_ = true;
            brightness_ = 0;
            fade_ = 0;
            fadeInPending_ = false;
            break;
        case FF6_EVENT_LOAD_MENU:
            openLoadMenu();
            break;
        case FF6_EVENT_RESTORE_GAME:
            restoreRequested_ = true;
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
        const bool toWorld = parentMap_ < FF6_FIRST_FIELD_MAP;  // the party comes out on the tile of the world map that it entered from
        const int facing = toWorld ? parentFacing_ : (parentFacing_ + 2) % FF6_DIRECTION_COUNT;  // step back out of the door
        const int stepX = toWorld ? 0 : kDeltaX[facing], stepY = toWorld ? 0 : kDeltaY[facing];
        if (toWorld && flightSaved_) pendingVehicle_ = -1;  // back from the deck: the airship still flies
        if (loadMapNow(parentMap_, parentX_ + stepX, parentY_ + stepY, facing, true, true)) hasParent_ = false;
        pendingVehicle_ = 0;
        return;
    }
    const bool hadParent = hasParent_;
    const uint32_t previousMap = parentMap_;
    const int previousX = parentX_, previousY = parentY_, previousFacing = parentFacing_;
    if (target.setParent) {
        hasParent_ = true;
        parentMap_ = world_ ? static_cast<uint32_t>(world_->world()) : map_.id;
        parentX_ = partyTileX();
        parentY_ = partyTileY();
        parentFacing_ = world_ ? world_->facing() : player_.facing();
    }
    if (!loadMapNow(target.map, target.x, target.y, target.facing, true, true) && target.setParent) {
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
        takeStep();
        for (size_t i = 0; i + FF6_EVENT_TRIGGER_SIZE <= map_.eventTriggers.size(); i += FF6_EVENT_TRIGGER_SIZE) {
            const EventTrigger trigger = decodeEventTrigger(&map_.eventTriggers[i]);
            if (trigger.x == x && trigger.y == y) {
                startEvent(trigger.offset);
                return;
            }
        }
        checkEncounter();
        if (event_.running()) return;
    }
    EntranceTarget target;
    if (armed_ && checkEntrances(map_, x, y, &target) && !leaving_) {  // the screen fades out, then the map is loaded
        leaving_ = true;
        leaveTarget_ = target;
        startFade(false, FF6_BRIGHTNESS_STEP);
    }
}

// Port of CheckNPCs: the confirm button talks to the npc that the party faces.
bool Game::talkToNpc() {
    Npc* npc = objects_.npcFacing(map_, player_.tileX(), player_.tileY(), player_.facing());
    if (!npc) return false;
    objects_.activate(*npc, player_.facing());
    startEvent(npc->eventOffset);
    return true;
}

// Port of CheckTreasure: the confirm button opens the chest or the pot that the party faces, once. Monster-in-a-box
// chests stay shut, since there are no battles.
bool Game::openTreasure() {
    static constexpr int kStepX[FF6_DIRECTION_COUNT] = {0, 1, 0, -1}, kStepY[FF6_DIRECTION_COUNT] = {-1, 0, 1, 0};
    const int x = player_.tileX() + kStepX[player_.facing()], y = player_.tileY() + kStepY[player_.facing()];
    MapLayout& layout = map_.layers[FF6_BG1];
    if (x < 0 || y < 0 || x >= layout.width || y >= layout.height) return false;
    uint8_t& tile = layout.cells[static_cast<size_t>(y) * layout.width + x];
    if (!(map_.tileProps[tile] & FF6_TILE_TREASURE)) return false;
    for (size_t i = 0; i + FF6_TREASURE_PROP_SIZE <= map_.treasures.size(); i += FF6_TREASURE_PROP_SIZE) {
        const uint8_t* chest = &map_.treasures[i];
        if (chest[0] != x || chest[1] != y) continue;
        const int kind = chest[2] | (chest[3] << 8), contents = chest[4];
        const size_t bit = static_cast<size_t>(kind & FF6_TREASURE_BIT_MASK);
        if (state_.treasures[bit]) return false;
        state_.treasures[bit] = true;
        uint32_t script = FF6_EVENT_SCRIPT_TREASURE_EMPTY;
        if (kind & FF6_TREASURE_KIND_GIL) {
            foundGil_ = static_cast<long>(contents) * FF6_TREASURE_GIL_UNIT;
            giveGil(state_, foundGil_);
            script = FF6_EVENT_SCRIPT_TREASURE_GIL;
        } else if (kind & FF6_TREASURE_KIND_ITEM) {
            receivedItem_ = contents;
            giveItem(state_, contents);
            script = FF6_EVENT_SCRIPT_TREASURE_ITEM;
        } else if (kind & FF6_TREASURE_KIND_MONSTER) {
            boxGroup_ = contents;
            script = FF6_EVENT_SCRIPT_TREASURE_MONSTER;
        }
        startEvent(script);
        if (tile == FF6_TILE_CHEST_CLOSED) {
            tile = FF6_TILE_CHEST_OPEN;
            sound_.command(FF6_SOUND_GAME_SFX, FF6_SFX_TREASURE_CHEST, FF6_SFX_PAN_CENTER);
        } else {
            sound_.command(FF6_SOUND_GAME_SFX, FF6_SFX_TREASURE_POT, FF6_SFX_PAN_CENTER);
        }
        return true;
    }
    return false;
}

// InitTreasures: the chests that were opened before are open when the map is loaded.
void Game::showOpenedTreasures() {
    MapLayout& layout = map_.layers[FF6_BG1];
    for (size_t i = 0; i + FF6_TREASURE_PROP_SIZE <= map_.treasures.size(); i += FF6_TREASURE_PROP_SIZE) {
        const uint8_t* chest = &map_.treasures[i];
        const int x = chest[0], y = chest[1];
        if (x >= layout.width || y >= layout.height) continue;
        uint8_t& tile = layout.cells[static_cast<size_t>(y) * layout.width + x];
        if (state_.treasures[static_cast<size_t>((chest[2] | (chest[3] << 8)) & FF6_TREASURE_BIT_MASK)] && tile == FF6_TILE_CHEST_CLOSED) {
            tile = FF6_TILE_CHEST_OPEN;
        }
    }
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
    if (cutscene_ && menuPhase_ == MenuPhase::Closed) {
        cutscene_->tick(input);
        if (train_) {
            int group = 0, background = 0;
            bool second = false;
            if (train_->takeBattle(&group, &second, &background)) {
                trainBattle_ = true;
                launchBattle(battleData_.eventBattle(group, second ? 1 : 0), background);
            }
        }
        if (cutscene_->finished()) endCutscene();
        return;
    }
    if (menuPhase_ != MenuPhase::Closed) {
        bits_.tickTimers(true);
        tickMenu(input);
        return;
    }
    if (world_) {
        tickWorld(input);
        return;
    }
    anim_.update(map_, frame_);
    stage_.update(static_cast<int>(frame_));
    for (int layer = 0; layer < FF6_LAYER_COUNT; ++layer) scroll_.drift(layer, stage_.speedX(layer), stage_.speedY(layer));
    updateBrightness();
    bits_.tickTimers(false);
    if (leaving_ && !fading()) {
        leaving_ = false;
        leaveThroughEntrance(leaveTarget_);
    }
    // The press that closes a dialogue or ends an event must not also start a new conversation.
    const bool wasBusy = dialogue_.active() || event_.running();
    if (dialogue_.active()) dialogue_.update(input.confirm, input.direction);
    event_.update(*this);
    if (restoreRequested_) {
        restoreRequested_ = false;
        restoreSavedGame();
        return;
    }
    if (fadeInPending_ && !event_.running()) {
        fadeInPending_ = false;
        startFade(true, FF6_BRIGHTNESS_STEP);
    }
    uint32_t timerEvent = 0;  // a timer that ran out starts its event once the party stands still and no event runs
    if (!event_.running() && !dialogue_.active() && !player_.moving() && !leaving_ && bits_.takeTimerEvent(&timerEvent)) {
        startEvent(timerEvent);
    }
    Npc* lead = objects_.lead();
    const bool eventRunning = event_.running();
    if (eventWasRunning_ && !eventRunning) {
        restorePending_ = true;
        foundGil_ = -1;
        receivedItem_ = -1;
    }
    eventWasRunning_ = eventRunning;
    // Once the event is over and the lead has finished its own script, it gets its movement back.
    if (restorePending_ && lead && lead->movement != FF6_OBJ_MOVE_SCRIPT && lead->aligned() && !player_.moving()) {
        lead->movement = leadSavedMovement_;
        restorePending_ = false;
        player_.place(map_, lead->tileX(), lead->tileY(), lead->facing);  // settles the priority of the tile
    }
    const bool userLead = !lead || lead->movement == FF6_OBJ_MOVE_USER;
    const bool playerActs = (!eventRunning && !dialogue_.active() && userLead) || player_.moving();
    if (!eventRunning && !dialogue_.active() && userLead && !leaving_) {
        if (input.confirm && !wasBusy && !player_.moving() && brightness_ == FF6_BRIGHTNESS_FULL) {
            if (!talkToNpc()) openTreasure();
        } else if (input.menu && !wasBusy && !player_.moving() && brightness_ == FF6_BRIGHTNESS_FULL) {
            menuPhase_ = MenuPhase::FadingOut;
            startFade(false, FF6_BRIGHTNESS_STEP);
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
    if (world_) {
        world_->enter(world_->world(), tileX, tileY, world_->facing(), worldLead());
        return;
    }
    player_.place(map_, tileX, tileY, player_.facing());
    objects_.placeParty(tileX, tileY);
    if (Npc* lead = objects_.lead()) copyPlayerToLead(*lead);
    lastTileX_ = tileX;
    lastTileY_ = tileY;
    armed_ = false;
    followScroll();
}

// Event command 0x7E: the same map is loaded again with the party elsewhere; the npcs, the doors and the music stay.
void Game::moveParty(int tileX, int tileY) {
    placeParty(tileX, tileY);
    scroll_.init();
    followScroll();
    brightness_ = 0;
    fadeInPending_ = false;
    startFade(true, FF6_BRIGHTNESS_STEP);
}

// The characters that have joined, for the average level.
CharacterMask Game::availableCharacters() const {
    CharacterMask mask = 0;
    for (int i = 0; i < FF6_PLAYABLE_COUNT; ++i) {
        if (bits_.get(FF6_EVENT_BIT_CHAR_AVAILABLE + i)) mask |= 1u << i;
    }
    return mask;
}

// The character as the menus show it, with the equipment that it has in `state`.
MenuCharacter Game::menuCharacter(int id, const CharacterState& state) const {
    const CharacterStats stats = calcStats(rules_, state);
    MenuCharacter character;
    character.id = id;
    character.level = state.level;
    character.hp = std::min(state.hp, stats.maxHp);
    character.maxHp = stats.maxHp;
    character.mp = std::min(state.mp, stats.maxMp);
    character.maxMp = stats.maxMp;
    character.vigor = stats.vigor;
    character.speed = stats.speed;
    character.stamina = stats.stamina;
    character.magicPower = stats.magicPower;
    character.attackPower = stats.attackPower;
    character.defense = stats.defense;
    character.magicDefense = stats.magicDefense;
    character.evade = stats.evade;
    character.magicEvade = stats.magicBlock;
    character.experience = state.experience;
    character.nextLevel = experienceToNextLevel(rules_, state);
    for (size_t i = 0; i < state.equipment.size(); ++i) character.equipment[i] = state.equipment[i];
    for (int esper = 0; esper < FF6_ESPER_COUNT; ++esper) {
        if (!state_.espers[static_cast<size_t>(esper)]) continue;
        character.skills[0].push_back(menuData_.esperName(esper));
        character.espers.push_back(esper);
    }
    character.equippedEsper = state.esper;
    for (int spell = 0; spell < FF6_SPELL_COUNT; ++spell) {
        if (state_.spells[static_cast<size_t>(id)][static_cast<size_t>(spell)]) {
            // The name, then the cost in mp: two digits, the first of them blank when it is 0.
            std::vector<uint8_t> entry = menuData_.spellName(spell);
            const int cost = rules_.attack(spell)[FF6_MAGIC_PROP_MP];
            entry.push_back(FF6_MENU_CODE_SPACE);
            entry.push_back(cost >= 10 ? static_cast<uint8_t>(FF6_MENU_CODE_ZERO + cost / 10 % 10) : FF6_MENU_CODE_SPACE);
            entry.push_back(static_cast<uint8_t>(FF6_MENU_CODE_ZERO + cost % 10));
            character.skills[1].push_back(entry);
        }
    }
    for (int tech = 0; tech < FF6_TECH_COUNT; ++tech) {
        if (state.actor == FF6_ACTOR_CYAN && (state_.swordtechs >> tech & 1)) character.skills[2].push_back(menuData_.bushidoName(tech));
        if (state.actor == FF6_ACTOR_SABIN && (state_.blitzes >> tech & 1)) character.skills[3].push_back(menuData_.blitzName(tech));
    }
    for (int lore = 0; lore < FF6_LORE_COUNT; ++lore) {
        if (state.actor != FF6_ACTOR_STRAGO || !(state_.lores >> lore & 1)) continue;
        std::vector<uint8_t> name = battleData_.attackName(FF6_ATTACK_FIRST_LORE + lore);
        name.erase(std::find(name.begin(), name.end(), FF6_BATTLE_NAME_PAD_CODE), name.end());
        character.skills[4].push_back(name);
    }
    if (state.actor == FF6_ACTOR_GAU) {  // the rages that he knows, by the name of the monster
        for (int monster = 0; monster < FF6_RAGE_MONSTERS; ++monster) {
            if (!state_.rages[static_cast<size_t>(monster)]) continue;
            std::vector<uint8_t> name = battleData_.name(monster);
            name.erase(std::find(name.begin(), name.end(), FF6_BATTLE_NAME_PAD_CODE), name.end());
            character.skills[5].push_back(name);
        }
    }
    if (state.actor == FF6_ACTOR_MOG) {  // and the dances of Mog
        static constexpr const char* kDances[FF6_DANCE_COUNT] = {"Wind Song", "Forest Suite", "Desert Aria", "Love Sonata",
                                                                 "Earth Blues", "Water Rondo", "Dusk Requiem", "Snowman Jazz"};
        for (int dance = 0; dance < FF6_DANCE_COUNT; ++dance) {
            if (!(state_.dances >> dance & 1)) continue;
            std::vector<uint8_t> name;
            for (const char letter : std::string(kDances[dance])) name.push_back(menuCode(letter));
            character.skills[6].push_back(name);
        }
    }
    return character;
}

std::vector<MenuItem> Game::inventoryItems() const {
    std::vector<MenuItem> items;
    for (const InventorySlot& slot : state_.inventory) {
        if (slot.item != FF6_ITEM_EMPTY) items.push_back({slot.item, slot.quantity});
    }
    return items;
}

std::vector<MenuCharacter> Game::partyCharacters() const {
    std::vector<MenuCharacter> party;
    for (const int id : objects_.partySlots()) {
        if (id >= 0) party.push_back(menuCharacter(id, state_.characters[static_cast<size_t>(id)]));
    }
    return party;
}

// The menu keeps hooks into the game: the equipment screens list, preview and change what a character wears.
MenuStatus Game::menuStatus() {
    MenuStatus status;
    status.party = partyCharacters();
    status.items = inventoryItems();
    status.item.items = [this] { return inventoryItems(); };
    status.item.party = [this] { return partyCharacters(); };
    status.item.usable = [this](int item) { return itemUsableInMenu(rules_, item); };
    status.item.canUse = [this](int item, int id) { return canUseItemOn(state_, rules_, id, item); };
    status.item.use = [this](int item, int id) { return useItemOn(state_, rules_, id, item); };
    status.equip.choices = [this](int id, int slot) { return equippableItems(state_, rules_, id, slot); };
    status.equip.preview = [this](int id, int slot, int item) {
        CharacterState state = state_.characters[static_cast<size_t>(id)];
        state.equipment[static_cast<size_t>(slot)] = item;
        return menuCharacter(id, state);
    };
    status.equip.equip = [this](int id, int slot, int item) {
        equipItem(state_, rules_, id, slot, item);
        return menuCharacter(id, state_.characters[static_cast<size_t>(id)]);
    };
    status.equip.equipEsper = [this](int id, int esper) {
        const bool let = state_.characters[static_cast<size_t>(id)].esper == esper;
        for (CharacterState& other : state_.characters) {
            if (other.esper == esper) other.esper = -1;  // an esper is carried by one character
        }
        if (!let) state_.characters[static_cast<size_t>(id)].esper = esper;
        return menuCharacter(id, state_.characters[static_cast<size_t>(id)]);
    };
    status.gil = state_.gil;
    status.steps = state_.steps;
    status.hours = static_cast<int>(std::min<uint32_t>(frame_ / FF6_MENU_FRAMES_PER_HOUR, FF6_MENU_MAX_HOURS));
    status.minutes = static_cast<int>(frame_ / FF6_MENU_FRAMES_PER_MINUTE % FF6_MENU_MINUTES_PER_HOUR);
    return status;
}

// Event commands 0xA9 and 0xAA: the title screen or the opening takes the screen, and the event goes on once it is over,
// with a bit that tells whether a press of the button ended it.
void Game::startCutscene(bool opening) {
    if (!titleDataLoaded_) {
        titleData_ = loadTitleData(packDir_);
        titleDataLoaded_ = true;
    }
    if (opening) {
        cutscene_ = std::make_unique<OpeningScene>(titleData_, sound_);
    } else {
        cutscene_ = std::make_unique<TitleScene>(titleData_, sound_);
    }
    brightness_ = 0;
    fade_ = 0;
    fadeInPending_ = false;
}

namespace {

// A cinematic that is not ported takes the screen for a while: black, with the lines of text that it has, until its time is up
// or (with no time) the button is pressed.
class CardScene : public Cutscene {
public:
    CardScene(MenuScreen& screen, std::vector<std::string> lines, int frames) : screen_(screen), lines_(std::move(lines)), frames_(frames) {}
    void tick(const InputState& input) override {
        ++frame_;
        if (frames_ > 0 ? frame_ >= frames_ : (input.confirm && frame_ >= FF6_CARD_MIN_FRAMES)) done_ = true;
    }
    void draw(uint8_t* rgba) override {
        screen_.clear();
        screen_.setTextOffset(0);
        int row = FF6_SCREEN_HEIGHT / FF6_MENU_TILE_SIZE / 2 - static_cast<int>(lines_.size()) / 2;
        for (const std::string& line : lines_) {
            screen_.text((FF6_SCREEN_WIDTH / FF6_MENU_TILE_SIZE - static_cast<int>(line.size())) / 2, row++, line, MenuScreen::Layer::Bg3, FF6_MENU_BG3_DEFAULT);
        }
        std::copy(screen_.pixels().begin(), screen_.pixels().end(), rgba);
    }
    bool finished() const override { return done_; }
    bool skipped() const override { return false; }

private:
    MenuScreen& screen_;
    std::vector<std::string> lines_;
    int frames_ = 0, frame_ = 0;
    bool done_ = false;
};

}  // namespace

void Game::startCard(const std::vector<std::string>& lines, int frames, bool toTitle) {
    cutscene_ = std::make_unique<CardScene>(menuScreen_, lines, frames);
    cardToTitle_ = toTitle;
    brightness_ = 0;
    fade_ = 0;
    fadeInPending_ = false;
}

// Event command 0xAE: the ride of the magitek train, which asks for battles and gives the event back when the way ends.
void Game::startTrain() {
    if (!worldDataLoaded_) {
        worldData_ = loadWorldData(packDir_);
        worldDataLoaded_ = true;
    }
    auto ride = std::make_unique<TrainRide>(worldData_.train, battleData_.rngTable, doomGazeCounter_, sound_);
    train_ = ride.get();
    cutscene_ = std::move(ride);
    brightness_ = 0;
    fade_ = 0;
    fadeInPending_ = false;
}

void Game::endCutscene() {
    if (train_) {
        doomGazeCounter_ = train_->counter();
        train_ = nullptr;
    } else {
        bits_.set(FF6_EVENT_BIT_SKIPPED, cutscene_->skipped());
    }
    cutscene_.reset();
    brightness_ = 0;
    fade_ = 0;
    if (cardToTitle_) {  // after "THE END" the game starts over
        cardToTitle_ = false;
        event_.stop();
        startCutscene(false);
    }
}

// Event command 0xAB: the choice between a new game and a saved one; the bit says whether the player chose to go on.
void Game::openLoadMenu() {
    loadMenu_.open(loadHandler_ && loadHandler_() != nullptr);
    loadMenuOpen_ = true;
    menuPhase_ = MenuPhase::FadingOut;
    brightness_ = 0;
    fade_ = 0;
}

// Event command 0xAC: the game goes on where the save left it, as if it had just been started from it.
void Game::restoreSavedGame() {
    const std::unique_ptr<SaveState> save = loadHandler_ ? loadHandler_() : nullptr;
    if (!save) return;
    event_.stop();
    bits_.init(common_);
    objects_.resetCharacters();
    for (int i = 0; i < FF6_CHAR_COUNT; ++i) setCharacterProperties(state_, rules_, i, i, 0);
    restore(*save);
    common_.windowTiles = buildWindowTiles(menuData_, config_);
    loadMapNow(static_cast<uint32_t>(save->map), save->x, save->y, save->facing, true, true);
}

// Event command 0x9B: the shop takes the screen like the menu does, and the event goes on once it is closed.
void Game::openShop(int shop) {
    if (shop < 0 || shop >= FF6_SHOP_COUNT) return;
    const Npc* lead = objects_.lead();
    const int sprite = lead ? lead->prop.gfx : 0;  // some shops change their prices with who is in front
    ShopHooks hooks;
    hooks.buyPrice = [this, shop, sprite](int item) { return shopPrice(rules_, shop, item, sprite); };
    hooks.sellPrice = [this](int item) { return sellPrice(rules_, item); };
    hooks.buy = [this, shop, sprite](int item, int quantity) {
        return buyItem(state_, item, quantity, shopPrice(rules_, shop, item, sprite));
    };
    hooks.sell = [this](int item, int quantity) { return sellItem(state_, item, quantity, sellPrice(rules_, item)); };
    hooks.items = [this] { return inventoryItems(); };
    hooks.gil = [this] { return state_.gil; };
    shop_.open(rules_.shop(shop)[0] & FF6_SHOP_KIND_MASK, shopStock(rules_, shop), hooks);
    shopOpen_ = true;
    menuPhase_ = MenuPhase::FadingOut;
    startFade(false, FF6_BRIGHTNESS_STEP);
}

// Event command 0x9A: the menu of the colosseum takes the screen; the item that was wagered and the fighter are kept for the
// command that starts the bout, and an event bit says whether there is a bout.
void Game::openColosseum() {
    ColosseumHooks hooks;
    hooks.items = [this] { return inventoryItems(); };
    hooks.party = [this] { return partyCharacters(); };
    hooks.match = [this](int item) {
        ColosseumMatch match;
        const uint8_t* entry = &battleData_.colosseum[static_cast<size_t>(item & 0xFF) * FF6_COLOSSEUM_ENTRY_BYTES];
        match.monster = entry[0];
        match.prize = entry[2];
        match.prizeHidden = entry[3] != 0;
        match.shadow = item == FF6_COLOSSEUM_ITEM_STRIKER && bits_.get(FF6_EVENT_BIT_COLOSSEUM_SHADOW);
        return match;
    };
    hooks.monsterName = [this](int monster) { return battleData_.name(monster); };
    colosseum_.open(hooks);
    colosseumOpen_ = true;
    menuPhase_ = MenuPhase::FadingOut;
    startFade(false, FF6_BRIGHTNESS_STEP);
}

// Event command 0x99: the characters are put into the parties by the player, then the event goes on. The characters of the
// game stand in the menu, those named by the command cannot be moved; with the top bit the parties are emptied first.
void Game::openPartySelect(int parties, unsigned forced) {
    PartyMenuSetup setup;
    setup.parties = std::max(parties & FF6_PARTY_SELECT_COUNT_MASK, 1);
    setup.forced = forced;
    setup.clear = (parties & FF6_PARTY_SELECT_CLEAR) != 0;
    for (int i = 0; i < FF6_CHAR_COUNT; ++i) {
        const Npc& npc = objects_.characters()[static_cast<size_t>(i)];
        PartyMenuCharacter& character = setup.characters[static_cast<size_t>(i)];
        character.enabled = npc.enabled;
        character.party = npc.party;
        character.gfx = npc.prop.gfx;
        character.palette = spritePalette_[static_cast<size_t>(npc.prop.palette) & (FF6_SPRITE_PALETTE_SLOTS - 1)];
        character.info = menuCharacter(i, state_.characters[static_cast<size_t>(i)]);
        character.status = state_.characters[static_cast<size_t>(i)].status | (character.info.hp == 0 ? FF6_ST_DEAD : 0u);
        for (int equipped : state_.characters[static_cast<size_t>(i)].equipment) {
            equipped &= 0xFF;
            if (equipped != FF6_ITEM_EMPTY && (rules_.item(equipped)[FF6_ITEM_PROP_EQUIP_STATUS_HIGH] & FF6_ITEM_EQUIP_FLOAT)) character.status |= FF6_ST_FLOAT;
        }
        if (character.info.equippedEsper >= 0) character.esper = menuData_.esperName(character.info.equippedEsper);
    }
    partyMenu_.open(setup);
    partyOpen_ = true;
    menuPhase_ = MenuPhase::FadingOut;
    startFade(false, FF6_BRIGHTNESS_STEP);
}

// What the player chose goes to the characters: the party of each and its place in it.
void Game::applyPartySelection() {
    const std::array<int, FF6_CHAR_COUNT> parties = partyMenu_.assignment(), places = partyMenu_.places();
    for (size_t i = 0; i < parties.size(); ++i) {
        Npc& character = objects_.characters()[i];
        character.party = parties[i];
        character.partySlot = parties[i] != 0 ? places[i] : 0;
    }
    objects_.restoreParty(objects_.activeParty());
    objects_.updateParty();
}

// Event command 0xAF: the character fights alone, by itself, against the monster of the item, in the battlefield of the colosseum.
void Game::startColosseumBattle() {
    if (colosseumFighter_ < 0 || colosseumItem_ == FF6_ITEM_EMPTY) return;
    colosseumBout_.monster = colosseumMatch_.monster;
    colosseumBout_.wager = colosseumItem_;
    colosseumBout_.prize = colosseumMatch_.prize;
    colosseumBackup_ = state_.characters[static_cast<size_t>(colosseumFighter_)];
    launchBattle(colosseumMatch_.shadow ? FF6_COLOSSEUM_SHADOW_BATTLE : FF6_COLOSSEUM_BATTLE, FF6_COLOSSEUM_BACKGROUND);
}

// Event commands 0x4C and 0x4D: the battle of an event group (the first one, or the second in one case of four) takes the
// screen, and the event goes on once it is over. The conditional battles and the background are not read.
void Game::startBattle(int group, int flags) {
    battleSeed_ = battleSeed_ * FF6_BATTLE_SEED_STEP + frame_ + 1;
    BattleRandom random(battleSeed_);
    beginBattle(battleData_.eventBattle(group, random.byte() >= FF6_EVENT_BATTLE_SECOND_LIMIT ? 1 : 0), flags);
}

// `flags` are the second byte of the event command: the low bits choose the background, or the map's with all of them set.
void Game::beginBattle(int battle, int flags) {
    int background = flags & FF6_BATTLE_BG_DEFAULT;
    if (background == FF6_BATTLE_BG_DEFAULT) background = map_.prop[FF6_MAP_PROP_FLAGS] & FF6_MAP_BG_MASK;
    launchBattle(battle, background);
}

void Game::startBattleNumber(int battle, int background) {
    if (battle < 0 || battle >= FF6_BATTLE_COUNT) throw std::runtime_error("invalid battle number");
    launchBattle(battle, background >= 0 ? background : map_.prop[FF6_MAP_PROP_FLAGS] & FF6_MAP_BG_MASK);
}

void Game::launchBattle(int battle, int background, bool veldt) {
    std::vector<int> characters;
    if (colosseumBout_.monster >= 0) {
        characters.push_back(colosseumFighter_);
    } else {
        for (const int id : objects_.partySlots()) {
            if (id >= 0 && characters.size() < FF6_BATTLE_PARTY_SLOTS) characters.push_back(id);
        }
        // InitParty: the places that stay empty are filled, once each, from the order that the player gave for the end.
        for (size_t at = 0; at < finalOrder_.size() && characters.size() < FF6_BATTLE_PARTY_SLOTS;) {
            const int next = finalOrder_[at];
            finalOrder_.erase(finalOrder_.begin() + static_cast<std::ptrdiff_t>(at));
            if (std::find(characters.begin(), characters.end(), next) == characters.end()) characters.push_back(next);
        }
    }
    if (characters.empty()) return;
    battleSeed_ = battleSeed_ * FF6_BATTLE_SEED_STEP + frame_ + 1;
    BattleRandom random(battleSeed_);
    if (battle & FF6_BATTLE_RANDOM_FLAG) battle = (battle & ~FF6_BATTLE_RANDOM_FLAG) + random.below(FF6_BATTLE_RANDOM_SPAN);
    if (battle == FF6_COND_BATTLE_FROM && bits_.battle(FF6_COND_BATTLE_FLAG)) battle = FF6_COND_BATTLE_TO;  // the one conditional battle of the game
    lastBattle_ = battle;
    bits_.setBattle(FF6_BATTLE_FLAG_DEFEATED, false);
    battleScreen_.start(battleData_, rules_, state_, characters, battle, battleSeed_, config_.battleSpeed(),
                        (config_.battle & FF6_CONFIG_WAIT) != 0, background, veldt, colosseumBout_);
    {  // Gau and Shadow come and go with the story
        Battle::Story story;
        story.gauAvailable = bits_.get(FF6_EVENT_BIT_CHAR_AVAILABLE + FF6_ACTOR_GAU);
        story.gauObtained = bits_.battle(FF6_BATTLE_FLAG_GAU_OBTAINED);
        story.shadowAvailable = bits_.get(FF6_EVENT_BIT_CHAR_AVAILABLE + FF6_ACTOR_SHADOW);
        story.shadowStays = bits_.battle(FF6_BATTLE_FLAG_SHADOW_STAYS);
        story.magicOnly = bits_.battle(FF6_BATTLE_FLAG_MAGIC_ONLY);
        battleScreen_.setStory(story);
    }
    // The music of the battle comes from its properties; the song of the map waits, and comes back afterwards.
    mapSong_ = -1;
    fanfarePlayed_ = false;
    const uint16_t aux = battleData_.formation(battle).aux;
    static constexpr uint8_t kSongs[8] = FF6_BATTLE_SONGS;
    uint8_t song = kSongs[(aux & FF6_BATTLE_AUX_SONG_MASK) >> FF6_BATTLE_AUX_SONG_SHIFT];
    if (const Battle* opened = battleScreen_.battle()) {  // the song of a scene comes before the one of the battle
        if (opened->sceneSong() >= 0) song = static_cast<uint8_t>(opened->sceneSong());
    }
    if (!(aux & FF6_BATTLE_AUX_KEEP_SONG) && song != 0xFF) {
        sound_.command(FF6_SOUND_PLAY_SONG, song, FF6_SOUND_FULL_VOLUME);
        mapSong_ = sound_.previousSong();
    }
    battleOpen_ = true;
    menuPhase_ = MenuPhase::FadingOut;
    startFade(false, FF6_BRIGHTNESS_STEP);
}

// CheckBattleSub, once the party has arrived on a tile: the random battle is an event, like in the game.
void Game::checkEncounter() {
    if (bits_.get(FF6_EVENT_BIT_NO_ENCOUNTERS)) return;
    const int battle = encounters_.step(battleData_, static_cast<int>(map_.id), (map_.prop[FF6_MAP_PROP_ENCOUNTERS] & FF6_MAP_ENCOUNTERS_FLAG) != 0);
    if (battle < 0) return;
    pendingBattle_ = battle;
    startEvent(FF6_EVENT_SCRIPT_RAND_BATTLE);
}

// The end of a battle: the veldt can bring it back, unless its formation is not made for it.
void Game::recordVeldtBattle() {
    if (lastBattle_ < 0 || lastBattle_ >= FF6_VELDT_BATTLE_COUNT) return;
    const BattleFormation formation = battleData_.formation(lastBattle_);
    if (formation.aux & FF6_BATTLE_AUX_NOT_ON_VELDT) return;
    for (const int monster : formation.monsters) {
        if (monster != FF6_MONSTER_NONE && monster < 0x100) {
            state_.veldtBattles[static_cast<size_t>(lastBattle_)] = true;
            return;
        }
    }
}

// A defeated party goes to the game over scene of the story and back to the saved game; without one it stands up again
// with all its hp.
void Game::endBattle() {
    bool gameOver = false;
    const BattleOutcome outcome = battleScreen_.outcome();
    const bool colosseum = colosseumBout_.monster >= 0;
    if (outcome == BattleOutcome::Lost && !colosseum) {
        if (loadHandler_ && loadHandler_()) {
            bits_.setBattle(FF6_BATTLE_FLAG_DEFEATED, true);  // the events of the battles go on with GameOver
            gameOver = true;
        } else {
            for (const Combatant& c : battleScreen_.battle()->combatants()) {
                if (c.present && !c.monster) {
                    CharacterState& character = state_.characters[static_cast<size_t>(c.id)];
                    character.hp = calcStats(rules_, character).maxHp;
                }
            }
        }
    }
    const bool gauRecruited = battleScreen_.battle() && battleScreen_.battle()->recruitedGau();
    const bool gauLeaped = battleScreen_.battle() && battleScreen_.battle()->gauLeaped();
    const bool shadowLeft = battleScreen_.battle() && battleScreen_.battle()->shadowLeft();
    battleScreen_.close();
    battleOpen_ = false;
    if (colosseum) {  // the fighter is as it was: only the wager and the prize count
        state_.characters[static_cast<size_t>(colosseumFighter_)] = colosseumBackup_;
        colosseumBout_ = Battle::Colosseum();
        colosseumFighter_ = -1;
        colosseumItem_ = FF6_ITEM_EMPTY;
    }
    recordVeldtBattle();
    if (gauRecruited) {  // a script of the battle won him over: he is a character that has joined, in the active party
        bits_.set(FF6_EVENT_BIT_CHAR_AVAILABLE + FF6_ACTOR_GAU, true);
        bits_.setBattle(FF6_BATTLE_FLAG_GAU_OBTAINED, true);
        objects_.addToParty(FF6_ACTOR_GAU, objects_.activeParty());
    } else if (gauLeaped) {  // he stays on the veldt until he jumps in again
        bits_.set(FF6_EVENT_BIT_CHAR_AVAILABLE + FF6_ACTOR_GAU, false);
        objects_.addToParty(FF6_ACTOR_GAU, 0);
    }
    if (shadowLeft) {
        bits_.set(FF6_EVENT_BIT_CHAR_AVAILABLE + FF6_ACTOR_SHADOW, false);
        objects_.addToParty(FF6_ACTOR_SHADOW, 0);
    }
    if (trainBattle_) {  // the ride of the train goes on, or the party is defeated and the story with it
        trainBattle_ = false;
        if (gameOver && train_) {
            train_ = nullptr;
            cutscene_.reset();
            event_.stop();
            startEvent(FF6_EVENT_SCRIPT_GAME_OVER);
        } else if (train_) {
            doomGazeCounter_ = train_->counter();
            train_->resume();
        }
    }
    if (doomGazeBattle_ && world_ && !gameOver) {
        if (outcome == BattleOutcome::Won) {  // the story takes the party to the scene of the magicite, on board
            bits_.setBattle(FF6_BATTLE_FLAG_DOOM_GAZE_DEFEATED, true);
            world_->startVehicleEvent(FF6_EVENT_SCRIPT_DOOM_GAZE_DEFEATED);
        }
        placeDoomGaze();
    }
    doomGazeBattle_ = false;
    if (gameOver && world_) startEvent(FF6_EVENT_SCRIPT_GAME_OVER);  // the battles of the world have no event of their own
    if (world_ && !gameOver) world_->restartMusic();
    if (!world_ && !gameOver && mapSong_ >= 0) sound_.command(FF6_SOUND_PLAY_SONG, static_cast<uint8_t>(mapSong_), FF6_SOUND_FULL_VOLUME);
    mapSong_ = -1;
}

void Game::tickMenu(const InputState& input) {
    updateBrightness();
    if (fading()) return;
    if (menuPhase_ == MenuPhase::FadingOut) {
        if (!shopOpen_ && !battleOpen_ && !loadMenuOpen_ && !colosseumOpen_ && !nameOpen_ && !finalOrderOpen_ && !partyOpen_) menu_.open(menuStatus(), config_);
        menuPhase_ = MenuPhase::Open;
        startFade(true, FF6_BRIGHTNESS_STEP);
    } else if (menuPhase_ == MenuPhase::Open && loadMenuOpen_) {
        loadMenu_.update(input);
        if (loadMenu_.closed()) {
            menuPhase_ = MenuPhase::Leaving;
            startFade(false, FF6_BRIGHTNESS_STEP);
        }
    } else if (menuPhase_ == MenuPhase::Open && battleOpen_) {
        battleScreen_.update(input);
        if (const Battle* fought = battleScreen_.battle()) {  // the fanfare of a victory, unless the battle has none
            if (fought->victory() && !fanfarePlayed_) {
                fanfarePlayed_ = true;
                if (!(battleData_.formation(lastBattle_).flags & FF6_BATTLE_FLAG_NO_FANFARE) && !fought->shadowLeft()) {
                    sound_.command(FF6_SOUND_PLAY_SONG, FF6_SONG_VICTORY_FANFARE, FF6_SOUND_FULL_VOLUME);
                }
            }
        }
        if (battleScreen_.finished()) {
            menuPhase_ = MenuPhase::Leaving;
            startFade(false, FF6_BRIGHTNESS_STEP);
        }
    } else if (menuPhase_ == MenuPhase::Open && partyOpen_) {
        partyMenu_.update(input);
        if (partyMenu_.closed()) {
            menuPhase_ = MenuPhase::Leaving;
            startFade(false, FF6_BRIGHTNESS_STEP);
        }
    } else if (menuPhase_ == MenuPhase::Open && finalOrderOpen_) {
        finalOrderMenu_.update(input);
        if (finalOrderMenu_.closed()) {
            menuPhase_ = MenuPhase::Leaving;
            startFade(false, FF6_BRIGHTNESS_STEP);
        }
    } else if (menuPhase_ == MenuPhase::Open && nameOpen_) {
        nameMenu_.update(input);
        if (nameMenu_.closed()) {
            menuPhase_ = MenuPhase::Leaving;
            startFade(false, FF6_BRIGHTNESS_STEP);
        }
    } else if (menuPhase_ == MenuPhase::Open && colosseumOpen_) {
        colosseum_.update(input);
        if (colosseum_.closed()) {
            menuPhase_ = MenuPhase::Leaving;
            startFade(false, FF6_BRIGHTNESS_STEP);
        }
    } else if (menuPhase_ == MenuPhase::Open && shopOpen_) {
        shop_.update(input);
        if (shop_.closed()) {
            menuPhase_ = MenuPhase::Leaving;
            startFade(false, FF6_BRIGHTNESS_STEP);
        }
    } else if (menuPhase_ == MenuPhase::Open) {
        menu_.update(input);
        if (menu_.takeSaveRequest() && saveHandler_) saveHandler_(saveState());
        if (menu_.closed()) {
            menuPhase_ = MenuPhase::Leaving;
            startFade(false, FF6_BRIGHTNESS_STEP);
        }
    } else {
        menuPhase_ = MenuPhase::Closed;
        if (loadMenuOpen_) {  // the story goes on in the dark, with what the player chose
            loadMenuOpen_ = false;
            bits_.set(FF6_EVENT_BIT_SKIPPED, loadMenu_.continueChosen());
            brightness_ = 0;
            fade_ = 0;
            return;
        }
        shopOpen_ = false;
        if (partyOpen_) {
            partyOpen_ = false;
            applyPartySelection();
        }
        if (finalOrderOpen_) {
            finalOrderOpen_ = false;
            finalOrder_ = finalOrderMenu_.order();
        }
        if (nameOpen_) {  // an empty name keeps the one that the character had
            nameOpen_ = false;
            if (!nameMenu_.name().empty()) setCharacterName(nameMenu_.character(), nameMenu_.name());
        }
        if (colosseumOpen_) {
            colosseumOpen_ = false;
            colosseumItem_ = colosseum_.chosen() ? colosseum_.item() : FF6_ITEM_EMPTY;
            colosseumFighter_ = colosseum_.chosen() ? colosseum_.character() : -1;
            colosseumMatch_ = colosseum_.match();
            bits_.set(FF6_EVENT_BIT_COLOSSEUM_ITEM, colosseum_.chosen());
        }
        if (battleOpen_) endBattle();
        common_.windowTiles = buildWindowTiles(menuData_, config_);  // the dialogues have the windows of the config
        if (world_) world_->setLead(worldLead());  // the menu may have changed who leads
        startFade(true, FF6_BRIGHTNESS_STEP);
    }
}

void Game::composeDialogue() {
    std::vector<uint8_t>& image = overlayPixels_;
    std::fill(image.begin(), image.end(), 0);
    const int windowY = dialogue_.atBottom() ? FF6_WINDOW_BOTTOM_Y : FF6_WINDOW_Y;
    if (!dialogueTextOnly_) drawWindow(image, common_.windowTiles, FF6_WINDOW_X, windowY, FF6_WINDOW_COLS, FF6_WINDOW_ROWS);
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

// The timer that the story shows: minutes and seconds in a small window at the top right; the game's own place is not known.
void Game::composeTimer(int frames) {
    const int seconds = frames / FF6_FRAMES_PER_SECOND;
    std::vector<int> codes;
    const std::string text = std::to_string(seconds / 60) + ":" + std::string(seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
    for (const char c : text) codes.push_back(c == ':' ? FF6_GLYPH_COLON : FF6_GLYPH_ZERO + (c - '0'));
    int width = 0;
    for (const int code : codes) width += common_.fontWidths[static_cast<size_t>(code)];
    const int x = FF6_SCREEN_WIDTH - FF6_WINDOW_X - FF6_TIMER_COLS * FF6_WINDOW_TILE_SIZE, y = FF6_WINDOW_Y;
    drawWindow(overlayPixels_, common_.windowTiles, x, y, FF6_TIMER_COLS, FF6_TIMER_ROWS);
    const uint8_t shadow[FF6_BYTES_PER_PIXEL] = FF6_TEXT_SHADOW_RGBA;
    const uint8_t ink[FF6_BYTES_PER_PIXEL] = FF6_TEXT_INK_RGBA;
    for (const uint8_t* color : {shadow, ink}) {
        const int offset = color == shadow ? FF6_TEXT_SHADOW_OFFSET : 0;
        int pen = x + (FF6_TIMER_COLS * FF6_WINDOW_TILE_SIZE - width) / 2;
        for (const int code : codes) {
            const size_t base = static_cast<size_t>(code - FF6_FIRST_GLYPH_CODE) * FF6_GLYPH_ROWS;
            for (int row = 0; row < FF6_GLYPH_ROWS; ++row) {
                const uint16_t bits = common_.fontRows[base + static_cast<size_t>(row)];
                for (int col = 0; col < FF6_GLYPH_ROW_BITS; ++col) {
                    if (bits & (FF6_GLYPH_LEFT_BIT >> col)) blendPixel(overlayPixels_, pen + col + offset, y + FF6_TIMER_TEXT_Y + row + offset, color);
                }
            }
            pen += common_.fontWidths[static_cast<size_t>(code)];
        }
    }
}

// InitPortrait: the special npcs with no graphics of their own show the portrait that is in the sprite memory.
void Game::drawPortrait(uint8_t* screen, bool above) {
    if (portrait_ < 0 || !bits_.get(FF6_EVENT_BIT_PORTRAIT)) return;
    if (portraitShown_ != portrait_) {
        portraitImage_ = buildPortraitImage(menuData_, portrait_);
        portraitShown_ = portrait_;
    }
    for (const Npc& npc : objects_.npcs()) {
        if (!npc.enabled || !npc.visible || !npc.prop.special || npc.prop.gfx != FF6_MAP_SPRITE_NOTHING) continue;
        if (npc.topIsAbove() != above) continue;
        const int layer = npc.prop.scrollBg2 ? FF6_BG2 : FF6_BG1;
        const int x = npc.pixelX() - scroll_.x(layer);
        const int y = npc.pixelY() - FF6_SPECIAL_SPRITE_Y_OFFSET - npc.jumpHeight - scroll_.y(layer) - stage_.shakeOffset(3);
        const int tile = npc.prop.vram * 2;  // the characters of the sprite memory are 16 to a row
        const int size = npc.prop.large ? 4 : 2;
        for (int row = 0; row < size * FF6_MENU_TILE_SIZE; ++row) {
            const int sourceY = (tile >> 4) * FF6_MENU_TILE_SIZE + row, screenY = y + row;
            if (sourceY >= FF6_MENU_PORTRAIT_SIZE || screenY < 0 || screenY >= FF6_SCREEN_HEIGHT) continue;
            for (int column = 0; column < size * FF6_MENU_TILE_SIZE; ++column) {
                const int sourceX = (tile & 15) * FF6_MENU_TILE_SIZE + column, screenX = x + column;
                if (sourceX >= FF6_MENU_PORTRAIT_SIZE || screenX < 0 || screenX >= FF6_SCREEN_WIDTH) continue;
                const uint8_t* source = &portraitImage_[(static_cast<size_t>(sourceY) * FF6_MENU_PORTRAIT_SIZE + sourceX) * FF6_BYTES_PER_PIXEL];
                if (source[FF6_ALPHA_OFFSET] == 0) continue;
                std::copy_n(source, FF6_BYTES_PER_PIXEL, screen + (static_cast<size_t>(screenY) * FF6_SCREEN_WIDTH + screenX) * FF6_BYTES_PER_PIXEL);
            }
        }
    }
}

// Paints the sprites that are drawn below (above = false) or above the high priority layers.
void Game::drawSprites(bool above) {
    std::vector<Drawable> drawables;
    const auto add = [&](Npc& object) {
        const int layer = object.prop.scrollBg2 ? FF6_BG2 : FF6_BG1;
        Drawable drawable{object.pixelY(), stripOf(object), object.frame, object.pixelX() - scroll_.x(layer),
                          object.pixelY() - FF6_SPRITE_Y_OFFSET - object.jumpHeight - scroll_.y(layer) - stage_.shakeOffset(3),
                          object.topIsAbove(), object.bottomIsAbove()};
        if ((object.vehicle & FF6_VEHICLE_MASK) != 0) {
            // The steps come from the position along the axis that it moves on, and the lead is moved by the player.
            const bool led = &object == objects_.lead() && player_.moving();
            const bool horizontal = object.speedX != 0 || (led && (object.facing == Left || object.facing == Right));
            drawable.onVehicle = true;
            drawable.look = vehicleLook(object.vehicle, object.facing, horizontal ? object.pixelX() : object.pixelY(),
                                        horizontal || object.speedY != 0 || led);
        }
        drawables.push_back(drawable);
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
    drawPortrait(screen, above);
    for (const Drawable& sprite : drawables) {
        const std::vector<uint8_t>& strip = strips_[static_cast<size_t>(sprite.strip)];
        if (sprite.onVehicle) {
            // The OAM order puts the front first and the bottom sprites after the top ones: paint the back first.
            for (auto piece = sprite.look.pieces.rbegin(); piece != sprite.look.pieces.rend(); ++piece) {
                if ((piece->top ? sprite.topAbove : sprite.bottomAbove) != above) continue;
                const int x = sprite.screenX + piece->x, y = sprite.screenY + piece->y;
                if (piece->tile == 0) {
                    drawSpriteRows(screen, strip, sprite.look.riderFrame, piece->firstRow, piece->rows, x, y);
                } else {
                    drawVehicleSprite(screen, vehicleSprite(spritePalette_[FF6_SPRITE_PALETTE_SLOTS - 1], piece->tile), x, y);
                }
            }
            continue;
        }
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
    if (cutscene_ && menuPhase_ == MenuPhase::Closed) {
        cutscene_->draw(screenPixels_.data());
        renderer_.beginFrame();
        renderer_.updateTexture(screen_, screenPixels_.data());
        renderer_.draw(screen_, kScreen, kScreen);
        renderer_.endFrame();
        return;
    }
    if (menuPhase_ == MenuPhase::Open || menuPhase_ == MenuPhase::Leaving) {
        if (battleOpen_) {
            battleScreen_.draw();
        } else if (loadMenuOpen_) {
            loadMenu_.draw();
        } else if (shopOpen_) {
            shop_.draw();
        } else if (finalOrderOpen_) {
            finalOrderMenu_.draw();
        } else if (nameOpen_) {
            nameMenu_.draw();
        } else if (colosseumOpen_) {
            colosseum_.draw();
        } else if (partyOpen_) {
            partyMenu_.draw();
        } else {
            menu_.draw();
        }
        screenPixels_ = menuScreen_.pixels();
        renderer_.beginFrame();
        applyBrightness(screenPixels_);
        renderer_.updateTexture(screen_, screenPixels_.data());
        renderer_.draw(screen_, kScreen, kScreen);
        renderer_.endFrame();
        return;
    }
    if (world_) {
        world_->draw(screenPixels_.data());
        renderer_.beginFrame();
        applyBrightness(screenPixels_);
        renderer_.updateTexture(screen_, screenPixels_.data());
        renderer_.draw(screen_, kScreen, kScreen);
        renderer_.endFrame();
        return;
    }
    for (auto& layer : layerPixels_) {
        for (auto& pixels : layer) std::fill(pixels.begin(), pixels.end(), 0);
    }
    for (auto& pixels : spritePixels_) std::fill(pixels.begin(), pixels.end(), 0);
    for (int layer = 0; layer < FF6_LAYER_COUNT; ++layer) {
        auto& pixels = layerPixels_[static_cast<size_t>(layer)];
        renderBg(map_, layer, scroll_.x(layer), scroll_.y(layer) + stage_.shakeOffset(layer), pixels[0].data(), pixels[1].data());
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
    ColorMath math = decodeColorMath(map_.colorMath);
    if (stage_.fixedActive()) {  // the fixed color is added to (or taken from) the three layers of bg, with no sub screen
        int rgb[3];
        stage_.fixedRgb(rgb);
        math.designation = static_cast<uint8_t>(FF6_SCREEN_BG1 | FF6_SCREEN_BG2 | FF6_SCREEN_BG3 | (stage_.fixedSubtract() ? FF6_COLOR_MATH_SUBTRACT : 0));
        math.subScreen = 0;
        for (int channel = 0; channel < FF6_COLOR_CHANNELS; ++channel) math.fixed[channel] = static_cast<uint8_t>(rgb[channel]);
    }
    composeScreen(math, backdrop, layers, sizeof(layers) / sizeof(layers[0]), screenPixels_.data());
    applyStage(screenPixels_);
    renderer_.beginFrame();
    applyBrightness(screenPixels_);
    renderer_.updateTexture(screen_, screenPixels_.data());
    renderer_.draw(screen_, kScreen, kScreen);
    const int shownTimer = bits_.shownTimer();
    if (dialogue_.active() || shownTimer >= 0) {
        if (dialogue_.active()) composeDialogue();
        else std::fill(overlayPixels_.begin(), overlayPixels_.end(), 0);
        if (shownTimer >= 0) composeTimer(shownTimer);
        applyBrightness(overlayPixels_);
        renderer_.updateTexture(overlay_, overlayPixels_.data());
        renderer_.draw(overlay_, kScreen, kScreen);
    }
    renderer_.endFrame();
}

}  // namespace ff6
