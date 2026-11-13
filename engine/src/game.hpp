#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "data/pack.hpp"
#include "battle/battle_screen.hpp"
#include "battle/encounters.hpp"
#include "cutscene/opening.hpp"
#include "cutscene/title.hpp"
#include "data/battle_data.hpp"
#include "data/game_rules.hpp"
#include "data/save.hpp"
#include "data/world_data.hpp"
#include "field/anim.hpp"
#include "field/entrance.hpp"
#include "field/event.hpp"
#include "field/map.hpp"
#include "field/obj.hpp"
#include "field/player.hpp"
#include "field/scroll.hpp"
#include "field/stage.hpp"
#include "field/text.hpp"
#include "input.hpp"
#include "menu/field_menu.hpp"
#include "menu/colosseum_menu.hpp"
#include "menu/name_menu.hpp"
#include "menu/final_order_menu.hpp"
#include "menu/load_menu.hpp"
#include "menu/party_menu.hpp"
#include "menu/shop_menu.hpp"
#include "renderer.hpp"
#include "sound.hpp"
#include "state/party_ops.hpp"
#include "cutscene/train.hpp"
#include "cutscene/ending_card.hpp"
#include "cutscene/falcon.hpp"
#include "cutscene/floating.hpp"
#include "cutscene/ruin.hpp"
#include "cutscene/the_end.hpp"
#include "world/world_screen.hpp"

namespace ff6 {

// The menu takes the screen: the field fades out, the menu fades in, and the other way round when it is closed.
enum class MenuPhase { Closed, FadingOut, Open, Leaving };

// The way into a battle. The field has the mosaic of BattleMosaic, or a cut to black when the event turns it off; the world
// map and the cutscenes fade out. Then the screen stays black while the game loads the battle, and the battle fades in.
enum class BattleEntry { None, Fade, Mosaic, Cut, Loading, FadeIn };

// Marks a game that begins like the console: the title screen, then the story and the opening, which ends with the choice
// of a new game or a saved one.
struct NewGame {};

// The field: game state and the drawing of it. It reaches the graphics library only through Renderer.
class Game : private EventHost {
public:
    // The renderer owns the textures created here, so it must outlive the game; so must the sound. `party` lists the
    // characters that the game starts with, Terra alone when it is empty.
    Game(CommonData common, std::string packDir, uint32_t startMap, Renderer& renderer, Sound& sound,
         const std::vector<int>& party = {});
    // A game that goes on where a save left it.
    Game(CommonData common, std::string packDir, const SaveState& save, Renderer& renderer, Sound& sound);
    // A game that begins with the event of the start of the game, which shows the title screen first.
    Game(CommonData common, std::string packDir, NewGame, Renderer& renderer, Sound& sound);
    // What is needed to go on from here: see SaveState.
    SaveState saveState() const;

    // Advances the game by 1/FF6_FRAMES_PER_SECOND second.
    void tick(const InputState& input);
    void draw();
    // Puts the party on another tile of the current map, for captures and tests.
    void placeParty(int tileX, int tileY);
    // The lead of the party rides a vehicle (FF6_VEHICLE_*), for captures.
    void setLeadVehicle(int vehicle);

