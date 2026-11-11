#include "cutscene/ending_tasks.hpp"

#include <algorithm>

namespace ff6 {
namespace {

constexpr int kEnding1Base = 0x8A70, kEnding2Base = 0xF450, kCoinBase = 0xEC93, kTextBase = 0x1000;
constexpr int kAnimFast = 0xFE, kAnimLoop = 0xFF;
constexpr int kMiniMoogleJumpOffset = 0xF55E;
// The addresses in the first block of the animations (C2) and in the second (CF) that the tasks name.
constexpr int kTerraPendantAnim = 0xF5B4, kCyanKatanaAnim = 0xF5FA, kEdgarCoinAnim = 0xEC93;
constexpr int kNameTable = 0x8DE4, kAsAnim = 0x8E38, kAsX = 0x79, kAsY = 0xC0, kAsFrames = 10 * 60, kNameY = 0xD0;
constexpr int kUmaroMiniMoogleAnim1 = 0xF458, kUmaroMiniMoogleAnim2 = 0xF473, kUmaroMiniMoogleAnim3 = 0xF47C;
constexpr int kGogoSparkle = 0x957C;

}  // namespace

uint8_t EndingTasks::byte(int bank, int address) const {
    const std::vector<uint8_t>* block = nullptr;
    int base = 0;
    switch (bank) {
        case kBankEnding1: block = &data_.endingAnim; base = kEnding1Base; break;
        case kBankEnding2: block = &data_.endingAnim2; base = kEnding2Base; break;
        case kBankCoin: block = &data_.endingCoin; base = kCoinBase; break;
        case kBankText: block = &text_; base = kTextBase; break;
        default: return 0;
    }
    const int at = (address & 0xFFFF) - base;
    return at >= 0 && at < static_cast<int>(block->size()) ? (*block)[static_cast<size_t>(at)] : 0;
}

std::array<uint16_t, 16> EndingTasks::palette(int address) const {
    std::array<uint16_t, 16> colors{};
    for (size_t i = 0; i < colors.size(); ++i) colors[i] = word(kBankEnding1, address + static_cast<int>(i) * 2);
    return colors;
}

int EndingTasks::active() const {
    return static_cast<int>(std::count_if(slots_.begin(), slots_.end(), [](const Task& task) { return task.code != nullptr; }));
}

EndingTasks::Task& EndingTasks::create(int priority, Code code) {
    for (int slot = priority * kSlotsPerPriority; slot < kSlots; ++slot) {
        if (slots_[static_cast<size_t>(slot)].code) continue;
        slots_[static_cast<size_t>(slot)] = Task();
        slots_[static_cast<size_t>(slot)].code = code;
        return slots_[static_cast<size_t>(slot)];
    }
    return slots_.back();  // the game stops here for good
}

void EndingTasks::exec(std::vector<PpuSprite>& sprites) {
    sprites.clear();
    out_ = &sprites;
    for (int slot = 0; slot < kSlots; ++slot) {
        Task& task = slots_[static_cast<size_t>(slot)];
        if (!task.code) continue;
        current_ = slot;
        if (!(this->*task.code)(task)) task = Task();
    }
    out_ = nullptr;
}

// InitAnimTask: back to the first entry of the table, whose counter is its third byte.
void EndingTasks::initAnimation(Task& task) const {
    task.offset = 0;
    task.animCounter = byte(task.bank, task.ptr + 2);
}

// UpdateAnimData: a counter of 0xFE stays, 0xFF starts again, and at zero the next entry (three bytes) begins.
void EndingTasks::updateAnimData(Task& task) const {
    for (;;) {
        if (task.animCounter == kAnimFast) return;
        if (task.animCounter == kAnimLoop) {
            task.offset = 0;
            task.animCounter = byte(task.bank, task.ptr + task.offset + 2);
            continue;
        }
        if (task.animCounter != 0) {
            --task.animCounter;
            return;
        }
        task.offset = (task.offset + 3) & 0xFF;
        task.animCounter = byte(task.bank, task.ptr + task.offset + 2);
    }
}

// UpdateAnimSprites and DrawAnimSprite: the list of sprites of the entry, moved by the position of the task (and by the
// scroll of the first background for the tasks that follow it), with the palette that the task asks for.
void EndingTasks::updateAnimSprites(const Task& task) {
    if (!out_) return;
    const int list = word(task.bank, task.ptr + task.offset);
    int count = byte(task.bank, list);
    int at = list + 1;
    while (count-- > 0 && static_cast<int>(out_->size()) < kMaxSprites) {
        const int first = byte(task.bank, at), y = byte(task.bank, at + 1), tile = byte(task.bank, at + 2);
        int attr = byte(task.bank, at + 3);
        at += 4;
        int x = first & 0x7F;
        if (task.flags & 1) x -= bg1HScroll;
        x = (x + (task.posX >> 8)) & 0xFF;
        if (task.flags & 2) attr |= 0x40;
        if (task.pal) attr = (attr & 0xF1) | task.pal;
        PpuSprite sprite;
        sprite.large = (first & 0x80) != 0;
        sprite.x = x;
        sprite.y = (y + (task.posY >> 8) + 1) & 0xFF;
        sprite.attr = static_cast<uint8_t>(attr);
        sprite.tile = tile | (attr & 1) << 8;
        out_->push_back(sprite);
    }
}

// FadePalTask: 31 steps at the speed that the task has; a color goes up by a step once what is left to climb is no more
// than the steps that remain, and goes down a step at once.
void EndingTasks::fadePalette(int firstColor, const std::array<uint16_t, 16>& target, int speed) {
    Task& task = create(0, &EndingTasks::fadeTask);
    task.fadeFirst = firstColor;
    task.fadeSpeed = speed;
    task.fadeTarget = target;
}

bool EndingTasks::fadeTask(Task& task) {
    if (task.state == 0) {
        task.counter = 0x1F;
        task.state = 1;
        return true;
    }
    if (task.frames == 0) {
        task.frames = task.fadeSpeed;
        for (size_t i = 0; i < 16; ++i) {
            uint16_t& color = cgram_[static_cast<size_t>(task.fadeFirst) + i];
            const uint16_t goal = task.fadeTarget[i];
            uint16_t next = 0;
            for (int shift = 0; shift <= 10; shift += 5) {
                int now = color >> shift & 31;
                const int wanted = goal >> shift & 31;
                if (wanted < now) {
                    --now;
                } else if (wanted > now && wanted - now >= task.counter) {
                    ++now;
                }
                next = static_cast<uint16_t>(next | now << shift);
            }
            color = next;
        }
        if (task.counter == 0 || --task.counter == 0) return false;
    }
    --task.frames;
    return true;
}

// The palettes of the font of the credits, in the sprite palettes 4 (the big letters) and 5 (the small ones), at 0xC0 and 0xD0.
constexpr int kCreditsPal4 = 0xC0, kCreditsPal5 = 0xD0, kCreditsFadeSpeed = 4, kCreditsLife = 420, kCreditsStill = 0x164, kCreditsFadeOut = 0x80;
constexpr int kCreditsNormalPal4 = 0x9744, kCreditsNormalPal5 = 0x973C, kCreditsInversePal4 = 0x9754, kCreditsInversePal5 = 0x974C;

// CreateCreditsTextTask: each letter of the word is a sprite 8 pixels after the last (a byte, so the ones that pass 127 are
// the big ones for the game too); the big font is 16x16 sprites of the tiles of the second table with the palette 4, the small
// font 8x8 sprites with the palette 5.
void EndingTasks::createCreditsWord(const std::string& codes, bool big, int x, int y) {
    const int address = kTextBase + static_cast<int>(text_.size());
    text_.push_back(static_cast<uint8_t>((address + 3) & 0xFF));
    text_.push_back(static_cast<uint8_t>((address + 3) >> 8));
    text_.push_back(kAnimFast);
    text_.push_back(static_cast<uint8_t>(codes.size()));
    for (size_t i = 0; i < codes.size(); ++i) {
        text_.push_back(static_cast<uint8_t>((i * 8 & 0xFF) | (big ? 0x80 : 0)));
        text_.push_back(0);
        text_.push_back(static_cast<uint8_t>(codes[i]));
        text_.push_back(big ? 0x09 : 0x0B);
    }
    Task& task = create(0, &EndingTasks::creditsWordTask);
    task.counter = kCreditsLife;
    task.posX = (x & 0xFF) << 8;
    task.posY = (y & 0xFF) << 8;
    setAnimation(task, kBankText, address);
    creditsShown_ = false;
}

// The word starts 32 pixels low and goes up half a pixel a frame for 64 frames; the first word of the page to run brings the
// palettes in, and the first one to have 128 frames left takes them out.
bool EndingTasks::creditsWordTask(Task& task) {
    if (task.state == 0) {
        task.state = 1;
        task.posY = (task.posY + 0x2000) & 0xFFFF;
        task.speedX = 0;
        task.speedY = 0xFF80;
        initAnimation(task);
        if (!creditsShown_) {
            creditsShown_ = true;
            fadePalette(kCreditsPal4, palette(creditsNormal ? kCreditsNormalPal4 : kCreditsInversePal4), kCreditsFadeSpeed);
            fadePalette(kCreditsPal5, palette(creditsNormal ? kCreditsNormalPal5 : kCreditsInversePal5), kCreditsFadeSpeed);
        }
    }
    if (task.counter == 0) return false;
    if (task.counter == kCreditsStill) task.speedY = 0;
    if (task.counter == kCreditsFadeOut && creditsShown_) {
        creditsShown_ = false;
        blackPalette(kCreditsPal4, kCreditsFadeSpeed);
        blackPalette(kCreditsPal5, kCreditsFadeSpeed);
    }
    updateEndingAnim(task);
    task.counter = (task.counter - 1) & 0xFFFF;
    return true;
}

void EndingTasks::createBigName(int character) {
    const int entry = kNameTable + character * 6;
    for (int half = 0; half < 2; ++half) {
        Task& task = create(1, &EndingTasks::bigTextTask);
        setAnimation(task, kBankEnding1, word(kBankEnding1, entry + half * 3));
        task.posX = byte(kBankEnding1, entry + half * 3 + 2) << 8;
        task.posY = kNameY << 8;
    }
}

void EndingTasks::createAs() {
    Task& task = create(3, &EndingTasks::animCounterTask);
    setAnimation(task, kBankEnding1, kAsAnim);
    task.counter = kAsFrames;
    task.posX = kAsX << 8;
    task.posY = kAsY << 8;
}

// The letters wait, rise for 2.5 seconds, stay for 4, rise out for 6 and go; each wait counts its frames down in the counter.
bool EndingTasks::bigTextTask(Task& task) {
    if (bigNameStop || task.state == 8) return false;
    const auto wait = [&] {
        if (task.counter == 0) {
            ++task.state;
        } else {
            --task.counter;
        }
        updateEndingAnim(task);
        return true;
    };
    const auto move = [&](int speed, int frames) {
        task.speedY = speed;
        task.counter = frames;
        ++task.state;
        return wait();
    };
    switch (task.state) {
        case 0:
            ++task.state;
            task.speedY = 0;
            task.counter = bigNameWait;
            initAnimation(task);
            return wait();
        case 2: return move(0xFFC0, 0x96);
        case 4: return move(0, 0xF0);
        case 6: return move(0xFFC0, 0x12C);
        default: return wait();
    }
}

bool EndingTasks::animTask(Task& task) {
    if (task.state == 0) {
        task.state = 1;
        initAnimation(task);
    }
    updateEndingAnim(task);
    return true;
}

bool EndingTasks::animCounterTask(Task& task) {
    if (task.state == 0) {
        task.state = 1;
        initAnimation(task);
    }
    if (task.counter == 0) return false;
    updateEndingAnim(task);
    task.counter = (task.counter - 1) & 0xFFFF;
    return true;
}

bool EndingTasks::bg1ScrollTask(Task& task) {
    if (task.counter == 0) return false;
    --task.counter;
    if ((frameCounter & 3) == 0) bg1HScroll = (bg1HScroll + 1) & 0xFFFF;
    return true;
}

bool EndingTasks::bg3ScrollTask(Task&) {
    if (frameCounter == bg3Target) return false;
    if ((frameCounter & 3) == 0) bg3VScroll = (bg3VScroll - 1) & 0xFFFF;
    return true;
}

// The coin of Edgar: half a pixel a frame to the right for 200 frames (a byte of the counter, which wraps and stops it again).
bool EndingTasks::coinTask(Task& task) {
    if (task.state == 0) {
        task.state = 1;
        setAnimation(task, kBankCoin, kEdgarCoinAnim);
        task.speedX = 0x80;
        task.counter = (task.counter & 0xFF00) | 0xC8;
        task.posX = 0x1000 | (task.posX & 0xFF);
        task.posY = 0x6400 | (task.posY & 0xFF);
        initAnimation(task);
    }
    if ((task.counter & 0xFF) == 0) task.speedX &= 0xFF00;
    task.counter = (task.counter & 0xFF00) | ((task.counter - 1) & 0xFF);
    updateEndingAnim(task);
    return true;
}

namespace {

// The tasks of Relm and of Cyan that leave sparkles behind: after 360 frames, one every eight, from right to left.
bool sparklesTask(EndingTasks& tasks, EndingTasks::Task& task, int x, int count, int y, int anim, EndingTasks::Code sparkle) {
    if (task.state == 0) {
        task.state = 1;
        task.counter = 0x168;
        task.param = x;
        task.remaining = count;
    }
    if (task.remaining == 0) return false;
    if (task.counter == 0) {
        task.counter = 8;
        EndingTasks::Task& made = tasks.create(2, sparkle);
        made.posY = y << 8;
        tasks.setAnimation(made, EndingTasks::kBankEnding2, anim);
        made.posX = (task.param & 0xFF) << 8;
        task.param = (task.param - 4) & 0xFF;
        --task.remaining;
    }
    task.counter = (task.counter - 1) & 0xFFFF;
    return true;
}

}  // namespace

bool EndingTasks::relmBrushTask(Task& task) { return sparklesTask(*this, task, 0xE4, 3, 0x68, kTerraPendantAnim, &EndingTasks::sparkleTask); }

bool EndingTasks::cyanSwordTask(Task& task) { return sparklesTask(*this, task, 0xC8, 4, 0x60, kCyanKatanaAnim, &EndingTasks::sparkleTask); }

bool EndingTasks::sparkleTask(Task& task) {
    if (task.state == 0) {
        task.state = 1;
        task.flags = 1;
        initAnimation(task);
    }
    if (task.animCounter == kAnimFast) return false;
    updateEndingAnim(task);
    return true;
}

// The glimmer of the helmet of Gogo: the palette of the sprites goes to light and, a second later, back to black.
bool EndingTasks::gogoGlimmerTask(Task& task) {
    switch (task.state) {
        case 0:
        case 2:
            if ((task.counter & 0xFF) == 0) ++task.state;
            task.counter = (task.counter & 0xFF00) | ((task.counter - 1) & 0xFF);
            return true;
        case 1:
            fadePalette(128 + 4 * 16, palette(kGogoSparkle), 1);
            task.counter = (task.counter & 0xFF00) | 0x3C;
            ++task.state;
            return true;
        default:
            blackPalette(128 + 4 * 16, 1);
            return false;
    }
}

// The mini-moogles of Mog and of Umaro.
bool EndingTasks::miniMoogleTask(Task& task) {
    const auto start = [&](int address) {
        setAnimation(task, kBankEnding2, address);
        initAnimation(task);
    };
    const auto walk = [&] {  // _c3ebd5
        ++task.state;
        setAnimation(task, kBankEnding2, kUmaroMiniMoogleAnim2);
        task.speedX = 0x40;
        initAnimation(task);
    };
    const auto step = [&] {
        task.counter = (task.counter - 1) & 0xFFFF;
        updateEndingAnim(task);
    };
    switch (task.state) {
        case 0:  // dances in place
            ++task.state;
            task.counter = 0x168;
            task.posY = (task.posY & 0xFF) | 0x6400;
            task.flags = 1;
            start(kUmaroMiniMoogleAnim1);
            [[fallthrough]];
        case 1:
            if (task.counter == 0) {
                ++task.state;
                task.counter = 0x10;
                start(kUmaroMiniMoogleAnim3);
            }
            step();
            return true;
        case 2: {  // jumps
            if (task.counter == 0) {
                ++task.state;
                task.counter = 0x3C;
            }
            task.counter = (task.counter - 1) & 0xFFFF;
            const int offset = static_cast<int8_t>(byte(kBankEnding2, kMiniMoogleJumpOffset + (task.frames & 0x0F)));
            task.posY = (task.posY & 0xFF) | (((task.posY >> 8) + offset) & 0xFF) << 8;
            updateEndingAnim(task);
            task.frames = (task.frames + 1) & 0xFF;
            return true;
        }
        case 3:
            if (task.counter == 0) {
                walk();
                task.counter = 0x12C;
            }
            step();
            return true;
        case 4:
        case 7:
            updateEndingAnim(task);
            return true;
        case 5:  // the ones of Umaro start by walking
            walk();
            task.posY = (task.posY & 0xFF) | 0x7000;
            task.flags = 1;
            [[fallthrough]];
        case 6:
            if (task.counter == 0) {
                ++task.state;
                task.speedX = 0;
                setAnimation(task, kBankEnding2, task.saved);
                initAnimation(task);
            }
            step();
            return true;
        default:
            updateEndingAnim(task);
            return true;
    }
}

// The pages of the book: the tiles that the entry of the animation names go over the picture.
bool EndingTasks::bookTask(Task& task) {
    if (task.state == 0) {
        task.state = 1;
        initAnimation(task);
        return true;
    }
    updateAnimData(task);
    const int descriptor = word(task.bank, task.ptr + task.offset);
    if (copyTiles) copyTiles(word(task.bank, descriptor + 4), word(task.bank, descriptor + 6), word(task.bank, descriptor), word(task.bank, descriptor + 2));
    return true;
}

int16_t EndingTasks::sine(int index) const {
    const size_t at = static_cast<size_t>(index & 0xFF) * 2;
    return at + 1 < data_.sine16.size() ? static_cast<int16_t>(data_.sine16[at] | data_.sine16[at + 1] << 8) : 0;
}

namespace {

// _c3cc31 over the sign of the sine: a sixteenth of it.
int sixteenth(int value) { return value < 0 ? -((-value) >> 4) : value >> 4; }

constexpr int kAirshipSplashAnim = 0x8D0B, kShipBird5 = 0xF74F;
// _c3ce6e: the steps of a bird that flies by: the speed across and down, the frames that it lasts.
constexpr int kFlyingBird[7][3] = {{0x0020, -0x10, 0x384}, {0x0020, 0, 0x3C}, {0x0020, 0x20, 0x3C}, {0x0010, 0x40, 0x3C},
                                   {-0x20, 0x40, 0x3C},   {-0x40, 0x40, 0xB4}, {-0x80, 0x80, 0xB4}};

}  // namespace

// A bird that sways: the phase of the sine that it starts from is a byte of the code of the game.
bool EndingTasks::oscillatingBirdTask(Task& task) {
    if (task.state == 0) {
        task.state = 1;
        const size_t at = static_cast<size_t>(current_) * 2;
        task.counter = at < data_.endingPhase.size() ? data_.endingPhase[at] : 0;
        task.bank = kBankEnding2;
        initAnimation(task);
    }
    const int cosine = sine((task.counter & 0xFFFF) + 0x40);
    task.posX = (sixteenth(cosine) + task.speedX) & 0xFFFF;
    task.counter = (task.counter & 0xFF00) | ((task.counter + 1) & 0xFF);
    updateEndingAnim(task);
    return true;
}

// The height that the sine gives to the airships: the base is in the speed of the task, which the update adds again.
static void bobAirship(const EndingTasks& tasks, EndingTasks::Task& task) {
    task.posY = (sixteenth(tasks.sine(task.counter & 0xFFFF)) + task.speedY) & 0xFFFF;
}

void EndingTasks::createSplash() {
    if (frameCounter & 3) return;
    Task& splash = create(1, &EndingTasks::animCounterTask);
    setAnimation(splash, kBankEnding1, kAirshipSplashAnim);
    splash.speedX = 0x100;
    splash.speedY = 0xFF80;
    splash.counter = 0x18;
    splash.posX = 0x7E00;
    splash.posY = 0x7C00;
}

bool EndingTasks::splashAirshipTask(Task& task) {
    if (task.state == 0) {
        task.state = 1;
        task.counter = 0;
        task.posX = (task.posX & 0xFF) | 0x7800;
        task.speedY = (task.speedY & 0xFF) | 0x2000;
        initAnimation(task);
    }
    bobAirship(*this, task);
    task.counter = (task.counter & 0xFF00) | ((task.counter + 1) & 0xFF);
    if ((task.counter & 0xFF) < 0x38) createSplash();
    updateEndingAnim(task);
    return true;
}

bool EndingTasks::airshipTask(Task& task) {
    if (task.state == 0) {
        task.state = 1;
        task.counter &= 0xFF00;
        initAnimation(task);
    }
    bobAirship(*this, task);
    task.counter = (task.counter & 0xFF00) | ((task.counter + 1) & 0xFF);
    updateAnimTask(task);
    return true;
}

bool EndingTasks::boatTask(Task& task) {
    if (task.state == 0) {
        task.state = 1;
        task.counter = 0x40;
        initAnimation(task);
    }
    if (task.counter != 0) {
        task.counter = (task.counter - 1) & 0xFFFF;
    } else {
        updateEndingAnim(task);
    }
    return true;
}

bool EndingTasks::flyingBirdTask(Task& task) {
    const auto next = [&] {  // _c3ce43
        const int* row = kFlyingBird[std::min(task.frames, 6)];
        ++task.frames;
        ++task.state;
        task.speedX = row[0] & 0xFFFF;
        task.speedY = row[1] & 0xFFFF;
        task.counter = row[2];
    };
    if (task.state == 0) {
        task.state = 1;
        setAnimation(task, kBankEnding2, kShipBird5);
        task.frames = 0;
        next();
        initAnimation(task);
    }
    if (task.state < 8) {
        if (task.counter == 0) next();
        task.counter = (task.counter - 1) & 0xFFFF;
        updateEndingAnim(task);
        return true;
    }
    updateEndingAnim(task);
    return (task.posX >> 8) >= 1;
}

}  // namespace ff6
