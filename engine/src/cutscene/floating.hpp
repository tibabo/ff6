#pragma once

#include <array>

#include "cutscene/core.hpp"

namespace ff6 {

// The continent that rises (src/cutscene/floating_cont.asm): the ground of mode 7 shows, the plane goes
// away as the matrix grows, and a color that comes by steps covers the screen.
class FloatingScene : public Cutscene {
public:
    // What the threads of the scene change: the matrix of mode 7 and the color math.
    struct Registers {
        uint16_t a = 1, d = 1;
        uint8_t cgwsel = 0, cgadsub = 0;
        std::array<uint8_t, 3> fixed{};
    };

    FloatingScene(const TitleData& data, Sound& sound);

    void tick(const InputState& input) override;
    void draw(uint8_t* rgba) override { core_.render(rgba); }
    bool finished() const override { return done_; }
    bool skipped() const override { return false; }

    const CutsceneCore& core() const { return core_; }

private:
    void loadGraphics();
    void state();
    void setLines(Ppu& ppu) const;

    CutsceneCore core_;
    Registers regs_;
    bool ending_ = false, done_ = false;
};

}  // namespace ff6
