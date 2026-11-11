#pragma once

#include <memory>

#include "cutscene/credits_pages.hpp"
#include "cutscene/ending_tasks.hpp"
#include "cutscene/ppu.hpp"
#include "data/title_data.hpp"

namespace ff6 {

// The ground of the flights of the credits (src/menu/ending.asm): a map of mode 7 that a camera moves over, turns on and tilts,
// by the buttons that a script presses for it. The matrix of each line comes from the angle, the zoom and the tilt (UpdateMode7HDMA).
class FlightBackdrop {
public:
    enum class Ground { Clouds, Sea, SeaBoat, Land, Airship, Jet };
    enum class Sky { None, Left, Right, Rising };  // the sky above the horizon, and how its clouds go

    // `state` is the one of the ending that starts the flight: 2, 4, 5, 9, 12, 16, 20, 24, 28 or 32.
    FlightBackdrop(const TitleData& data, int state);

    void tick();
    void draw(uint8_t* rgba) const { ppu_.render(rgba); }
    const Ppu& ppu() const { return ppu_; }

private:
    void build(Ground ground);
    void buildSky();
    void updateMatrix();
    void applyLines();
    uint16_t scriptWord(int at) const;
    void loadSprites();
    void loadCreditsFont();
    void animateTiles();  // the propellers and the jet trails, which the game changes in the map of the plane
    void startSprites();  // the sprites that the scene has from its first frame, and the ones that come later
    void createBirds(int table, bool follows);
    EndingTasks::Task& createAirship(EndingTasks::Code code, int priority, int address, int x, int y);

    const TitleData& data_;
    Ppu ppu_;
    EndingTasks tasks_;
    int state_ = 0, ticks_ = 0;
    int scale_ = 0x10;  // the zoom of the airship above the clouds (state 5), or of the jet (state 32)
    int angle_ = 0, zoom_ = 0x100, tilt_ = 0;  // $c3, $c5, $c7
    int centerX_ = 0x100, centerY_ = 0x80, scrollX_ = 0x80, scrollY_ = 0;  // $3d, $3f: the mode 7 center and the scroll of bg1
    std::vector<CreditsPage> pages_;  // the words that come up over the ground, as sprites of the font of the credits
    Sky sky_ = Sky::None;
    int skyX_ = 0, skyY_ = 0, frame_ = 0;  // the scroll of the sky, and the frames since the start
    uint16_t buttons_ = 0;  // what the script presses
    int script_ = -1, index_ = 0, duration_ = 0;
    std::array<int16_t, 112> a_{}, b_{}, c_{};
};

// The ground of the scene that a state starts, null for the scenes that do not have one yet.
std::unique_ptr<FlightBackdrop> makeFlightBackdrop(const TitleData& data, int state);

}  // namespace ff6
