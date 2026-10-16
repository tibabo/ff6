#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "data/pack.hpp"

namespace ff6 {

struct PlacedGlyph {
    int x;
    int line;
    uint8_t code;
};

struct ChoiceMark {
    int x;
    int line;
};

// Port of the English dialogue typewriter (UpdateDlgText): one step per frame. Text with choice marks ends with
// a cursor that the player moves with up and down, instead of waiting for a key.
class Dialogue {
public:
    // The text must stay valid while the dialogue is open.
    void open(const DialogueText& text, const std::array<uint8_t, FF6_FONT_WIDTH_COUNT>& fontWidths, bool atBottom);
    // direction: the direction held this frame, or FF6_NO_DIRECTION.
    void update(bool confirmPressed, int direction);
    bool active() const { return active_; }
    bool waitingForKey() const { return waiting_; }
    bool choosing() const { return choosing_; }
    int cursor() const { return cursor_; }
    bool atBottom() const { return atBottom_; }
    // The choice made in the last dialogue and how many there were (0 when it had none).
    int choice() const { return choice_; }
    int choiceCount() const { return choiceCount_; }
    const std::vector<PlacedGlyph>& glyphs() const { return glyphs_; }
    const std::vector<ChoiceMark>& choiceMarks() const { return marks_; }

private:
    void newLine();
    void step();
    int wordWidth(size_t from) const;
    uint8_t argument() const;

    const uint8_t* codes_ = nullptr;
    size_t length_ = 0;
    const std::array<uint8_t, FF6_FONT_WIDTH_COUNT>* widths_ = nullptr;
    std::vector<PlacedGlyph> glyphs_;
    std::vector<ChoiceMark> marks_;
    size_t pos_ = 0;
    int x_ = FF6_TEXT_START_X, line_ = 0, pause_ = 0;
    int cursor_ = 0, choice_ = 0, choiceCount_ = 0, lastDirection_ = FF6_NO_DIRECTION;
    bool active_ = false, waiting_ = false, clearOnKey_ = false, closeOnKey_ = false;
    bool waitAfterPause_ = false, choosing_ = false, atBottom_ = false;
};

}  // namespace ff6
