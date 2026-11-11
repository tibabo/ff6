#pragma once

#include <array>
#include <vector>

#include "cutscene/core.hpp"
#include "cutscene/ppu.hpp"
#include "data/title_data.hpp"
#include "data/world_data.hpp"

namespace ff6 {

// The flight of the Falcon that opens the ending (FalconEndingMain, src/world/cutscene.asm; the `ending airship` command): the
// ground of the credits seen in mode 7 by the world program, which the airship crosses, and a haze that goes from blue to black
// down the screen. The Falcon is one sprite that drifts to the right, falls, and goes off to the upper left; when it is far enough
// the screen fades out.
class FalconScene : public Cutscene {
public:
    FalconScene(const TitleData& data, const WorldData& world);

    void tick(const InputState&) override;
    void draw(uint8_t* rgba) override { ppu_.render(rgba); }
    bool finished() const override { return finished_; }
    bool skipped() const override { return false; }
    const Ppu& ppu() const { return ppu_; }

private:
    void updateFalcon();  // UpdateSpriteAnim_17 and _18
    void updateParticles();  // the rest of UpdateSpriteAnim_18
    void drawSprites();  // DrawWorldSprites
    void movePosition();  // UpdateVehiclePos
    void applyLines();

    struct Entry {  // a line of the table of sprites ($7eb5d0): a frame of the animations and where it goes
        int frame = 0, x = 0, y = 0;
    };

    const WorldData& world_;
    std::vector<uint8_t> random_, frameTable_;
    Ppu ppu_;
    // The Falcon: its place and its steps in 1/256 pixel ($7eb650 to $7eb656), and the frames since the start.
    int falconX_ = 0xC000, falconY_ = 0x8000, stepX_ = 0x60, stepY_ = 0xFF00, frames_ = 0;
    int shownX_ = 0, shownY_ = 0, shownFrame_ = 0, wing_ = 0, wingCounter_ = 1;
    int speed_ = 0x400, heading_ = 100;
    int positionX_ = 0, positionY_ = 0;  // where the camera is over the plane, in 1/256 pixel
    // The particles ($7eb660): two sets of five, of eight bytes (state, the two counters, y, x as a word), a set each frame.
    std::array<Entry, 10> entries_{};
    std::array<uint8_t, 0x100> particles_{};
    int particleBase_ = 0, particleStep_ = 0, randomIndex_ = 0;
    int brightness_ = 0, target_ = 15;
    bool finished_ = false;
};

}  // namespace ff6
