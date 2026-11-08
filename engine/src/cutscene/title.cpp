#include "cutscene/title.hpp"

#include <algorithm>

namespace ff6 {
namespace {

// Addresses in the cutscene program ($7e5000 on), by the names that the disassembly gives them.
constexpr uint16_t kSplashPal = 0x5135;
constexpr uint16_t kIntroPal = 0x7B43;  // _7e7b43: the palette of the texts
constexpr uint16_t kDarkPal = 0x7B63;
constexpr uint16_t kFlamesPal = 0x7B83;
constexpr uint16_t kFlamesPal1 = 0x7BA3;  // _7e7ba3
constexpr uint16_t kIntroPal0 = 0x7BC3;  // _7e7bc3
constexpr uint16_t kUpperCloudsPal = 0x7BE3;
constexpr uint16_t kLowerCloudsPal = 0x7C03;
constexpr uint16_t kCloudsPal = 0x7C23;
constexpr uint16_t kTextAnim1 = 0x7A5C;  // "(c)1994 Square Co.,Ltd" and "Licensed by"
constexpr uint16_t kTextAnim2 = 0x7A5F;  // "Nintendo"
constexpr uint16_t kLightningAnim = 0x563E;  // the palettes of a flash of lightning
constexpr uint16_t kBoltAnims = 0x5746;  // the shapes of the bolts
constexpr int kText1X = 0x40, kText2X = 0x90, kTextY = 0xA0;
constexpr int kSplashFrames = 300, kEndFadeFrames = 15;

// The tables that the HDMA reads: four bytes for each line, the horizontal then the vertical scroll.
constexpr int kBg1Table = 0x3D00, kBg2Table = 0x4080, kBg3Table = 0x4400;
constexpr int kTableEnd = 0x4780;
constexpr int kTableLines = 223;
constexpr int kWaveLines = 48, kWaveFirstLine = 64;
constexpr int kBg1BaseScroll = 0x011F, kBg1HiddenScroll = 0x0100, kBg1HiddenFirstLine = 63, kBg1HiddenLines = 65;
constexpr int kEdgeLines = 63, kEdgeLastStep = 0x40, kEdgeFirstAddress = 0x3DFA;

// The sine waves that make the logo ripple (_7e53c5 and _7e5431): bg2 or bg3 are moved a little from line to line.
class WaveTask : public CutsceneTask {
public:
    WaveTask(int table, int multiplier, int start, bool vertical) : table_(table), multiplier_(multiplier), start_(start), vertical_(vertical) {}
    bool run(CutsceneCore& core) override {
        if (core.flags & 0x80) return false;
        for (int i = 0; i < kWaveLines; ++i) {
            const int value = (multiplier_ * core.sine(phase_ + start_ + i * 4)) >> 8;
            const int address = table_ + (kWaveFirstLine + i) * 4;
            core.setRamWord(address, static_cast<uint16_t>(value));
            if (vertical_) core.setRamWord(address + 2, static_cast<uint16_t>(value));
        }
        ++phase_;
        return true;
    }

private:
    int table_, multiplier_, start_;
    bool vertical_;
    int phase_ = 0;
};

// _7e54af: every other frame, for 63 frames, the vertical scroll of bg1 along the line of the horizon is computed again.
class EdgeTask : public CutsceneTask {
public:
    bool run(CutsceneCore& core) override {
        if ((core.frame & 1) == 0) return true;
        if (step_ == kEdgeLastStep) return false;
        const unsigned quotient = 0x4000u / static_cast<unsigned>(step_);
        unsigned accumulated = 0;
        for (int i = 0, address = kEdgeFirstAddress + 4 * kEdgeLines; i < kEdgeLines; ++i, address -= 4) {
            uint16_t value;
            if (step_ < i) {
                value = 0x0100;
            } else {
                value = static_cast<uint16_t>(-static_cast<int>(((accumulated >> 8) - static_cast<unsigned>(i)) & 0xFF));
            }
            core.setRamWord(address, value);
            accumulated = (accumulated + quotient) & 0xFFFF;
        }
        ++step_;
        return true;
    }

private:
    int step_ = 1;
};

// _7e5539: bg1 scrolls down a little every four frames, and the rows of the picture that it has left are replaced.
class StreamTask : public CutsceneTask {
public:
    bool run(CutsceneCore& core) override {
        if (!started_) {
            started_ = true;
            core.dmaDestination = 0xFFFF;
            core.dmaBytes = 0x40;
            core.scroll[1] = 1;
        }
        if (core.frame & 3) return true;
        const uint16_t scroll = core.scroll[1];
        if (scroll == 0x011F) {
            core.dmaDestination = 0xFFFF;
            return false;
        }
        if (scroll < 0x0100 && (scroll & 7) == 0) {
            const uint16_t row = static_cast<uint16_t>((scroll - 8) & 0xFFF8);
            core.dmaSource = &core.data.gfx[static_cast<size_t>(0x23C0 + ((row << 3) & 0xFFFF))];
            core.dmaDestination = static_cast<uint16_t>(row << 2);
        }
        core.scroll[1] = static_cast<uint16_t>(scroll + 1);
        return true;
    }

private:
    bool started_ = false;
};

// _7e56f9: a bolt of lightning, one time in four.
class BoltTask : public CutsceneTask {
public:
    bool run(CutsceneCore& core) override {
        if (!started_) {
            started_ = true;
            const int shape = core.rand() & 3;
            if (shape == 0) return false;
            x_ = core.rand();
            const int random = core.rand() & 0x7F;
            y_ = (random + 0x10 + (core.randCarry() ? 1 : 0)) & 0xFF;
            core.initAnim(anim_, core.data.wordAt(kBoltAnims + static_cast<uint32_t>(shape) * 2));
        }
        if (anim_.duration == 0xFE) return false;
        core.updateAnim(anim_);
        core.drawAnim(anim_, x_, y_);
        return true;
    }

private:
    bool started_ = false;
    int x_ = 0, y_ = 0;
    AnimState anim_;
};

// _7e55c8: now and then, the palette of the clouds flashes, and a bolt shows up.
class LightningTask : public CutsceneTask {
public:
    explicit LightningTask(int palette) : palette_(palette) {}
    bool run(CutsceneCore& core) override {
        switch (state_) {
            case 0:
                state_ = 1;
                delay_ = 0x80;
                core.initAnim(anim_, kLightningAnim);
                return true;
            case 1:
                if (delay_ != 0) {
                    if (delay_ == 1) core.createTask<BoltTask>(2);
                    --delay_;
                    return true;
                }
                if (anim_.duration == 0xFE) state_ = 2;
                core.updateAnim(anim_);
                core.loadPal(palette_, core.data.wordAt(anim_.table + static_cast<uint32_t>(anim_.index)));
                return true;
            default:
                delay_ = core.rand();
                state_ = 1;
                core.initAnim(anim_, kLightningAnim);
                return true;
        }
    }

private:
    int palette_;
    int state_ = 0;
    int delay_ = 0;
    AnimState anim_;
};

}  // namespace

TitleScene::TitleScene(const TitleData& data, Sound& sound) : core_(data, sound) {
    initHardware();
    sound.command(FF6_SOUND_PLAY_SONG, 2, 0);  // PlayTitleSong, which is silent at first
    core_.ppu.clearVram();
    loadGraphics();
    core_.hdma = [this](Ppu& ppu) { setLines(ppu); };
}

// InitHWRegs, InitRAM and the tables of _7e7864 and _7e7870.
void TitleScene::initHardware() {
    Ppu& ppu = core_.ppu;
    ppu.objsel = 0x63;
    ppu.bgsc = {0x03, 0x11, 0x19};
    ppu.nba12 = 0x23;
    ppu.nba34 = 0x22;
    ppu.tm = 0x1F;
    ppu.ts = 0x04;
    for (PpuLine& line : ppu.lines) {
        line.bgMode = 1;
        line.cgwsel = 0x02;
        line.cgadsub = 0x42;
    }
    std::fill(core_.ram.begin() + kBg1Table, core_.ram.begin() + kTableEnd, 0);
    for (int line = 0; line < 224; ++line) core_.setRamWord(kBg1Table + line * 4 + 2, kBg1BaseScroll);
    for (int line = 0; line < kBg1HiddenLines; ++line) core_.setRamWord(kBg1Table + (kBg1HiddenFirstLine + line) * 4, kBg1HiddenScroll);
}

void TitleScene::transfer(int destination, const uint8_t* source, size_t bytes, int add) {
    for (size_t i = 0; i + 1 < bytes; i += 2) {
        core_.ppu.vram[static_cast<size_t>(destination) + i / 2] = static_cast<uint16_t>((source[i] | source[i + 1] << 8) + add);
    }
}

// LoadTitleGfx.
void TitleScene::loadGraphics() {
    const uint8_t* gfx = core_.data.gfx.data();
    transfer(0x1900, gfx + 0x7930, 0x200, 0x1400);  // the flames of bg3, with the palette 5
    transfer(0x2000, gfx + 0x7B30, 0x6B0, 0);
    transfer(0x1100, gfx + 0x81E0, 0x200, 0x0080);  // the logo of bg2
    transfer(0x0000, gfx + 0x1BC0, 0x1800, 0);  // the picture of bg1
    transfer(0x4000, gfx + 0x33C0, 0x4200, 0);
    transfer(0x3000, core_.data.mapGfx[0].data(), 0x2000, 0);  // the sealed gate
    transfer(0x2800, gfx + 0x83E0, 0x800, 0);
    for (const int start : {0x0400, 0x0C00}) std::fill_n(core_.ppu.vram.begin() + start, 0x400, static_cast<uint16_t>(0x0777));
    transfer(0x0500, gfx + 0x8BE0, 0x200, 0x0777);
    transfer(0x6770, gfx + 0x8DE0, 0x1110, 0);  // the sprites: lightning
    transfer(0x7000, gfx + 0x9EF0, 0xFB8, 0);  // and the texts
}

// _7e7897: bg1, bg2 and bg3 take their scroll from the tables, line by line.
void TitleScene::setLines(Ppu& ppu) {
    const int tables[3] = {kBg1Table, kBg2Table, kBg3Table};
    for (size_t bg = 0; bg < 3; ++bg) {
        if (!(core_.hdmaMask & (0x04 << bg))) continue;
        for (size_t y = 0; y < ppu.lines.size(); ++y) {
            const int address = tables[bg] + static_cast<int>(std::min<size_t>(y, kTableLines - 1)) * 4;
            ppu.lines[y].hofs[bg] = core_.ramWord(address);
            ppu.lines[y].vofs[bg] = core_.ramWord(address + 2);
        }
    }
}

// _7e5306: the texts of the license, which the splash screen and the title show.
void TitleScene::createText() {
    core_.createTask<DefaultAnimTask>(0, kTextAnim1, kText1X << 8, kTextY << 8, 0, 0);
    core_.createTask<DefaultAnimTask>(0, kTextAnim2, kText2X << 8, kTextY << 8, 0, 0);
}

void TitleScene::splashState() {
    if (core_.state == 0) {
        createText();
        core_.loadPal(0x3000, kBlackPal);
        core_.loadPal(0x3020, kBlackPal);
        core_.loadPal(0x3120, kBlackPal);
        core_.createFadePal(1, 0x3120, kIntroPal);
        core_.createFadePal(1, 0x3000, kWhitePal);
        core_.createFadePal(1, 0x3020, kSplashPal);
        core_.scroll[0] = 0x0100;
        core_.timer = kSplashFrames;
        ++core_.state;
        core_.brightness = 0x0F;
    } else if (core_.timer == 0) {
        core_.state = 0xFF;
    }
}

void TitleScene::titleState() {
    CutsceneCore& core = core_;
    if (core.state != 0x09 && core.state != 0x0B && core.timer != 0) return;
    switch (core.state) {
        case 0x00:
            core.loadPal(0x3120, kDarkPal);
            core.loadPal(0x3140, kIntroPal);
            core.loadPal(0x3000, kBlackPal);
            core.loadPal(0x3020, kIntroPal0);
            for (const int destination : {0x30C0, 0x3040, 0x3060, 0x30E0, 0x30A0}) core.loadPal(destination, kBlackPal);
            core.createFadePal(4, 0x30C0, kLowerCloudsPal);
            core.createFadePal(4, 0x3040, kCloudsPal);
            core.createFadePal(4, 0x3060, kCloudsPal);
            core.createFadePal(4, 0x30E0, kTownPal1);
            core.createFadePal(4, 0x30A0, kUpperCloudsPal);
            core.createTask<StreamTask>(1);
            core.timer = 60;
            ++core.state;
            core.brightness = 0x0F;
            for (const int palette : {0x3040, 0x3060, 0x30C0}) core.createTask<LightningTask>(1, palette);
            break;
        case 0x01:
            ++core.state;
            core.timer = 1091;
            core.sound.command(FF6_SOUND_PLAY_SONG, 2, FF6_SOUND_FULL_VOLUME);
            break;
        case 0x02:
            core.hdmaMask |= 0x1C;
            core.timer = 30;
            ++core.state;
            break;
        case 0x03:
            core.createTask<WaveTask>(0, kBg2Table, 8, 0, false);
            core.createTask<WaveTask>(0, kBg3Table, 10, 0x10, true);
            core.createTask<EdgeTask>(0);
            core.createFadePal(2, 0x3000, kFlamesPal);
            core.createFadePal(1, 0x3020, kFlamesPal1);
            core.timer = 330;
            ++core.state;
            break;
        case 0x04:
            ++core.state;
            core.timer = 240;
            break;
        case 0x05:
            ++core.state;
            core.timer = 960;
            createText();
            core.createFadePal(4, 0x3120, kIntroPal);
            break;
        case 0x06:
            ++core.state;
            core.timer = 256;
            core.createFadePal(2, 0x3100, kDarkPal);
            core.createFadePal(4, 0x3120, kDarkPal);
            core.createFadePal(8, 0x3000, kDarkPal);
            core.createFadePal(8, 0x3020, kDarkPal);
            break;
        case 0x07:
            core.flags |= 0x40;
            ++core.state;
            core.timer = 1;
            core.createTask<TownLightsTask>(0);
            break;
        case 0x08:
            ++core.state;
            core.hdmaMask = 0;
            core.flags = 0x80;
            core.timer = 928;
            break;
        case 0x09:
            if (core.timer == 0) {
                core.timer = 480;
                ++core.state;
            } else if ((core.frame & 3) == 0) {
                ++core.scroll[1];
            }
            break;
        case 0x0A:
            ++core.state;
            break;
        default:
            core.state = 0xFF;
            skipped_ = false;
            break;
    }
}

void TitleScene::beginFade(Phase next) {
    core_.timer = kEndFadeFrames;
    core_.createTask<FadeOutTask>(0);
    phase_ = next;
}

void TitleScene::tick(const InputState& input) {
    core_.pressed = input.confirm;
    for (;;) {
        switch (phase_) {
            case Phase::Splash:
                if (core_.state & 0x80) {
                    beginFade(Phase::SplashFade);
                    continue;
                }
                splashState();
                core_.execTasks();
                core_.vblank();
                return;
            case Phase::SplashFade:
                if (core_.timer != 0) {
                    core_.execTasks();
                    core_.vblank();
                    return;
                }
                core_.state = 0;
                core_.scroll[0] = core_.scroll[1] = 0;
                core_.resetTasks();
                core_.ppu.forceBlank = true;
                phase_ = Phase::Title;
                continue;
            case Phase::Title:
                if (core_.state & 0x80) {
                    beginFade(Phase::TitleFade);
                    continue;
                }
                titleState();
                if (core_.pressed) {
                    skipped_ = true;
                    beginFade(Phase::TitleFade);
                    continue;
                }
                core_.execTasks();
                core_.vblank();
                return;
            case Phase::TitleFade:
                if (core_.timer != 0) {
                    core_.execTasks();
                    core_.vblank();
                    return;
                }
                core_.sound.command(FF6_SOUND_STOP_ALL, 0, 0);
                phase_ = Phase::Done;
                return;
            case Phase::Done:
                return;
        }
    }
}

}  // namespace ff6
