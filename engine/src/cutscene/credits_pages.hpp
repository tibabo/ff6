#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ff6 {

// One word of the credits: where it is (the top left corner, in pixels of the screen), what it says, and the codes of its
// letters as the game has them (tiles of the font of the sprites), in the big font or in the small one.
struct CreditsWord {
    int x = 0, y = 0;
    std::string text;
    std::string codes;
    bool big = false;
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

}  // namespace ff6
