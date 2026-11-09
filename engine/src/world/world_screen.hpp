#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "cutscene/ppu.hpp"
#include "data/pack.hpp"
#include "data/world_data.hpp"
#include "field/entrance.hpp"
#include "input.hpp"
#include "sound.hpp"

namespace ff6 {

// What the world map asks of the game around it.
struct WorldHooks {
    std::function<bool(int)> bit;  // an event bit of the game
    std::function<void(int, bool)> setBit;
    // The battle that a step on a tile with random battles starts: the group of the table (a word that may carry
    // FF6_BATTLE_RANDOM_FLAG) or -1. `sector` is the 32 x 32 tile square of the map that the battle table is read at.
    std::function<int(int world, int tileBackground, int sectorX, int sectorY, int* background)> encounter;
    std::function<void()> step;  // a step was taken: the counter of the menu
};

// Port of the world map program (src/world): the walk on the world of balance and the world of ruin, in mode 7 with the
// perspective that the game gets from changing the matrix at every line. It draws with the picture processor of the
// cutscenes, filled the way the game fills the video memory.
class WorldScreen {
public:
    static constexpr int kPixelsPerStep = 16;

    // The sprite sheet and the palette of the character that leads the party.
    struct Lead {
        int gfx = 0;
        int palette = 0;
    };

    WorldScreen(const WorldData& data, const CommonData& common, Sound& sound, WorldHooks hooks);

    // LoadWorld and InitWorld: the party stands on a tile, looking in a direction; the music of the map starts unless
    // an event asked to keep the one that plays.
    void enter(int world, int tileX, int tileY, int facing, const Lead& lead);
    void setLead(const Lead& lead);
    // The music of the map starts again, once a battle is over.
    void restartMusic() { playMusic(); }
    void tick(const InputState& input);
    // The screen as FF6_SCREEN_WIDTH x FF6_SCREEN_HEIGHT RGBA pixels.
    void draw(uint8_t* rgba);

    int world() const { return world_; }
    int tileX() const { return dfx_ >> 8; }
    int tileY() const { return dfy_ >> 8; }
    int pixelX() const { return dfx_ >> 4; }
    int pixelY() const { return dfy_ >> 4; }
    int facing() const { return facing_; }
    bool moving() const { return speedX_ != 0 || speedY_ != 0; }
    bool aligned() const { return (dfx_ & 0xFF) == 0 && (dfy_ & 0xFF) == 0; }
    bool eventRunning() const { return eventRunning_; }
    // The tile that the map holds at a place, after the changes that the events made to it.
    int tileAt(int x, int y) const { return tilemap_[static_cast<size_t>((y & 255) * FF6_WORLD_SIZE + (x & 255))]; }
    uint16_t propAt(int x, int y) const;
    bool zooming() const { return zoom_ >= 0; }
    // The menu button was pressed on a tile.
    bool takeMenuRequest();
    // The party stepped on an entrance, or an event loads another map: where it leads.
    bool takeEntrance(EntranceTarget* target);
    // The step started a random battle (after the zoom): the group and the background.
    bool takeBattle(int* group, int* background);
    // The airship: where it is parked (a tile), and the flight of the party that is on board.
    void setAirship(int x, int y) { airshipX_ = x; airshipY_ = y; }
    int airshipX() const { return airshipX_; }
    int airshipY() const { return airshipY_; }
    // The party comes out of the story on board, flying in a direction (degrees, 0 is north) at an altitude.
    void enterFlying(int world, int tileX, int tileY, int facing, const Lead& lead, int angle = 0, int altitude = 0);
    bool flying() const { return mode_ != Mode::Walk; }
    // The airship keeps flying while the party is on its deck.
    bool keepsFlying() const { return deck_ || keepFlight_; }
    // Doom Gaze waits in two places of the world of ruin, in squares of 4 x 4 tiles; the battle starts when the airship goes over one.
    void setDoomGaze(bool active, int x0, int y0, int x1, int y1) { doomGaze_ = {active, {{{x0, y0}, {x1, y1}}}}; }
    bool takeDoomGaze() {
        const bool ready = doomGazeReady_;
        doomGazeReady_ = false;
        return ready;
    }
    // A script of the airship, which the story runs while it flies.
    void startVehicleEvent(uint32_t offset) { startEvent(offset); }
    int flightAngle() const { return flight_.angle >> 8; }
    int flightAltitude() const { return flight_.altitude; }
    // The tile that the airship is over.
    int flightTileX() const { return flight_.x >> 12; }
    int flightTileY() const { return flight_.y >> 12; }
    // The tile properties of the lines of the picture, for tests and captures.
    const Ppu& ppu() const { return ppu_; }

private:
    enum class Mode { Walk, TakeoffFade, TweenIn, Fly, TweenOut };
    // The airship in flight: positions are in 1/256 pixel, angles in 1/256 degree, altitudes as the game keeps them.
    struct Flight {
        int angle = 0, rate = 0, speed = 0, climb = 0, altitude = 0;
        int x = 0, y = 0;
        int horizon = 0x92, rotation = 0;  // $85 and $83
        bool landing = false;
    };
    // The camera of the mode 7 view: the walk and the flight differ only by these.
    struct Camera {
        int scale = 0x9000, divStart = 0x8FFF, divEnd = 0xFFFF;  // $8b, $8d, $8f
        int lines = 0xE0;  // $87: the lines of the map, at the bottom of the screen
        int pivot = 0x70;  // $7d: the line of the screen that the party is on
        int horizon = 0xF2;  // $62: the horizon in the transitions
    };
    void loadGraphics();
    void loadFlightGraphics();
    void startFlight(int angle, int altitude, bool tween);
    void tickFlight(const InputState& input);
    void moveFlight(const InputState& input);
    void landAirship();
    void finishLanding();
    void renderFlight();
    void execVehicleCmd();
    void drawAnimFrame(int frame, int x, int y, size_t firstSprite, size_t count);
    void drawAirship();
    void drawParkedAirship();
    void hideSprites(size_t first, size_t count);
    void boardAirship();
    void applyModifications();
    void buildWindow();
    void updateMode7Vars();
    void movePlayer(const InputState& input);
    void getPlayerInput(const InputState& input);
    void checkEvent();
    bool checkEntrance();
    void checkBattle();
    void execWorldCmd();
    void drawWorldChar();
    void updateSpriteAnim();
    void updateWater();
    void drawMinimap();
    void setCharacterSprites();
    void startEvent(uint32_t offset);
    bool conditionMet(int word) const;
    uint8_t scriptByte();
    int scriptWord();
    void jumpTo(uint32_t offset);
    void loadMapCommand();
    void playMusic();

