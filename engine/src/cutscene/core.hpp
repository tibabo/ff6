#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "cutscene/ppu.hpp"
#include "data/title_data.hpp"
#include "input.hpp"
#include "sound.hpp"

namespace ff6 {

class CutsceneCore;

// What the game asks of a cutscene (the title screen, the opening): one tick for each frame of the console.
class Cutscene {
public:
    virtual ~Cutscene() = default;
    virtual void tick(const InputState& input) = 0;
    // The screen as FF6_SCREEN_WIDTH x FF6_SCREEN_HEIGHT RGBA pixels.
    virtual void draw(uint8_t* rgba) = 0;
    virtual bool finished() const = 0;
    // A press of the confirm button ended it, which the story remembers.
    virtual bool skipped() const = 0;
};

// One of the threads of the cutscene program ($3200 on): ExecTasks runs them in the order of their slots.
class CutsceneTask {
public:
    virtual ~CutsceneTask() = default;
    // False ends the task.
    virtual bool run(CutsceneCore& core) = 0;
};

// The table of frames of an animated sprite: pairs of a pointer to a sprite and a duration in frames, with 0xFE after
// the last one to stay on it and 0xFF to start again.
struct AnimState {
    uint16_t table = 0;
    int index = 0;  // byte offset of the current pair
    int duration = 0;
};

// What the title screen and the opening have in common: the picture processor, the threads, the palette that is
// copied every frame, the sprites that the threads draw, and the order of a frame (the state function, ExecTasks, the
// vertical blank). The names of the game's memory are in the comments.
class CutsceneCore {
public:
    static constexpr int kTaskSlots = 128;
    static constexpr int kGroupSlots = 16;
    static constexpr int kMaxSprites = 128;
    static constexpr int kPaletteBase = 0x3000;

    CutsceneCore(const TitleData& data, Sound& sound);

    // Creates a thread in the first free slot from the group on (the group is its priority); null when none is free.
    template <class T, class... Args>
    T* createTask(int group, Args&&... args) {
        auto task = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = task.get();
        return insertTask(group, std::move(task)) ? raw : nullptr;
    }
    void execTasks();  // ExecTasks: runs the threads and collects their sprites
    void resetTasks();
    int taskCount() const { return taskCount_; }

    uint8_t rand();  // Rand
    bool randCarry() const { return randCarry_; }
    void loadPal(int destination, uint16_t source);  // LoadPal: 16 colors from the program to a palette at $3000 on
    void createFadePal(int speed, int destination, uint16_t source);  // CreateFadePalTask

    void initAnim(AnimState& anim, uint16_t table) const;  // InitAnimTask
    void updateAnim(AnimState& anim) const;  // UpdateAnimData
    void drawAnim(const AnimState& anim, int x, int y);  // UpdateAnimSprites with the position of the thread

    // The end of a frame (WaitVBlank): the registers, the object memory and the palette go to the picture processor.
    void vblank();
    void render(uint8_t* rgba) const { ppu.render(rgba); }
    int sine(int index) const { return static_cast<int8_t>(data.sine[static_cast<size_t>(index & 255)]); }

    const TitleData& data;
    Sound& sound;
    Ppu ppu;
    std::array<uint16_t, 256> palette{};  // the copy of the palette at $3000
    std::vector<uint8_t> ram = std::vector<uint8_t>(0x10000, 0);  // for the tables that the HDMA reads
    // The direct page of the program
    uint16_t timer = 0;  // $15: counts down to 0, one step for each frame
    uint8_t frame = 0;  // $18: counts the frames
    uint8_t state = 0;  // $19: the state of the scene; bit 7 ends it
    std::array<uint16_t, 6> scroll{};  // $25 to $30: the scroll of bg1, bg2 and bg3, horizontal then vertical
    uint8_t hdmaMask = 0;  // $31
    uint8_t brightness = 0;  // $32
    uint8_t flags = 0;  // $33
    bool pressed = false;  // $06 bit 7: the confirm button went down

    uint16_t ramWord(int address) const { return static_cast<uint16_t>(ram[static_cast<size_t>(address)] | ram[static_cast<size_t>(address) + 1] << 8); }
    void setRamWord(int address, uint16_t value) {
        ram[static_cast<size_t>(address)] = static_cast<uint8_t>(value);
        ram[static_cast<size_t>(address) + 1] = static_cast<uint8_t>(value >> 8);
    }

    // The DMA to the video memory that every vertical blank does, until the address is 0xFFFF ($10, $12, $14, $0e).
    uint16_t dmaDestination = 0xFFFF;
    const uint8_t* dmaSource = nullptr;
    size_t dmaBytes = 0;

    // Fills the lines of the picture processor from the HDMA tables that are set; the scene supplies them.
    std::function<void(Ppu&)> hdma;

private:
    bool insertTask(int group, std::unique_ptr<CutsceneTask> task);

    std::array<std::unique_ptr<CutsceneTask>, kTaskSlots> tasks_;
    int taskCount_ = 0;
    uint8_t randomCounter_ = 0;  // $34
    bool randCarry_ = false;
    int spritesLeft_ = kMaxSprites;  // $21
};

// The thread that moves a palette to another one a little at a time (FadePalTask).
class FadePalTask : public CutsceneTask {
public:
    FadePalTask(int speed, int destination, uint16_t source) : speed_(speed), destination_(destination), source_(source) {}
    bool run(CutsceneCore& core) override;

private:
    int speed_, destination_;
    uint16_t source_;
    int counter_ = 0;  // $3900
    int threshold_ = -1;  // $3600: 0x1F at first
};

// An animated sprite that moves by itself (DefaultAnimTask): the position and the speed are in 8.8 fixed point.
class DefaultAnimTask : public CutsceneTask {
public:
    DefaultAnimTask(uint16_t table, uint16_t x, uint16_t y, uint16_t speedX, uint16_t speedY)
        : x_(x), y_(y), speedX_(speedX), speedY_(speedY), table_(table) {}
    bool run(CutsceneCore& core) override;

private:
    uint16_t x_, y_, speedX_, speedY_;
    uint16_t table_;
    AnimState anim_;
    bool started_ = false;
};

// The brightness goes down to black while the timer counts to 0 (the fade out at the end of a scene).
class FadeOutTask : public CutsceneTask {
public:
    bool run(CutsceneCore& core) override;

private:
    int level_ = -1;
};

// The palette of the lights of the town, which blinks every other frame (TownLightsThread).
class TownLightsTask : public CutsceneTask {
public:
    bool run(CutsceneCore& core) override;
};

// The addresses of the program that more than one scene uses.
constexpr uint16_t kWhitePal = 0x7445;
constexpr uint16_t kBlackPal = 0x7465;
constexpr uint16_t kTownPal1 = 0x7C43;
constexpr uint16_t kTownPal2 = 0x6498;

}  // namespace ff6
