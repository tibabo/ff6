#include "cutscene/core.hpp"

#include <algorithm>

namespace ff6 {
namespace {

// UpdateFadeColor: each channel of `current` goes one step to the one of `target`. A channel goes down at once, but goes
// up only when it is at least `threshold` steps away, so the farthest channels start first.
uint16_t fadeColor(uint16_t current, uint16_t target, int threshold) {
    uint16_t result = 0;
    for (int shift = 0; shift <= 10; shift += 5) {
        int value = current >> shift & 31;
        const int difference = (target >> shift & 31) - value;
        if (difference < 0) {
            --value;
        } else if (difference > 0 && difference >= threshold) {
            ++value;
        }
        result = static_cast<uint16_t>(result | value << shift);
    }
    return result;
}

}  // namespace

CutsceneCore::CutsceneCore(const TitleData& titleData, Sound& soundOut) : data(titleData), sound(soundOut) {}

bool CutsceneCore::insertTask(int group, std::unique_ptr<CutsceneTask> task) {
    for (int slot = group * kGroupSlots; slot < kTaskSlots; ++slot) {
        if (!tasks_[static_cast<size_t>(slot)]) {
            tasks_[static_cast<size_t>(slot)] = std::move(task);
            ++taskCount_;
            return true;
        }
    }
    return false;
}

void CutsceneCore::resetTasks() {
    for (auto& task : tasks_) task.reset();
    taskCount_ = 0;
    ppu.sprites.clear();
}

void CutsceneCore::execTasks() {
    ppu.sprites.clear();
    spritesLeft_ = kMaxSprites;
    for (size_t slot = 0; slot < tasks_.size(); ++slot) {
        if (!tasks_[slot]) continue;
        if (!tasks_[slot]->run(*this)) {
            tasks_[slot].reset();
            --taskCount_;
        }
    }
}

uint8_t CutsceneCore::rand() {
    ++randomCounter_;
    const unsigned sum = static_cast<unsigned>(frame) + randomCounter_;
    randCarry_ = sum > 255;
    return data.random[sum & 255];
}

void CutsceneCore::loadPal(int destination, uint16_t source) {
    const size_t first = static_cast<size_t>(destination - kPaletteBase) / 2;
    for (size_t i = 0; i < 16; ++i) palette[(first + i) & 255] = data.wordAt(source + static_cast<uint32_t>(i) * 2);
}

void CutsceneCore::createFadePal(int speed, int destination, uint16_t source) {
    createTask<FadePalTask>(0, speed, destination, source);
}

void CutsceneCore::initAnim(AnimState& anim, uint16_t table) const {
    anim.table = table;
    anim.index = 0;
    anim.duration = data.byteAt(table + 2u);
}

void CutsceneCore::updateAnim(AnimState& anim) const {
    for (;;) {
        if (anim.duration == 0xFE) return;
        if (anim.duration == 0xFF) {
            anim.index = 0;
        } else if (anim.duration == 0) {
            anim.index = (anim.index + 3) & 0xFF;
        } else {
            --anim.duration;
            return;
        }
        anim.duration = data.byteAt(anim.table + static_cast<uint32_t>(anim.index) + 2);
    }
}

void CutsceneCore::drawAnim(const AnimState& anim, int x, int y) {
    const uint16_t frameAddress = data.wordAt(anim.table + static_cast<uint32_t>(anim.index));
    if (spritesLeft_ == 0) return;
    const uint8_t count = data.byteAt(frameAddress);
    for (int i = 0; i < count && spritesLeft_ > 0; ++i, --spritesLeft_) {
        const uint8_t* sprite = data.at(frameAddress + 1u + static_cast<uint32_t>(i) * 4);
        PpuSprite out;
        out.large = (sprite[0] & 0x80) != 0;
        out.x = ((sprite[0] & 0x7F) + x) & 0xFF;
        out.y = (sprite[1] + y) & 0xFF;
        out.attr = sprite[3];
        out.tile = sprite[2] | (sprite[3] & 1) << 8;
        ppu.sprites.push_back(out);
    }
}

void CutsceneCore::vblank() {
    for (size_t y = 0; y < ppu.lines.size(); ++y) {
        PpuLine& line = ppu.lines[y];
        for (size_t bg = 0; bg < 3; ++bg) {
            line.hofs[bg] = scroll[bg * 2];
            line.vofs[bg] = scroll[bg * 2 + 1];
        }
    }
    if (hdma) hdma(ppu);
    ppu.cgram = palette;
    if (dmaDestination != 0xFFFF && dmaSource) {
        for (size_t i = 0; i + 1 < dmaBytes; i += 2) {
            ppu.vram[(dmaDestination + i / 2) & (Ppu::kVramWords - 1)] = static_cast<uint16_t>(dmaSource[i] | dmaSource[i + 1] << 8);
        }
    }
    if (timer != 0) --timer;
    ++frame;
    ppu.brightness = brightness & 15;
    ppu.forceBlank = (brightness & 0x80) != 0;
}

bool FadePalTask::run(CutsceneCore& core) {
    if (threshold_ < 0) {  // the first frame only starts it
        threshold_ = 0x1F;
        return true;
    }
    if (counter_ != 0) {
        --counter_;
        return true;
    }
    counter_ = speed_;
    const size_t first = static_cast<size_t>(destination_ - CutsceneCore::kPaletteBase) / 2;
    for (size_t i = 0; i < 16; ++i) {
        const uint16_t target = core.data.wordAt(source_ + static_cast<uint32_t>(i) * 2);
        uint16_t& color = core.palette[(first + i) & 255];
        color = fadeColor(color, target, threshold_);
    }
    if (threshold_ == 0) return false;
    --threshold_;
    if (threshold_ == 0) return false;
    --counter_;
    return true;
}

bool DefaultAnimTask::run(CutsceneCore& core) {
    if (!started_) {
        started_ = true;
        core.initAnim(anim_, table_);
    }
    if (core.flags & 0x40) return false;
    x_ = static_cast<uint16_t>(x_ + speedX_);
    y_ = static_cast<uint16_t>(y_ + speedY_);
    core.updateAnim(anim_);
    core.drawAnim(anim_, x_ >> 8, y_ >> 8);
    return true;
}

bool FadeOutTask::run(CutsceneCore& core) {
    if (level_ < 0) level_ = 15;
    if (core.timer != 0) {
        core.brightness = static_cast<uint8_t>(level_);
        level_ = (level_ + 255) & 255;
        return true;
    }
    core.brightness = 0x80;
    return false;
}

bool TownLightsTask::run(CutsceneCore& core) {
    core.loadPal(0x30E0, (core.frame & 1) ? kTownPal2 : kTownPal1);
    return true;
}

}  // namespace ff6
