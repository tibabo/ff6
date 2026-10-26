#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "data/pack.hpp"
#include "data/save.hpp"
#include "field/anim.hpp"
#include "field/entrance.hpp"
#include "field/event.hpp"
#include "field/map.hpp"
#include "field/obj.hpp"
#include "field/player.hpp"
#include "field/scroll.hpp"
#include "field/text.hpp"
#include "input.hpp"
#include "menu/field_menu.hpp"
#include "renderer.hpp"
#include "sound.hpp"

namespace ff6 {

// The menu takes the screen: the field fades out, the menu fades in, and the other way round when it is closed.
enum class MenuPhase { Closed, FadingOut, Open, Leaving };

// The field: game state and the drawing of it. It reaches the graphics library only through Renderer.
class Game : private EventHost {
public:
    // The renderer owns the textures created here, so it must outlive the game; so must the sound. `party` lists the
    // characters that the game starts with, Terra alone when it is empty.
    Game(CommonData common, std::string packDir, uint32_t startMap, Renderer& renderer, Sound& sound,
         const std::vector<int>& party = {});
    // A game that goes on where a save left it.
    Game(CommonData common, std::string packDir, const SaveState& save, Renderer& renderer, Sound& sound);
    // What is needed to go on from here: see SaveState.
    SaveState saveState() const;

    // Advances the game by 1/FF6_FRAMES_PER_SECOND second.
    void tick(const InputState& input);
    void draw();
    // Puts the party on another tile of the current map, for captures and tests.
    void placeParty(int tileX, int tileY);

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
    const GameConfig& config() const { return config_; }
    // What the Save option of the menu does with the state of the game; without one, it saves nothing.
    void setSaveHandler(std::function<void(const SaveState&)> handler) { saveHandler_ = std::move(handler); }
    // Ends the fades at once, for captures.
    void finishFade();

private:
    // EventHost
    void openDialogue(uint32_t id, uint16_t flags) override;
    bool dialogueBusy() const override { return dialogue_.active(); }
    int dialogueChoiceCount() const override { return dialogue_.choiceCount(); }
    int dialogueChoice() const override { return dialogue_.choice(); }
    void changeMap(uint32_t map, int x, int y, int facing, bool startupEvent, bool fadeIn) override;
    bool startObjectScript(int object, uint32_t offset) override;
    bool objectBusy(int object) const override;
    void objectCommand(uint8_t op, int object, uint32_t arg) override;
    void fieldCommand(uint8_t op, const uint8_t* bytes, size_t size) override;
    Sound& sound() override { return sound_; }

    bool loadMapNow(uint32_t mapId, int x, int y, int facing, bool startupEvent, bool fadeIn);
    void initialize(uint32_t startMap, const std::vector<int>& party, const SaveState* save);
    void restore(const SaveState& save);
    void startFade(bool in, int speed);
    void updateBrightness();
    void applyBrightness(std::vector<uint8_t>& image) const;
    int stripFor(int gfx, int palette);
    int stripOf(Npc& object);
    void startEvent(uint32_t offset);
    void followScroll();
    void copyPlayerToLead(Npc& lead) const;
    void invalidateStrips();
    void checkArrival();
    void talkToNpc();
    bool openDoor(int direction);
    void changeMapData(const uint8_t* bytes, size_t size);
    void moveParty(int tileX, int tileY);
    void tickMenu(const InputState& input);
    MenuStatus menuStatus() const;
    void leaveThroughEntrance(const EntranceTarget& target);
    void composeDialogue();
    void drawSprites(bool above);

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
    MenuPhase menuPhase_ = MenuPhase::Closed;
    GameConfig config_;
    std::function<void(const SaveState&)> saveHandler_;
    uint32_t frame_ = 0;
    std::set<int> warnedObjects_;  // objects that are not there, reported once
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
    bool leaving_ = false;  // the screen fades out before the party goes through an entrance
    EntranceTarget leaveTarget_;

    TextureId screen_ = 0, overlay_ = 0;  // the field with its color math, and the dialogue window over it
    std::array<std::array<std::vector<uint8_t>, 2>, FF6_LAYER_COUNT> layerPixels_;  // [layer][low, high priority]
    std::array<std::vector<uint8_t>, 2> spritePixels_;  // below and above the high priority layers
    std::vector<std::vector<uint8_t>> strips_;  // RGBA sprite strips
    std::map<std::pair<int, int>, int> stripIndex_;  // by graphics and palette
    std::vector<uint8_t> screenPixels_, overlayPixels_;
};

}  // namespace ff6