    const MapData& map() const { return map_; }
    const Player& player() const { return player_; }
    const Dialogue& dialogue() const { return dialogue_; }
    const Objects& objects() const { return objects_; }
    const EventBits& eventBits() const { return bits_; }
    // The screen brightness, FF6_BRIGHTNESS_FULL when it is lit; the fades move it.
    int brightness() const { return brightness_; }
    bool fading() const override { return fade_ != 0; }
    MenuPhase menuPhase() const { return menuPhase_; }
    const FieldMenu& menu() const { return menu_; }
    const ShopMenu& shop() const { return shop_; }
    bool shopOpen() const { return shopOpen_; }
    const ColosseumMenu& colosseum() const { return colosseum_; }
    bool colosseumOpen() const { return colosseumOpen_; }
    const BattleScreen& battle() const { return battleScreen_; }
    BattleScreen& battle() { return battleScreen_; }
    bool battleOpen() const { return battleOpen_; }
    const GameConfig& config() const { return config_; }
    const GameRules& rules() const { return rules_; }
    // The characters, the inventory and the gil.
    PartyState& party() { return state_; }
    const PartyState& party() const { return state_; }
    // What the Save option of the menu does with the state of the game; without one, it saves nothing.
    void setSaveHandler(std::function<void(const SaveState&)> handler) { saveHandler_ = std::move(handler); }
    // Where the menu after the title and the load command get the saved game from: null when there is none.
    void setLoadHandler(std::function<std::unique_ptr<SaveState>()> handler) { loadHandler_ = std::move(handler); }
    // The title screen or the opening has the screen.
    bool cutsceneOpen() const { return cutscene_ != nullptr; }
    // A save holds the whole game only when the party walks freely: no event, battle, menu, dialogue, scene or fade is running.
    bool canQuickSave() const;
    bool loadMenuOpen() const { return loadMenuOpen_; }
    // The party is on a world map, which has the screen, instead of a map of the field.
    bool worldOpen() const { return world_ != nullptr; }
    const WorldScreen* world() const { return world_.get(); }
    // Where the party is, on whichever kind of map.
    int partyTileX() const { return world_ ? world_->tileX() : player_.tileX(); }
    int partyTileY() const { return world_ ? world_->tileY() : player_.tileY(); }
    bool partyMoving() const { return world_ ? world_->moving() : player_.moving(); }
    // Ends the fades at once, for captures.
    void finishFade();
    // Starts battle number `battle` (0 to 575) over the field, with the background number `background` (-1 for the map's).
    void startBattleNumber(int battle, int background = -1);

private:
    // EventHost
    void openDialogue(uint32_t id, uint16_t flags) override;
    bool dialogueBusy() const override { return dialogue_.active(); }
    int dialogueChoiceCount() const override { return dialogue_.choiceCount(); }
    int dialogueChoice() const override { return dialogue_.choice(); }
    void changeMap(uint32_t map, int x, int y, int facing, bool startupEvent, bool fadeIn) override;
    void setVehicle(int vehicle) override { pendingVehicle_ = vehicle; }
    bool startObjectScript(int object, uint32_t offset) override;
    bool objectBusy(int object) const override;
    void objectCommand(uint8_t op, int object, uint32_t arg) override;
    void fieldCommand(uint8_t op, const uint8_t* bytes, size_t size) override;
    Sound& sound() override { return sound_; }
    unsigned characterMask(uint8_t op) const override;

    bool loadMapNow(uint32_t mapId, int x, int y, int facing, bool startupEvent, bool fadeIn);
    bool enterWorld(uint32_t mapId, int x, int y, int facing, bool fadeIn);
    void placeDoomGaze();
    void startTrain();
    void takeStep();
    void tickWorld(const InputState& input);
    WorldScreen::Lead worldLead() const;
    void initialize(uint32_t startMap, const std::vector<int>& party, const SaveState* save, bool newGame = false);
    void restore(const SaveState& save);
    void startFade(bool in, int speed);
    void playMapSong();
    void updateBrightness();
    void applyBrightness(std::vector<uint8_t>& image) const;
    int stripFor(int gfx, int palette);
    bool stageCommand(uint8_t op, const uint8_t* bytes, size_t size);
    void spritePaletteChanged();
    void resetSpriteColors();
    void applyStage(std::vector<uint8_t>& image);
    const std::vector<uint8_t>& vehicleSprite(int palette, uint16_t word);
    int stripOf(Npc& object);
    void loadSpecialGfx();
    void drawSpecial(uint8_t* screen, const Npc& npc, int x, int y) const;
    void startEvent(uint32_t offset);
    void followScroll();
    void copyPlayerToLead(Npc& lead) const;
    void invalidateStrips();
    void checkArrival();
    bool talkToNpc();
    bool openTreasure();
    void showOpenedTreasures();
    void appendItemName(int item);
    bool openDoor(int direction);
    void changeMapData(const uint8_t* bytes, size_t size);
    void moveParty(int tileX, int tileY);
    void tickMenu(const InputState& input);
    void openShop(int shop);
    void openColosseum();
    void openPartySelect(int parties, unsigned forced);
    void applyPartySelection();
    void startColosseumBattle();
    void startCutscene(bool opening);
    void startCard(const std::vector<std::string>& lines, int frames, bool toTitle);
    void endCutscene();
    void openLoadMenu();
    void restoreSavedGame();
    void startBattle(int group, int flags);
    void beginBattle(int battle, int flags);
    void launchBattle(int battle, int background, bool veldt = false, BattleEntry entry = BattleEntry::Fade);
    bool tickBattleEntry();
    int battleMosaicSize() const;
    void applyBattleFadeIn(std::vector<uint8_t>& image) const;
    void recordVeldtBattle();
    void checkEncounter();
    void endBattle();
    MenuStatus menuStatus();
    void updatePortraits();
    bool onSavePoint() const;  // the flag of the main menu that allows the tent and the sleeping bag
    int eventCharacter(int code) const;
    MenuCharacter menuCharacter(int id, const CharacterState& state) const;
    std::vector<MenuItem> inventoryItems() const;
    std::vector<MenuCharacter> partyCharacters() const;
    CharacterMask availableCharacters() const;
    void leaveThroughEntrance(const EntranceTarget& target);
    void composeDialogue();
    void composeTimer(int frames);
    void drawSprites(bool above);
    void drawPortrait(uint8_t* screen, bool above);

