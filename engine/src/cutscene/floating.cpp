#include "cutscene/floating.hpp"

#include <algorithm>

namespace ff6 {
namespace {

constexpr uint16_t kFloatingPal = 0x6CD2;
constexpr int kRiseFrames = 600, kHoldFrames = 60, kStartFrames = 16, kEndFrames = 15;

// _7e6b8c and _7e6b9d: the matrix grows (twice as fast at the end) and the plane goes down every 8 frames.
class ZoomTask : public CutsceneTask {
public:
    explicit ZoomTask(FloatingScene::Registers* regs) : regs_(regs) {}
    bool run(CutsceneCore& core) override {
        if (core.state == 3 || (core.state != 4 && (core.frame & 1) == 0)) {
            ++regs_->a;
            ++regs_->d;
        } else if (core.state == 4) {
            regs_->a = static_cast<uint16_t>(regs_->a + 2);
            regs_->d = static_cast<uint16_t>(regs_->d + 2);
        }
        if ((core.frame & 7) == 0) ++core.scroll[1];
        return true;
    }

private:
    FloatingScene::Registers* regs_;
};

// _7e6b18: half of the fixed color is added to the screen, and the color changes every 33 frames. Each write
// goes to the channels that its top bits pick, so the values that run past 0x3F reach the other channels.
class ColorTask : public CutsceneTask {
public:
    explicit ColorTask(FloatingScene::Registers* regs) : regs_(regs) {}
    bool run(CutsceneCore&) override {
        if (!started_) {
            started_ = true;
            regs_->cgwsel = 0x80;
            regs_->cgadsub = 0x57;
        } else if (timer_ == 0) {
            step(values_[0], 0x97);
            step(values_[1], 0x8F);
            step(values_[2], 0x91);
            timer_ = 0x20;
        }
        timer_ = static_cast<uint8_t>(timer_ - 1);
        return true;
    }

private:
    void step(uint8_t& value, uint8_t limit) {
        if (value >= limit) return;
        const uint8_t intensity = value & 0x1F;
        if (value & 0x20) regs_->fixed[0] = intensity;
        if (value & 0x40) regs_->fixed[1] = intensity;
        if (value & 0x80) regs_->fixed[2] = intensity;
        ++value;
    }

    FloatingScene::Registers* regs_;
    std::array<uint8_t, 3> values_{0x88, 0x20, 0x42};
    uint8_t timer_ = 0x20;
    bool started_ = false;
};

}  // namespace

FloatingScene::FloatingScene(const TitleData& data, Sound& sound) : core_(data, sound) {
    core_.ppu.objsel = 0x03;
    core_.ppu.m7sel = 0x80;
    core_.ppu.tm = 0x17;
    core_.ppu.ts = 0x17;
    core_.ppu.clearVram();
    loadGraphics();
    core_.hdma = [this](Ppu& ppu) { setLines(ppu); };
}

// LoadFloatingContGfx: 64 tiles of 4 bits become one byte for each pixel (in the high byte of the words), and the
// ground is a map of 32 x 10 tiles in the low bytes, at the top left of the plane.
void FloatingScene::loadGraphics() {
    const std::vector<uint8_t>& gfx = core_.data.floatingGfx;
    constexpr size_t kMapOffset = 0x7C0;
    for (size_t tile = 0; tile < 64; ++tile) {
        for (size_t row = 0; row < 8; ++row) {
            const size_t at = tile * 32 + row * 2;
            if (at + 17 >= gfx.size()) continue;
            for (size_t pixel = 0; pixel < 8; ++pixel) {
                const int bit = 7 - static_cast<int>(pixel);
                const int color = (gfx[at] >> bit & 1) | ((gfx[at + 1] >> bit & 1) << 1) |
                                  ((gfx[at + 16] >> bit & 1) << 2) | ((gfx[at + 17] >> bit & 1) << 3);
                core_.ppu.vram[tile * 64 + row * 8 + pixel] = static_cast<uint16_t>(color << 8);
            }
        }
    }
    for (size_t row = 0; row < 10; ++row) {
        for (size_t column = 0; column < 32; ++column) {
            const size_t from = kMapOffset + row * 32 + column;
            if (from < gfx.size()) core_.ppu.vram[row * 128 + column] |= gfx[from];
        }
    }
}

// _7e6acf: from the first state on, the matrix and the center of mode 7 go to the registers.
void FloatingScene::setLines(Ppu& ppu) const {
    const bool shown = core_.state != 0;
    for (PpuLine& line : ppu.lines) {
        line.bgMode = 7;
        line.cgwsel = regs_.cgwsel;
        line.cgadsub = regs_.cgadsub;
        line.fixed = regs_.fixed;
        line.m7a = shown ? static_cast<int16_t>(regs_.a) : 0;
        line.m7d = shown ? static_cast<int16_t>(regs_.d) : 0;
        line.m7b = line.m7c = 0;
    }
    ppu.m7x = 0x40;
    ppu.m7y = 0x20;
    ppu.m7hofs = core_.scroll[0];
    ppu.m7vofs = core_.scroll[1];
}

void FloatingScene::state() {
    CutsceneCore& core = core_;
    switch (core.state) {
        case 0:
            core.scroll[0] = 0xFFA0;
            core.scroll[1] = 0xFFC0;
            regs_.a = regs_.d = 1;
            for (const int destination : {0x3100, 0x3000}) {
                core.loadPal(destination, kBlackPal);
                core.createFadePal(2, destination, kFloatingPal);
            }
            core.createTask<ZoomTask>(0, &regs_);
            ++core.state;
            core.brightness = 0x0F;
            core.timer = kStartFrames;
            break;
        case 1:
            if (core.timer == 0) {
                ++core.state;
                core.timer = kRiseFrames;
                core.createTask<ColorTask>(0, &regs_);
            }
            break;
        case 2:
            if (core.timer == 0) {
                ++core.state;
                core.timer = kHoldFrames;
            }
            break;
        case 3:
            if (core.timer == 0) {
                ++core.state;
                core.timer = 1;
            }
            break;
        case 4:
            if (core.timer == 0) ++core.state;
            break;
        default: core.state = 0xFF; break;
    }
}

void FloatingScene::tick(const InputState&) {
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
