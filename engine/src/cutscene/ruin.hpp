#pragma once

#include "cutscene/core.hpp"

namespace ff6 {

// The world falls apart (src/cutscene/ruin.asm): the planet in the dark, two pieces of land that go away, sparks and
// explosions that come up by chance, then the fade out.
class RuinScene : public Cutscene {
public:
    RuinScene(const TitleData& data, Sound& sound);

    void tick(const InputState& input) override;
    void draw(uint8_t* rgba) override { core_.render(rgba); }
    bool finished() const override { return done_; }
    bool skipped() const override { return false; }

    const CutsceneCore& core() const { return core_; }

private:
    void initHardware();
    void loadGraphics();
    void state();
    void sparks();  // _7e6f97 and _7e6f4c: what comes by chance at each frame
    uint8_t chance();  // _7e6f83

    CutsceneCore core_;
    uint8_t chanceCounter_ = 0;
    bool carry_ = false;  // the carry of the last addition of chance()
    bool ending_ = false, done_ = false;
};

}  // namespace ff6
