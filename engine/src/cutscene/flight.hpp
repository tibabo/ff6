#pragma once

#include <memory>

#include "cutscene/ppu.hpp"
#include "data/title_data.hpp"

namespace ff6 {

// The ground of the flights of the credits (src/menu/ending.asm): a map of mode 7 that a camera moves over, turns on and tilts,
// by the buttons that a script presses for it. The matrix of each line comes from the angle, the zoom and the tilt (UpdateMode7HDMA).
class FlightBackdrop {
public:
    enum class Ground { Clouds, Sea, SeaBoat, Land };
    enum class Sky { None, Left, Right, Rising };  // the sky above the horizon, and how its clouds go

    // `state` is the one of the ending that starts the flight: 2, 4, 9, 12, 16, 20, 24 or 28.
    FlightBackdrop(const TitleData& data, int state);

    void tick();
    void draw(uint8_t* rgba) const { ppu_.render(rgba); }
    const Ppu& ppu() const { return ppu_; }
    // The ground is light, so the credits are written in gold instead of white (the normal palette of the game against the inverse one).
    bool light() const { return light_; }

private:
    void build(Ground ground);
    void buildSky();
    void updateMatrix();
    void applyLines();
    uint16_t scriptWord(int at) const;

    const TitleData& data_;
    Ppu ppu_;
    int angle_ = 0, zoom_ = 0x100, tilt_ = 0;  // $c3, $c5, $c7
    int centerX_ = 0x100, centerY_ = 0x80, scrollX_ = 0x80, scrollY_ = 0;  // $3d, $3f: the mode 7 center and the scroll of bg1
    bool light_ = false;
    Sky sky_ = Sky::None;
    int skyX_ = 0, skyY_ = 0, frame_ = 0;  // the scroll of the sky, and the frames since the start
    uint16_t buttons_ = 0;  // what the script presses
    int script_ = -1, index_ = 0, duration_ = 0;
    std::array<int16_t, 112> a_{}, b_{}, c_{};
};

// The ground of the scene that a state starts, null for the scenes that do not have one yet.
std::unique_ptr<FlightBackdrop> makeFlightBackdrop(const TitleData& data, int state);

}  // namespace ff6
