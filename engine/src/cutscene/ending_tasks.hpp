#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "cutscene/ppu.hpp"
#include "data/title_data.hpp"

namespace ff6 {

// The threads of the cutscene of the ending (the task list of the menu code: ExecTasks, CreateTask, InitAnimTask,
// UpdateAnimTask...): 64 slots in groups of eight by priority, run in order each frame, and the sprites of the tasks that
// animate go into the object memory in that order, so the first slot is in front.
class EndingTasks {
public:
    static constexpr int kSlots = 64, kSlotsPerPriority = 8, kMaxSprites = 128;
    // Where the animations are: the banks of the game that the pack holds, with the address that the first byte had.
    enum Bank { kBankEnding1 = 0xC2, kBankEnding2 = 0xCF, kBankCoin = 0xD8, kBankText = 0x7E };  // the last is the memory where the words of the credits are made
    struct Task;
    using Code = bool (EndingTasks::*)(Task&);  // false ends the task (a clear carry in the game)

    struct Task {
        Code code = nullptr;  // null is a free slot
        int state = 0;
        int counter = 0;  // $3349, a word
        int frames = 0;  // $35c9
        int saved = 0;  // $37c9, a word (an address of animation)
        int flags = 0, pal = 0;  // bit 0 of the flags: the sprites go with the scroll of the first background, bit 1: mirrored
        int posX = 0, posY = 0, speedX = 0, speedY = 0;  // words, whose high byte is the position on the screen
        int bank = kBankEnding1, ptr = 0, offset = 0, animCounter = 0;  // the animation that the task plays, and where it is
        int fadeFirst = 0, fadeSpeed = 0;  // for the fade of a palette: the first color to change, and the frames per step
        int param = 0, remaining = 0;  // what some tasks keep in the globals of the game: a position, and a count
        std::array<uint16_t, 16> fadeTarget{};
    };

    EndingTasks(const TitleData& data, std::array<uint16_t, 256>& cgram) : data_(data), cgram_(cgram) {}

    // ExecTasks: one frame of every task, then the sprites go to `sprites`.
    void exec(std::vector<PpuSprite>& sprites);
    // CreateTask: the first free slot from the first one of the priority. The task is ready to be filled in.
    Task& create(int priority, Code code);
    int active() const;
    int16_t sine(int index) const;  // SineTbl16

    // What the animations ask of the screen and of the picture.
    int bg1HScroll = 0, bg3VScroll = 0, frameCounter = 0, bg3Target = 0;
    // The tiles of a page of the book, over the picture: where they are in the graphics, where they go in the map, the
    // width in bytes and the rows.
    std::function<void(int, int, int, int)> copyTiles;

    // The shared helpers of the tasks.
    void setAnimation(Task& task, int bank, int address) const {
        task.bank = bank;
        task.ptr = address;
    }
    void initAnimation(Task& task) const;
    uint8_t byte(int bank, int address) const;
    uint16_t word(int bank, int address) const { return static_cast<uint16_t>(byte(bank, address) | byte(bank, address + 1) << 8); }
    std::array<uint16_t, 16> palette(int address) const;  // sixteen colors of the data of the animations
    // CreateFadePalTask: the colors, from the first one of CGRAM, go to the target by a step every `speed` frames.
    void fadePalette(int firstColor, const std::array<uint16_t, 16>& target, int speed);
    void blackPalette(int firstColor, int speed) { fadePalette(firstColor, std::array<uint16_t, 16>{}, speed); }

    // The codes of the tasks.
    bool animTask(Task& task);  // EndingAnimTask
    bool animCounterTask(Task& task);  // the generic one with a counter ($3349): ends when it is zero
    bool fadeTask(Task& task);  // FadePalTask
    bool bg1ScrollTask(Task& task);  // _c3e002: a pixel every four frames for `counter` frames
    bool bg3ScrollTask(Task& task);  // _c3d1b6: the text goes down a pixel every four frames
    bool coinTask(Task& task);  // CoinAnimTask
    bool relmBrushTask(Task& task);  // RelmBrushAnimTask
    bool cyanSwordTask(Task& task);  // CyanSwordAnimTask
    bool sparkleTask(Task& task);  // _c3ed14
    bool gogoGlimmerTask(Task& task);  // _c3e49f
    bool miniMoogleTask(Task& task);  // _c3eafc
    bool bookTask(Task& task);  // _c3ee04: the pages of the book that turn
    // The tasks of the flights of the credits.
    bool oscillatingBirdTask(Task& task);  // _c3cb68: a bird that sways to the sides
    bool splashAirshipTask(Task& task);  // _c3cbc6: the airship over the sea, that bobs and leaves a splash
    bool airshipTask(Task& task);  // _c3cff8: an airship, or a propeller, that bobs
    bool boatTask(Task& task);  // _c3c9a2: the half of the boat that starts late
    bool flyingBirdTask(Task& task);  // _c3cdea: a bird that crosses the screen by the steps of a table
    void createSplash();  // _c3cc38
    // The words of the credits: the letters of a word are the sprites of an animation that the game builds in its memory
    // (CreateCreditsTextTask); the palettes of the font (4 and 5 of the sprites) fade in with the first word of a page and out
    // with the first one that has 128 frames left. `normal` is the palette of the clouds and of the tiny airship, the other
    // one is its inverse.
    void createCreditsWord(const std::string& codes, bool big, int x, int y);
    bool creditsWordTask(Task& task);  // _c3dc7c
    bool creditsNormal = false;
    // The full name of a character, in two tasks of big letters, and the "as" that comes before it (CreateBigCharNameTask,
    // CreateEndingCharAsTask). The tasks wait `bigNameWait` frames ($85) and end when `bigNameStop` ($47) is set.
    void createBigName(int character);
    void createAs();
    bool bigTextTask(Task& task);  // EndingBigTextTask
    int bigNameWait = 300;
    bool bigNameStop = false;

    void updateEndingAnim(Task& task) {  // UpdateEndingAnimTask
        task.posX = (task.posX + task.speedX) & 0xFFFF;
        task.posY = (task.posY + task.speedY) & 0xFFFF;
        updateAnimTask(task);
    }
    void updateAnimTask(Task& task) {  // UpdateAnimTask
        updateAnimData(task);
        updateAnimSprites(task);
    }
    void updateAnimData(Task& task) const;
    void updateAnimSprites(const Task& task);

private:
    const TitleData& data_;
    std::array<uint16_t, 256>& cgram_;
    std::array<Task, kSlots> slots_{};
    std::vector<PpuSprite>* out_ = nullptr;
    int current_ = 0;  // the slot that runs
    std::vector<uint8_t> text_;  // the animations of the words, from $1000
    bool creditsShown_ = false;  // $af: the palettes of the font are in, or on the way
};

}  // namespace ff6
