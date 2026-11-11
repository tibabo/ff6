#pragma once

#include <string>
#include <vector>

#include "cutscene/core.hpp"
#include "cutscene/credits_pages.hpp"
#include "cutscene/ending_book.hpp"
#include "cutscene/ending_stage.hpp"
#include "cutscene/flight.hpp"
#include "menu/menu_screen.hpp"

namespace ff6 {

// A scene of the ending that is a card for now: lines of text on black, or the pages of the credits on a dark sky,
// that fade in and out for as long as the scene of the game lasts (src/menu/ending.asm), the same time that its music
// cues need.
class EndingCard : public Cutscene {
public:
    EndingCard(MenuScreen& screen, std::vector<std::string> lines, int frames, std::vector<int> portraits = {}, std::unique_ptr<FlightBackdrop> backdrop = nullptr, std::unique_ptr<EndingBook> book = nullptr,
               std::unique_ptr<EndingStage> stage = nullptr)
        : screen_(screen), lines_(std::move(lines)), frames_(frames), portraits_(std::move(portraits)),
          book_(std::move(book)), stage_(std::move(stage)), backdrop_(std::move(backdrop)) {
        if (stage_) frames_ = kUntilStageEnds;
    }

    void tick(const InputState&) override {
        ++frame_;
        if (backdrop_) backdrop_->tick();
        if (book_) book_->tick();
        if (stage_) {
            stage_->tick();
            if (stage_->finished() && frames_ == kUntilStageEnds) frames_ = frame_ + kStageFadeOut;
        }
    }
    void draw(uint8_t* rgba) override;
    bool finished() const override { return frame_ >= frames_; }
    bool skipped() const override { return false; }

private:
    static constexpr int kUntilStageEnds = 1 << 20, kStageFadeOut = 15;

    MenuScreen& screen_;
    std::vector<std::string> lines_;
    int frames_, frame_ = 0;
    std::vector<int> portraits_;  // the characters whose portraits show above the names
    std::unique_ptr<EndingBook> book_;  // the book, which draws its own background
    std::unique_ptr<EndingStage> stage_;  // the scene of a character: the picture, the sprites and the name
    std::unique_ptr<FlightBackdrop> backdrop_;  // the ground of a flight, instead of the plain sky
};

// How long the scene that the `ending` command starts lasts, in frames (0 for a state that is not the start of one).
int endingFrames(int state);

// The scene that the `ending` command starts for a state of the ending: the names are the ones that it shows, and `frames` is
// how long a card lasts when the game has nothing better.
std::unique_ptr<EndingCard> makeEndingCard(MenuScreen& screen, const TitleData& data, int state, const std::vector<std::string>& names, int frames);

// The characters that the scene is about, by number.
std::vector<int> endingCharacters(int state);

}  // namespace ff6
