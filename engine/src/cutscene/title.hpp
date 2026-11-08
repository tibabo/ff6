#pragma once

#include "cutscene/core.hpp"

namespace ff6 {

// The splash screen (the license), then the title screen of the game: the storm over Narshe, with the lightning, and
// the logo on flames (src/cutscene/title.asm). Pressing the confirm button ends it, which the story remembers.
class TitleScene : public Cutscene {
public:
    TitleScene(const TitleData& data, Sound& sound);

    void tick(const InputState& input) override;
    void draw(uint8_t* rgba) override { core_.render(rgba); }
    bool finished() const override { return phase_ == Phase::Done; }
    bool skipped() const override { return skipped_; }

    const CutsceneCore& core() const { return core_; }

private:
    enum class Phase { Splash, SplashFade, Title, TitleFade, Done };

    void initHardware();
    void loadGraphics();
    void transfer(int destination, const uint8_t* source, size_t bytes, int add);
    void splashState();
    void titleState();
    void createText();
    void beginFade(Phase next);
    void setLines(Ppu& ppu);

    CutsceneCore core_;
    Phase phase_ = Phase::Splash;
    bool skipped_ = false;
};

}  // namespace ff6
