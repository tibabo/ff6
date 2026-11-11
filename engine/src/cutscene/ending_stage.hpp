#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "cutscene/ending_tasks.hpp"
#include "cutscene/ppu.hpp"
#include "data/title_data.hpp"
#include "menu/menu_screen.hpp"

namespace ff6 {

// A scene of a character of the ending (src/menu/ending.asm, from the states 40 to 138): a picture on the background (the
// table, and what the character has on it), the sprites that move over it (the apple, the coin, the cards...), the name in
// large letters at the bottom, and the palettes that come and go. The states and their waits are the ones of the game, and
// the tasks run in the same order, so the picture is the same.
class EndingStage {
public:
    // `state` is the first of the scene (40 for Shadow, 50 Cyan, 60 Mog, 64 Edgar and Sabin, 70 Gogo, 80 Gau, 90 Terra,
    // 100 Locke and Celes, 110 Relm, 120 Strago, 130 Setzer, 135 Umaro); `names` the names that the scene shows, one or two.
    EndingStage(const TitleData& data, MenuScreen& screen, int state, std::vector<std::string> names);

    void tick();
    void draw(uint8_t* rgba);
    bool finished() const { return state_ == kTerminate; }
    int frame() const { return frame_; }
    const Ppu& ppu() const { return ppu_; }

    static bool handles(int state);

private:
    static constexpr int kTerminate = 0xFF, kWaitFade = 1, kFadeCredits = 56;

    void step();
    void commonStates();
    void startScene();
    void copyRect(int source, int dest, int widthBytes, int rows);
    void loadSprites(const std::vector<uint8_t>& gfx, size_t bytes, size_t vramWord, bool twoBits = false);
    // The helpers that the states call (the routines of the game with the same addresses in their names).
    void fadeInBackground();  // _c3e192
    void fadeOutBackground();  // _c3e145
    void fadeInText();  // _c3e1df
    void scrollText();  // _c3e206
    void fadeOutSprites0();  // _c3e255
    void fadeInSprites4();  // _c3e28a
    void fadeOutSprites4();  // _c3e29e
    void scrollBackground(int frames);  // _c3e839, _c3e83f, _c3e845
    void setWait(int frames) { wait_ = frames; }
    void drawText(uint8_t* rgba);

    const TitleData& data_;
    MenuScreen& screen_;
    Ppu ppu_;
    EndingTasks tasks_;
    std::array<uint16_t, 0x800> map_{};  // the two screens of the first background
    int scene_ = 0, state_ = 0, first_ = 0, wait_ = 0, frame_ = 0;
    std::vector<std::string> names_;
    size_t nameIndex_ = 0;  // the name that is on the background of the text
};

}  // namespace ff6
