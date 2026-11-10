#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "cutscene/core.hpp"
#include "data/world_data.hpp"

namespace ff6 {

// The magitek train ride (src/world/train_*.asm): a view of the way down a tunnel, made of layers of tiles at twelve sizes
// that the course moves, in a picture of 80 x 80 cells that the picture processor shows three times smaller, from the
// middle. A step of the course is four frames. Some stretches bring a battle, which the game runs and gives the ride back.
class TrainRide : public Cutscene {
public:
    // `rng` is the table of random numbers and `counter` the place in it that the game is at ($1f6d).
    TrainRide(const TrainData& data, const std::vector<uint8_t>& rng, int counter, Sound& sound);

    void tick(const InputState& input) override;
    void draw(uint8_t* rgba) override;
    bool finished() const override { return done_; }
    bool skipped() const override { return false; }

    // A battle that the course asks for: the group of the event battles, which one of the group, and the background.
    bool takeBattle(int* group, bool* second, int* background);
    // The battle is over: the ride goes on from the stretch where it was.
    void resume();
    int counter() const { return counter_; }

    // For the tests: a cell of the picture, and the place on the course.
    int cell(int x, int y) const { return buffer_[static_cast<size_t>(y) * 256 + static_cast<size_t>(x)]; }
    int step() const { return step_; }
    int brightness() const { return brightness_; }

private:
    struct Record {
        uint8_t x = 0, y = 0;
        uint16_t tile = 0;
    };

    void layOutTiles();
    void start();
    void loadStretch(int half, int offset);
    void update();
    void execCommand();
    void advance();
    void drawTiles();

    const TrainData& data_;
    const std::vector<uint8_t>& rng_;
    Sound& sound_;
    Ppu ppu_;
    std::vector<uint8_t> memory_;  // $7e0000: where the pictures of the tiles are laid out
    std::array<uint8_t, 0x8000> buffer_{};  // $7f8000: the picture, 256 bytes to a row
    std::array<uint8_t, FF6_TRAIN_RING> pitch1_{}, pitch2_{}, yaw_{}, background_{}, layerX_{}, layerY_{};
    std::array<uint8_t, 32> yawMultiplier_{};
    std::vector<Record> records_;
    int courseOffset_ = 0;  // $34
    int step_ = 0;  // $36
    int command_ = 0;  // $f0
    int counter_ = 0;  // $1f6d
    int frame_ = 0;
    int brightness_ = 0, targetBrightness_ = 15;  // $23, $22
    bool fading_ = false;  // $19
    bool done_ = false;
    bool battleReady_ = false;
    int battleGroup_ = 0, battleBackground_ = 0;
    bool battleSecond_ = false;
    bool waitingForBattle_ = false;
};

}  // namespace ff6
