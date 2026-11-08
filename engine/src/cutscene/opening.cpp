#include "cutscene/opening.hpp"

#include <algorithm>

namespace ff6 {
namespace {

// Addresses in the cutscene program ($7e5000 on), by the names that the disassembly gives them.
constexpr uint16_t kFontPal = 0x6400;
constexpr uint16_t kGroundPals[3] = {0x6410, 0x6430, 0x6450};  // OpeningBGPal1 to 3
constexpr uint16_t kArmorPal = 0x6470;
constexpr uint16_t kSnowSpritePal = 0x6490;
constexpr uint16_t kTilePalettes = 0x64B8;  // OpeningTilePal: the palette of each tile of the ground
constexpr uint16_t kCreditsAnim = 0x5D47;
constexpr uint16_t kArmorAnims = 0x5B45;  // for each part of an armor: the sprites, then the offset
constexpr uint16_t kArmorOffsets = 0x6027;
constexpr uint16_t kCreditsScroll = 0x5F07;
constexpr uint16_t kSnowAnims = 0x5F6E;
constexpr int kMode7MapBytes = 0x0BC0;  // in the graphics: the map of the ground, four boxes of 32 x 32 tiles
constexpr int kSnowGfxBytes = 0x0940, kSnowGfxSize = 0x400;
constexpr int kHorizon = 84;  // the lines above it are mode 1, the ones below it mode 7
constexpr int kMode7Lines = 224;
constexpr int kMode7TableAB = 0x3D00, kMode7TableCD = 0x4080;  // four bytes for each line
constexpr int kHdmaOffsetAB = 0x3CA0, kHdmaOffsetCD = 0x4020;  // where the HDMA starts: 24 lines before the tables
constexpr int kMode7Divisor = 292, kMode7Dividend = 0x4000;
constexpr int kCreditsRam = 0x4400, kCreditsVram = 0x7440, kCreditsBytes = 0x280;
constexpr int kStartFrames = 202, kCreditsFrames = 3300, kEndFadeFrames = 15;

// The fixed color that darkens the ground toward the horizon: for how many lines, then its intensity.
constexpr int kFixedSteps[][2] = {{84, 0}, {1, 13}, {1, 11}, {1, 10}, {2, 9}, {3, 8}, {4, 7}, {5, 6}, {6, 5}, {7, 4}, {8, 3}, {12, 2}, {15, 1}, {30, 0}};
constexpr int kBg3Bands[4] = {39, 16, 16, 16};  // the lines of each band of the credits, which scroll one by one

// Draws the credits that are at `text` in the map of bg3 that the DMA copies every frame (DrawCreditsText).
void drawCredits(CutsceneCore& core, OpeningScene::Shared& shared, uint16_t text) {
    uint16_t destination = core.data.wordAt(text);
    const uint8_t attributes = core.data.byteAt(text + 2u);
    const unsigned scroll = core.data.byteAt(text + 3u);
    for (unsigned band = 0; band < 4; ++band) {
        shared.bg3Scroll[band] = static_cast<uint8_t>(~core.data.byteAt(kCreditsScroll + scroll * 4 + band));
    }
    unsigned column = 0;
    for (uint32_t at = text + 4u;; ++at) {
        const uint8_t code = core.data.byteAt(at);
        if (code == 0) return;
        if (code == 0xFE || code == 0xFF) {  // the next line, or the one that is just after
            destination = static_cast<uint16_t>(destination + (code == 0xFE ? 0x80 : 0x40));
            column = 0;
            continue;
        }
        core.ram[(destination + column) & 0xFFFF] = code;
        core.ram[(destination + column + 1) & 0xFFFF] = attributes;
        column += 2;
    }
}

// The armors bob up and down together (ArmorOffsetTask).
class ArmorOffsetTask : public CutsceneTask {
public:
    explicit ArmorOffsetTask(OpeningScene::Shared& shared) : shared_(shared) {}
    bool run(CutsceneCore& core) override {
        for (;;) {
            if (duration_ == 0xFF) {
                index_ = 0;
            } else if (duration_ == 0) {
                index_ += 2;
            } else {
                --duration_;
                break;
            }
            duration_ = core.data.byteAt(kArmorOffsets + static_cast<uint32_t>(index_) + 1);
        }
        shared_.armorOffsetY = core.data.byteAt(kArmorOffsets + static_cast<uint32_t>(index_));
        return true;
    }

private:
    OpeningScene::Shared& shared_;
    int index_ = 0, duration_ = 8;
};

// One part of an armor (the pelvis, the body, the legs, the arms), which walks with the others (ArmorTask).
class ArmorTask : public CutsceneTask {
public:
    ArmorTask(OpeningScene::Shared& shared, int armor, int part) : shared_(shared), armor_(armor), part_(part) {}
    bool run(CutsceneCore& core) override {
        const uint32_t entry = kArmorAnims + static_cast<uint32_t>(part_) * 4;
        if (!started_) {
            started_ = true;
            core.initAnim(anim_, core.data.wordAt(entry));
            anim_.duration += armor_ * 8;  // the armors do not start together
        }
        const size_t armor = static_cast<size_t>(armor_);
        const int x = (core.data.byteAt(entry + 2) + shared_.armorX[armor]) & 0xFF;
        const int y = (shared_.armorOffsetY + core.data.byteAt(entry + 3) + shared_.armorY[armor]) & 0xFF;
        core.updateAnim(anim_);
        core.drawAnim(anim_, x, y);
        return true;
    }

private:
    OpeningScene::Shared& shared_;
    int armor_, part_;
    bool started_ = false;
    AnimState anim_;
};

// An armor goes up the screen for a while (ArmorMoveTask).
class ArmorMoveTask : public CutsceneTask {
public:
    ArmorMoveTask(OpeningScene::Shared& shared, int armor, int delay, int duration, uint16_t speed)
        : shared_(shared), armor_(static_cast<size_t>(armor)), counter_(delay), duration_(duration), speed_(speed) {}
    bool run(CutsceneCore&) override {
        if (state_ == 0) {
            y_ = static_cast<uint16_t>(shared_.armorY[armor_] << 8);
            state_ = 1;
        }
        if (state_ == 1) {
            if (counter_ != 0) {
                --counter_;
            } else {
                state_ = 2;
                counter_ = duration_;
            }
            return true;
        }
        if (counter_ == 0) return false;
        y_ = static_cast<uint16_t>(y_ + speed_);
        shared_.armorY[armor_] = static_cast<uint8_t>(y_ >> 8);
        --counter_;
        return true;
    }

private:
    OpeningScene::Shared& shared_;
    size_t armor_;
    int counter_, duration_;
    uint16_t speed_;
    uint16_t y_ = 0;
    int state_ = 0;
};

// _7e5aed: the ground and the town move, a little every few frames.
class ScrollTask : public CutsceneTask {
public:
    explicit ScrollTask(OpeningScene::Shared& shared) : shared_(shared) {}
    bool run(CutsceneCore& core) override {
        if ((core.frame & 3) == 0) {
            --shared_.centerY;
            --core.scroll[1];
        }
        if ((core.frame & 0x3F) == 0) ++core.scroll[3];
        return true;
    }

private:
    OpeningScene::Shared& shared_;
};

// A new snowflake every frame, while there are not too many threads (OpeningSnowTask).
class SnowTask : public CutsceneTask {
public:
    bool run(CutsceneCore& core) override {
        if (core.taskCount() >= 0x77) return true;
        core.flags &= static_cast<uint8_t>(~0x40);
        const uint16_t table = core.data.wordAt(kSnowAnims + static_cast<uint32_t>(core.frame & 7) * 2);
        const int first = core.rand(), second = core.rand();
        const int slow = (second & 3) + 1;
        core.createTask<DefaultAnimTask>(0, table, static_cast<uint16_t>(first << 8), static_cast<uint16_t>(second << 8),
                                         static_cast<uint16_t>(slow << 8 | slow), static_cast<uint16_t>(((first & 0x0F) + 1) << 8 | second));
        return true;
    }
};

// The names of the makers: each one fades in, stays, fades out (CreditsTextTask).
class CreditsTask : public CutsceneTask {
public:
    explicit CreditsTask(OpeningScene::Shared& shared) : shared_(shared) {}
    bool run(CutsceneCore& core) override {
        if (!started_) {
            started_ = true;
            core.initAnim(anim_, kCreditsAnim);
        }
        if (anim_.duration == 1) core.createFadePal(1, 0x3000, kFontPal);
        if (anim_.duration == 0x20) core.createFadePal(1, 0x3000, kBlackPal);
        if (anim_.duration == 0xFE) return false;
        core.updateAnim(anim_);
        drawCredits(core, shared_, core.data.wordAt(anim_.table + static_cast<uint32_t>(anim_.index)));
        return true;
    }

private:
    OpeningScene::Shared& shared_;
    bool started_ = false;
    AnimState anim_;
};

}  // namespace

OpeningScene::OpeningScene(const TitleData& data, Sound& sound) : core_(data, sound) {
    initHardware();
    sound.command(FF6_SOUND_PLAY_SONG, 4, FF6_SOUND_FULL_VOLUME);
    core_.ppu.clearVram();
    loadGraphics();
    core_.hdma = [this](Ppu& ppu) { setLines(ppu); };
}

void OpeningScene::initHardware() {
    Ppu& ppu = core_.ppu;
    ppu.objsel = 0x03;
    ppu.bgsc = {0x78, 0x7C, 0x74};
    ppu.nba12 = 0x22;
    ppu.nba34 = 0x77;
    ppu.tm = 0x17;
    ppu.ts = 0x02;
}

// Draws 128 tiles of the ground in the characters of mode 7 (one byte for each pixel, in the high byte of the words),
// then the map of the ground (one tile number for each word, in the low byte), and copies them to the video memory.
void OpeningScene::buildMode7(int firstTile, const std::vector<uint8_t>& tiles, int destination) {
    std::array<uint16_t, 0x2000> words{};
    const TitleData& data = core_.data;
    for (int tile = 0; tile < 128; ++tile) {
        const uint8_t palette = data.byteAt(kTilePalettes + static_cast<uint32_t>(firstTile + tile));
        for (int row = 0; row < 8; ++row) {
            const uint8_t* planes = &tiles[static_cast<size_t>(tile) * 32 + static_cast<size_t>(row) * 2];
            for (int pixel = 0; pixel < 8; ++pixel) {
                const int bit = 7 - pixel;
                int color = (planes[0] >> bit & 1) | ((planes[1] >> bit & 1) << 1) | ((planes[16] >> bit & 1) << 2) | ((planes[17] >> bit & 1) << 3);
                if (color != 0) color |= (palette << 4) & 0x30;
                words[static_cast<size_t>(tile * 64 + row * 8 + pixel)] = static_cast<uint16_t>(color << 8);
            }
        }
    }
    // The boxes of the map: where they start in the source, then where they go (in bytes of the video memory).
    static constexpr int kBoxes[8][2] = {{0x0000, 0x0000}, {0x0400, 0x0040}, {0x0000, 0x0080}, {0x0400, 0x00C0},
                                         {0x0800, 0x2000}, {0x0C00, 0x2040}, {0x0800, 0x2080}, {0x0C00, 0x20C0}};
    for (const auto& box : kBoxes) {
        for (int row = 0; row < 32; ++row) {
            for (int column = 0; column < 32; ++column) {
                words[static_cast<size_t>(box[1] / 2 + row * 128 + column)] |= data.gfx[static_cast<size_t>(kMode7MapBytes + box[0] + row * 32 + column)];
            }
        }
    }
    std::copy(words.begin(), words.end(), core_.ppu.vram.begin() + destination);
}

// LoadOpeningGfx and InitOpeningGfx.
void OpeningScene::loadGraphics() {
    const TitleData& data = core_.data;
    const uint8_t* gfx = data.gfx.data();
    const auto transfer = [&](int destination, const uint8_t* source, size_t bytes, int add) {
        for (size_t i = 0; i + 1 < bytes; i += 2) {
            core_.ppu.vram[static_cast<size_t>(destination) + i / 2] = static_cast<uint16_t>((source[i] | source[i + 1] << 8) + add);
        }
    };
    transfer(0x6400, gfx, 0x940, 0);  // the armors
    transfer(0x4000, gfx + 0x53C0, 0x2200, 0);  // the town
    transfer(0x7C00, gfx + 0x23C0, 0x800, 0x2000);
    std::fill_n(core_.ppu.vram.begin() + 0x7C00, 0x220, static_cast<uint16_t>(0x3E87));  // the sky
    transfer(0x7000, gfx + 0x75C0, 0x370, 0);  // the letters
    // The characters of the ground: 64 tiles of two graphics of Narshe, then 64 tiles of another one, the snow, and nothing.
    std::vector<uint8_t> first(0x1000, 0), second(0x1000, 0);
    std::copy_n(data.mapGfx[1].begin(), 0x800, first.begin());
    std::copy_n(data.mapGfx[2].begin() + 0x800, 0x800, first.begin() + 0x800);
    std::copy_n(data.mapGfx[3].begin(), 0x800, second.begin());
    std::copy_n(data.gfx.begin() + kSnowGfxBytes, kSnowGfxSize, second.begin() + 0x800);
    buildMode7(0, first, 0x0000);
    buildMode7(0x80, second, 0x2000);
}

// The registers that change from line to line: the mode, the color math, the fixed color, the scroll of the credits and
// the matrix of mode 7 (the HDMA tables of InitOpeningHDMA).
void OpeningScene::setLines(Ppu& ppu) {
    std::array<int, kMode7Lines> fixed{};
    size_t line = 0;
    for (const auto& step : kFixedSteps) {
        for (int i = 0; i < step[0] && line < fixed.size(); ++i) fixed[line++] = step[1];
    }
    for (size_t y = 0; y < ppu.lines.size(); ++y) {
        PpuLine& out = ppu.lines[y];
        const bool above = static_cast<int>(y) < kHorizon;
        out.bgMode = above ? 0x09 : 0x07;
        out.cgwsel = above ? 0x82 : 0x80;
        out.cgadsub = above ? 0x04 : 0x81;
        const uint8_t intensity = static_cast<uint8_t>(fixed[y]);
        out.fixed = {intensity, intensity, intensity};
        int band = 0, edge = kBg3Bands[0];
        while (band < 3 && static_cast<int>(y) >= edge) edge += kBg3Bands[++band];
        out.hofs[2] = shared_.bg3Scroll[static_cast<size_t>(band)];
        const int row = static_cast<int>(std::min<size_t>(y, 222)) * 4;
        out.m7a = static_cast<int16_t>(core_.ramWord(kHdmaOffsetAB + row));
        out.m7b = static_cast<int16_t>(core_.ramWord(kHdmaOffsetAB + row + 2));
        out.m7c = static_cast<int16_t>(core_.ramWord(kHdmaOffsetCD + row));
        out.m7d = static_cast<int16_t>(core_.ramWord(kHdmaOffsetCD + row + 2));
    }
    ppu.m7hofs = core_.scroll[0];
    ppu.m7vofs = core_.scroll[1];
}

void OpeningScene::updateMode7Position() {
    core_.ppu.m7x = shared_.centerX;
    core_.ppu.m7y = shared_.centerY;
}

void OpeningScene::openingState() {
    CutsceneCore& core = core_;
    switch (core.state) {
        case 0x00: {
            // InitOpeningMode7Vars, InitOpeningHDMA and the tables of the ground in perspective.
            shared_.centerX = 0x100;
            shared_.centerY = 0x80;
            core.scroll[0] = 0x80;
            core.scroll[1] = 0;
            core.dmaDestination = kCreditsVram;
            core.dmaSource = &core.ram[kCreditsRam];
            core.dmaBytes = kCreditsBytes;
            core.hdmaMask |= 0xFC;
            for (int k = 0; k < 224; ++k) {
                const unsigned divisor = static_cast<unsigned>(k * kMode7Divisor) >> 8 & 0xFF;
                const uint16_t scale = divisor == 0 ? 0xFFFF : static_cast<uint16_t>(kMode7Dividend / divisor);
                core.setRamWord(kMode7TableAB + k * 4, scale);
                core.setRamWord(kMode7TableAB + k * 4 + 2, 0);
                core.setRamWord(kMode7TableCD + k * 4, 0);
                core.setRamWord(kMode7TableCD + k * 4 + 2, scale);
            }
            core.resetTasks();
            for (const int palette : {0x3100, 0x3120, 0x3160, 0x3180, 0x31A0, 0x3000, 0x3020, 0x3040, 0x3060, 0x30E0}) {
                core.loadPal(palette, kBlackPal);
            }
            core.scroll[2] = 0;
            core.scroll[3] = 0x38;
            shared_.armorOffsetY = 0;
            core.createTask<ArmorOffsetTask>(0, shared_);
            for (int armor = 0; armor < 3; ++armor) {
                for (int part = 0; part < 6; ++part) core.createTask<ArmorTask>(2, shared_, armor, part);
            }
            core.createFadePal(1, 0x3120, kSnowSpritePal);
            core.createFadePal(2, 0x3160, kSnowSpritePal);
            core.createFadePal(4, 0x3180, kSnowSpritePal);
            core.createFadePal(6, 0x31A0, kSnowSpritePal);
            core.createFadePal(3, 0x3100, kArmorPal);
            core.createFadePal(2, 0x3020, kGroundPals[0]);
            core.createFadePal(2, 0x3040, kGroundPals[1]);
            core.createFadePal(2, 0x3060, kGroundPals[2]);
            core.createFadePal(4, 0x30E0, kTownPal1);
            core.createTask<SnowTask>(0);
            core.createTask<ScrollTask>(0, shared_);
            core.createTask<ArmorMoveTask>(0, shared_, 0, 1, 0x5A, static_cast<uint16_t>(0xFFA0));
            core.createTask<ArmorMoveTask>(0, shared_, 1, 0x0A, 0x28, static_cast<uint16_t>(0xFF60));
            core.createTask<ArmorMoveTask>(0, shared_, 2, 0x07, 0x2D, static_cast<uint16_t>(0xFF80));
            ++core.state;
            core.brightness = 1;
            core.vblank();
            core.timer = kStartFrames;
            core.brightness = 0x0F;
            break;
        }
        case 0x01:
            if (core.timer == 0) {
                ++core.state;
                core.timer = kCreditsFrames;
                core.createTask<CreditsTask>(0, shared_);
                core.createTask<TownLightsTask>(0);
            }
            updateMode7Position();
            break;
        case 0x02:
            if (core.timer == 0) ++core.state;
            updateMode7Position();
            break;
        default:
            core.state = 0xFF;
            skipped_ = false;
            break;
    }
}

void OpeningScene::tick(const InputState& input) {
    core_.pressed = input.confirm;
    for (;;) {
        switch (phase_) {
            case Phase::Run:
                if (core_.state & 0x80) {
                    core_.timer = kEndFadeFrames;
                    core_.createTask<FadeOutTask>(0);
                    phase_ = Phase::Fade;
                    continue;
                }
                openingState();
                if (core_.pressed) {
                    skipped_ = true;
                    core_.state = 0xFF;
                    continue;
                }
                core_.execTasks();
                core_.vblank();
                return;
            case Phase::Fade:
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