    const WorldData& data_;
    const CommonData& common_;
    Sound& sound_;
    WorldHooks hooks_;
    Ppu ppu_;
    Lead lead_;
    int world_ = 0;
    std::vector<uint8_t> tilemap_;  // $7f0000: the map, with the changes of the events
    int windowX_ = -1, windowY_ = -1;  // the tile at the top left of the 64 x 64 tiles that the video memory holds

    // The direct page of the program: positions are in 1/16 pixel, the way the game keeps them.
    uint16_t dfx_ = 0, dfy_ = 0;  // $df, $e1
    int speedX_ = 0, speedY_ = 0;  // $e3, $e5
    int facing_ = 2;  // $f6
    int walkSpeed_ = 0x10;  // $f3
    bool forest_ = false;  // $e7 bit 4: the lower half of the character shows through the trees
    bool stepStarted_ = false;  // $e7 bit 1: a step began since the last tile
    bool battleChecked_ = false;  // $e8 bit 3: the tile that the party is on has been looked at
    bool menuRequested_ = false;  // $e8 bit 0
    bool menuLocked_ = false;  // $e9 bit 4: an event or an entrance does not let the menu open
    bool confirm_ = false;  // the confirm button went down this frame ($08)
    bool eventRunning_ = false;  // $e7 bit 0
    bool inCommand_ = false;  // $e7 bit 2: a command that lasts is not over
    int sectorX_ = 0, sectorY_ = 0;  // the position that the random battle table is read at ($1f60): a stale one, as in the game
    int phase_ = 0, phaseCounter_ = 0;  // $7eb650 and $7eb652: the step of the walk
    int frame_ = 0;  // $f7: the frame of the sprite sheet
    bool hidden_ = false;  // $e7 bit 5
    int brightness_ = 15, targetBrightness_ = 15;  // $23 and $22

    // The script of an event ($ea to $f0)
    uint32_t scriptPtr_ = 0;
    int command_ = 0;  // $f0
    int commandCount_ = 0;  // $ef
    int pauseCounter_ = 0;  // $f1 of the pause command
    bool entranceReady_ = false;
    EntranceTarget entrance_;
    bool battleReady_ = false;
    int battleGroup_ = 0, battleBackground_ = 0;
    int zoom_ = -1;  // the step of BattleZoom, -1 when it does not run
    int zoomHigh_ = 0x90;  // $8c: the high byte of $8b, which sets the scale of the first line

    // The water: the rows of four characters that turn, and the colors that turn every 16 frames.
    std::array<uint8_t, 0x100> water_{};
    std::array<uint8_t, 16> waterCounter_{};
    int paletteCounter_ = 0;  // $b860
    bool minimapShown_ = true;
    Mode mode_ = Mode::Walk;
    Flight flight_;
    Camera camera_;
    int airshipX_ = 0, airshipY_ = 0;  // $1f62: the tile where the airship is parked
    int tween_ = 0;  // the step of the camera in the transitions
    int modeFrames_ = 0;
    int shipFrame_ = 0, shipCounter_ = 1, shipRate_ = 1;  // the propellers: $7eb650, $7eb652, $7eb654
    int bg2Scroll_ = 0;  // $70
    struct DoomGaze {
        bool active = false;
        std::array<std::array<int, 2>, 2> cells{};
    };
    DoomGaze doomGaze_;
    bool doomGazeReady_ = false;
    bool keepFlight_ = false;  // a script of the airship leaves the world while the airship flies on
    bool deck_ = false;  // the party goes to the deck: the airship keeps its heading and altitude for the way back
    unsigned autoHeld_ = 0;  // the buttons that a script of the airship holds down
};

}  // namespace ff6
