#pragma once

#include <array>

#include "cutscene/core.hpp"

namespace ff6 {

// The march of the magitek armors in the snow, with the names of the makers of the game (src/cutscene/opening.asm).
// The ground is mode 7, seen in perspective; above the horizon, Narshe and the credits are on bg2 and bg3.
class OpeningScene : public Cutscene {
public:
    // What the threads of the scene share: the positions of the armors and the registers of mode 7.
    struct Shared {
        std::array<uint8_t, 3> armorX{0x68, 0x48, 0x78};  // $39
        std::array<uint8_t, 3> armorY{0xC0, 0x80, 0x90};  // $3c
        uint8_t armorOffsetY = 0;  // $38: the armors bob up and down together
        uint16_t centerX = 0, centerY = 0;  // $3f and $41: the center of mode 7, which the ground turns around
        std::array<uint8_t, 4> bg3Scroll{};  // the horizontal scroll of the bands of the credits
    };

    OpeningScene(const TitleData& data, Sound& sound);

    void tick(const InputState& input) override;
    void draw(uint8_t* rgba) override { core_.render(rgba); }
    bool finished() const override { return phase_ == Phase::Done; }
    bool skipped() const override { return skipped_; }

    const CutsceneCore& core() const { return core_; }

private:
    enum class Phase { Run, Fade, Done };

    void initHardware();
    void loadGraphics();
    void buildMode7(int firstTile, const std::vector<uint8_t>& tiles, int destination);
    void openingState();
    void setLines(Ppu& ppu);
    void updateMode7Position();

    CutsceneCore core_;
    Shared shared_;
    Phase phase_ = Phase::Run;
    int lag_ = 0;
    bool skipped_ = false;
};

}  // namespace ff6
