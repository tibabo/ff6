#include "cutscene/ruin.hpp"

namespace ff6 {
namespace {

// Addresses in the cutscene program ($7e5000 on).
constexpr uint16_t kLandAnim1 = 0x707A, kLandAnim2 = 0x70A6;
constexpr uint16_t kBlinkAnim = 0x70DA, kSparkAnim = 0x70F6, kFireAnim = 0x7119;
constexpr uint16_t kPlanetPal = 0x717D, kLandEdgePal = 0x719D, kLandPal = 0x71BD, kExplosionPal = 0x71DD;
constexpr uint16_t kFirePositions = 0x6EE9;  // ten places, one every 16 frames
constexpr uint16_t kSparkPositions = 0x6F7B;
constexpr int kLandFrames = 0x294, kStartFrames = 180, kFireStage = 1080, kEndFrames = 15;

// One piece of land (_7e6efd): it stays for a while, then goes off at its speed.
class LandTask : public CutsceneTask {
public:
    LandTask(uint16_t table, int x, int y, int speedX, int speedY, int delay)
        : table_(table), x_(static_cast<uint16_t>(x << 8)), y_(static_cast<uint16_t>(y << 8)), speedX_(speedX), speedY_(speedY), delay_(delay) {}
    bool run(CutsceneCore& core) override {
        if (!started_) {
            started_ = true;
            core.initAnim(anim_, table_);
        }
        if (counter_ != 0) {
            if (delay_ == 0) {
                x_ = static_cast<uint16_t>(x_ + speedX_);
                y_ = static_cast<uint16_t>(y_ + speedY_);
            }
            --counter_;
            if (delay_ != 0) --delay_;
        }
        core.updateAnim(anim_);
        core.drawAnim(anim_, x_ >> 8, y_ >> 8);
        return true;
    }

private:
    uint16_t table_, x_, y_;
    int speedX_, speedY_, delay_, counter_ = kLandFrames;
    bool started_ = false;
    AnimState anim_;
};

// A flash that plays its frames once and goes (_7e705a).
class FlashTask : public CutsceneTask {
public:
    FlashTask(uint16_t table, int x, int y) : table_(table), x_(x), y_(y) {}
    bool run(CutsceneCore& core) override {
        if (!started_) {
            started_ = true;
            core.initAnim(anim_, table_);
        } else if (anim_.duration == 0xFE) {
            return false;
        }
        core.updateAnim(anim_);
        core.drawAnim(anim_, x_, y_);
        return true;
    }

private:
    uint16_t table_;
    int x_, y_;
    bool started_ = false;
    AnimState anim_;
};

// The fires that come one after the other (_7e6e8a), and the land palette that follows the last.
class FireTask : public CutsceneTask {
public:
    bool run(CutsceneCore& core) override {
        if (delay_ == 0) {
            const uint32_t at = kFirePositions + static_cast<uint32_t>(index_);
            core.createTask<DefaultAnimTask>(0, kFireAnim, core.data.byteAt(at) << 8, core.data.byteAt(at + 1) << 8, 0, 0);
            index_ += 2;
            if (index_ == 20) {
                core.loadPal(0x3180, kLandEdgePal);
                return false;
            }
            delay_ = 0x10;
        }
        --delay_;
        return true;
    }

private:
    int index_ = 0, delay_ = 0;
};

}  // namespace

RuinScene::RuinScene(const TitleData& data, Sound& sound) : core_(data, sound) {
    initHardware();
    core_.ppu.clearVram();
    loadGraphics();
}

void RuinScene::initHardware() {
    Ppu& ppu = core_.ppu;
    ppu.objsel = 0x03;
    ppu.bgsc = {0x03, 0x11, 0x19};
    ppu.nba12 = 0x23;
    ppu.nba34 = 0x22;
    ppu.tm = 0x17;
    ppu.ts = 0x01;
    for (PpuLine& line : ppu.lines) {
        line.bgMode = 1;
        line.cgwsel = 0x82;
        line.cgadsub = 0x50;
    }
}

// LoadWorldOfRuinGfx: the characters of bg1, its map, and the sprites.
void RuinScene::loadGraphics() {
    const std::vector<uint8_t>& gfx = core_.data.ruinGfx;
    const auto transfer = [&](size_t from, size_t bytes, size_t to) {
        for (size_t i = 0; i + 1 < bytes && from + i + 1 < gfx.size(); i += 2) {
            core_.ppu.vram[to + i / 2] = static_cast<uint16_t>(gfx[from + i] | gfx[from + i + 1] << 8);
        }
    };
    transfer(0x0000, 0x0FE0, 0x3000);
    transfer(0x0FE0, 0x0700, 0x0000);
    transfer(0x16E0, 0x0DE0, 0x6000);
}

// _7e6f83: a number from the table of random numbers, which the table gives again. The last addition has no clear carry before it,
// so the carry of the one before goes in (the loop of the scene clears it at each frame, and a new task clears it too).
uint8_t RuinScene::chance() {
    ++chanceCounter_;
    const uint8_t first = core_.data.random[chanceCounter_];
    const int sum = core_.data.random[first] + first + (carry_ ? 1 : 0);
    carry_ = sum > 0xFF;
    return static_cast<uint8_t>(sum);
}

// _7e6f97 (a blink) and then _7e6f4c (a spark): the order of the draws is the one of the game.
void RuinScene::sparks() {
    if ((chance() & 7) == 0) {
        carry_ = false;
        const int low = chance() & 0x3F;
        const int x = low + 0x68 + (carry_ ? 1 : 0);
        carry_ = false;
        const int high = chance() & 0x3F;
        const int y = high + 0x98 + (carry_ ? 1 : 0);
        carry_ = false;
        core_.createTask<FlashTask>(0, kBlinkAnim, x, y);
    }
    if ((chance() & 0x1F) == 0) {
        carry_ = false;
        const uint32_t at = kSparkPositions + static_cast<uint32_t>(chance() & 3) * 2;
        carry_ = false;
        core_.createTask<FlashTask>(0, kSparkAnim, core_.data.byteAt(at), core_.data.byteAt(at + 1));
    }
}

void RuinScene::state() {
    CutsceneCore& core = core_;
    carry_ = false;  // the loop shifts a small number just before a state runs
    switch (core.state) {
        case 0:
            chanceCounter_ = 0;
            for (const int destination : {0x3000, 0x3180, 0x31A0}) core.loadPal(destination, kBlackPal);
            core.createFadePal(4, 0x3000, kPlanetPal);
            core.createFadePal(4, 0x3180, kLandPal);
            core.createFadePal(4, 0x31A0, kExplosionPal);
            core.createTask<LandTask>(2, kLandAnim1, 0x72, 0x80, 4, -4, 0x168);
            core.createTask<LandTask>(2, kLandAnim2, 0x50, 0xA0, -2, 4, 0x168);
            ++core.state;
            core.brightness = 0x0F;
            core.timer = kStartFrames;
            break;
        case 1:
            if (core.timer == 0) {
                ++core.state;
                core.timer = kFireStage;
                core.createTask<FireTask>(0);
            }
            sparks();
            break;
        case 2:
            if (core.timer == 0) {
                ++core.state;
                core.timer = kStartFrames;
                for (const int destination : {0x3000, 0x3180, 0x31A0}) core.createFadePal(8, destination, kBlackPal);
            }
            sparks();
            break;
        case 3:
            if (core.timer == 0) ++core.state;
            sparks();
            break;
        default: core.state = 0xFF; break;
    }
}

void RuinScene::tick(const InputState&) {
    if (done_) return;
    if (!ending_) {
        if (core_.state & 0x80) {
            ending_ = true;
            core_.timer = kEndFrames;
            core_.createTask<FadeOutTask>(0);
        } else {
            state();
        }
    } else if (core_.timer == 0) {
        done_ = true;
        return;
    }
    core_.execTasks();
    core_.vblank();
}

}  // namespace ff6