    CommonData common_;
    std::string packDir_;
    Renderer& renderer_;
    Sound& sound_;
    MapData map_;
    MapAnim anim_;
    Scroll scroll_;
    Player player_;
    Objects objects_;
    Dialogue dialogue_;
    EventBits bits_;
    EventRunner event_;
    MenuData menuData_;
    MenuScreen menuScreen_;
    FieldMenu menu_;
    ShopMenu shop_;
    bool shopOpen_ = false;  // the menu that is open is a shop, not the field menu
    ColosseumMenu colosseum_;
    bool colosseumOpen_ = false;  // or the menu of the colosseum
    NameMenu nameMenu_;
    bool nameOpen_ = false;  // or the one that names a character
    FinalOrderMenu finalOrderMenu_;
    bool finalOrderOpen_ = false;  // or the one that orders them for the end of the game
    std::vector<int> finalOrder_;  // what it chose: the characters that fill the empty places of a party
    int colosseumItem_ = FF6_ITEM_EMPTY, colosseumFighter_ = -1;  // what the menu chose
    ColosseumMatch colosseumMatch_;
    Battle::Colosseum colosseumBout_;  // set while the bout is fought
    CharacterState colosseumBackup_;  // what the fighter was before: the bout leaves no mark on it
    BattleData battleData_;
    BattleGfx battleGfx_;
    AttackAnim attackAnim_;
    BattleScreen battleScreen_;
    bool battleOpen_ = false;  // the screen is the battle: it takes the place of the menu
    BattleEntry battleEntry_ = BattleEntry::None;
    int battleEntryFrame_ = 0;  // the frames of the step of the entry
    int battleExitFrame_ = 0;  // the frames of black once the battle has faded out
    uint32_t battleSeed_ = 1;
    Encounters encounters_;
    int lastBattle_ = -1;  // the battle that started last
    bool fanfarePlayed_ = false, fanfareSounded_ = false;  // its time has come; it was heard (the battle has one)
    int pendingBattle_ = 0;  // the battle that the random battle event starts
    int boxGroup_ = 0;  // the group of the monster in the box that was just opened
    MenuPhase menuPhase_ = MenuPhase::Closed;
    GameConfig config_;
    GameRules rules_;
    PartyState state_;
    std::function<void(const SaveState&)> saveHandler_;
    std::function<std::unique_ptr<SaveState>()> loadHandler_;
    TitleData titleData_;
    bool titleDataLoaded_ = false;
    bool cardToTitle_ = false;  // the end of the game: the title screen comes back after the card
    TrainRide* train_ = nullptr;  // the cutscene, when it is the ride of the magitek train
    bool trainBattle_ = false;  // the battle that is open comes from the ride
    std::unique_ptr<Cutscene> cutscene_;  // the title screen or the opening, which take the screen like the menu does
    WorldData worldData_;
    bool worldDataLoaded_ = false;
    std::unique_ptr<WorldScreen> world_;  // the world map, which takes the place of the field
    int airshipX_ = 0, airshipY_ = 0;  // the tile of the world map where the airship is parked
    int pendingVehicle_ = 0;  // what the map that is going to load has the party on: FF6_EVENT_MAP_AIRSHIP
    bool flightSaved_ = false;  // the party is on the deck of the airship, which flies on
    int flightAngle_ = 0, flightAltitude_ = 0;
    int doomGazeCounter_ = 0;
    bool eventLoad_ = false;  // the map is loaded by an event of the field, which hands what follows to the world map
    bool worldScript_ = false;  // the event waits while the world map runs its script
    bool resumePending_ = false;  // the script of the world map loads a map, where the event goes on
    uint32_t resumeOffset_ = 0;
    bool doomGazeBattle_ = false;  // the battle that is open is Doom Gaze
    LoadMenu loadMenu_;
    PartyMenu partyMenu_;
    bool partyOpen_ = false;  // the menu that is open is the party selection
    int leaveItem_ = -1;  // the tent or warp stone that ended the main menu: its event runs when the menu is gone
    bool loadMenuOpen_ = false;
    bool restoreRequested_ = false;  // the event asked for the saved game: it is loaded once the event is out of the way
    uint32_t frame_ = 0;
    std::set<int> warnedObjects_;  // objects that are not there, reported once
    std::vector<uint8_t> dialogueText_;  // the dialogue that is open, with the values that the game fills in
    bool dialogueTextOnly_ = false;  // the dialogue has no window, for the story of the opening
    long foundGil_ = -1;  // what the dialogue says about the chest that was just opened; -1 for nothing
    int receivedItem_ = -1;
    bool screenLocked_ = false;  // the screen follows the camera object instead of the party
    int leadSavedMovement_ = FF6_OBJ_MOVE_USER;  // what the lead does again when the event is over
    bool restorePending_ = false;
    bool eventWasRunning_ = false;
    std::array<int, FF6_SPRITE_PALETTE_SLOTS> spritePalette_{0, 1, 2, 3, 4, 5, 6, 7};  // palette of each slot
    int lastTileX_ = 0, lastTileY_ = 0;
    bool armed_ = false;  // entrances only fire once the party has stepped off the tile it was placed on
    bool hasParent_ = false;
    uint32_t parentMap_ = 0;
    int parentX_ = 0, parentY_ = 0, parentFacing_ = 0;
    int brightness_ = FF6_BRIGHTNESS_FULL;
    int fade_ = 0;  // 0 when no fade runs, else the speed, with FF6_FADE_OUT_FLAG for a fade out
    bool fadeInPending_ = false;  // the screen fades in once the startup event of the map is over
    bool mapSongPending_ = false;  // and its song plays then
    bool quietMapLoad_ = false;  // the map that a new game loads behind the title screen plays no song
    bool leaving_ = false;  // the screen fades out before the party goes through an entrance
    EntranceTarget leaveTarget_;

