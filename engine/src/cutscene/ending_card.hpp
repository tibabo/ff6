#pragma once

#include <string>
#include <vector>

#include "cutscene/core.hpp"
#include "cutscene/ending_names.hpp"
#include "cutscene/flight.hpp"
#include "menu/menu_screen.hpp"

namespace ff6 {

// One word of the credits: where it is (the top left corner, in pixels of the screen) and what it says.
struct CreditsWord {
    int x = 0, y = 0;
    std::string text;
};

// The words that come up together, at the frame of the scene that the game gives (CreditsTextTask).
struct CreditsPage {
    int start = 0;
    std::vector<CreditsWord> words;
};

// The pages of the credits of the scene 1 to 7 of the flight, from the data that the ROM holds (C2/9894).
std::vector<CreditsPage> creditsPages(int scene, const std::vector<uint8_t>& data);

// The scene of the credits that a scene of the ending starts, 0 for the others.
int creditsScene(int state);

// A scene of the ending that is a card for now: lines of text on black, or the pages of the credits on a dark sky,
// that fade in and out for as long as the scene of the game lasts (src/menu/ending.asm), the same time that its music
// cues need.
class EndingCard : public Cutscene {
public:
    EndingCard(MenuScreen& screen, std::vector<std::string> lines, int frames, std::vector<CreditsPage> pages = {},
               std::vector<int> portraits = {}, std::unique_ptr<FlightBackdrop> backdrop = nullptr, std::unique_ptr<EndingNames> names = nullptr)
        : screen_(screen), lines_(std::move(lines)), frames_(frames), pages_(std::move(pages)), portraits_(std::move(portraits)),
          backdrop_(std::move(backdrop)), names_(std::move(names)) {}

    void tick(const InputState&) override {
        ++frame_;
        if (backdrop_) backdrop_->tick();
        if (names_) names_->tick();
    }
    void draw(uint8_t* rgba) override;
    bool finished() const override { return frame_ >= frames_; }
    bool skipped() const override { return false; }

private:
    MenuScreen& screen_;
    std::vector<std::string> lines_;
    int frames_, frame_ = 0;
    std::vector<CreditsPage> pages_;
    std::vector<int> portraits_;  // the characters whose portraits show above the names
    std::unique_ptr<EndingNames> names_;  // the big letters of the name, over the card
    std::unique_ptr<FlightBackdrop> backdrop_;  // the ground of a flight, instead of the plain sky
};

// How long the scene that the `ending` command starts lasts, in frames (0 for a state that is not the start of one).
int endingFrames(int state);

// The characters that the scene is about, by number.
std::vector<int> endingCharacters(int state);

}  // namespace ff6