    TextureId screen_ = 0, overlay_ = 0;  // the field with its color math, and the dialogue window over it
    std::array<std::array<std::vector<uint8_t>, 2>, FF6_LAYER_COUNT> layerPixels_;  // [layer][low, high priority]
    std::array<std::vector<uint8_t>, 2> spritePixels_;  // below and above the high priority layers
    std::vector<std::vector<uint8_t>> strips_;  // RGBA sprite strips
    FieldStage stage_;  // what the events that stage a scene ask of the screen
    std::array<uint16_t, FF6_MAP_PALETTE_COLORS> bgBase_{};  // the colors of the map before the events changed them
    std::array<uint16_t, FF6_MAP_PALETTE_COLORS> spriteBase_{}, spriteColors_{};  // the eight palettes of the sprites, as they are and as they were
    bool spriteModified_ = false;
    int defaultParty_ = 1;  // the party that 0x7B gives back
    int portrait_ = -1;  // the character of 0xE7
    int portraitShown_ = -1;
    std::vector<uint8_t> portraitImage_;
    std::vector<uint8_t> specialChars_;  // the sprite characters of the special npcs: palette indices, 16 x 16 tiles of 8 x 8 pixels
    std::map<std::pair<int, int>, int> stripIndex_;  // by graphics and palette
    std::map<std::pair<int, int>, std::vector<uint8_t>> vehicleSprites_;  // by palette and word of the tile
    std::vector<uint8_t> screenPixels_, overlayPixels_;
};

}  // namespace ff6
