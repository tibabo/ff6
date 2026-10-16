#include "field/text.hpp"

#include "field/map.hpp"

namespace ff6 {

void Dialogue::open(const DialogueText& text, const std::array<uint8_t, FF6_FONT_WIDTH_COUNT>& fontWidths,
                    bool atBottom) {
    codes_ = text.codes;
    length_ = text.length;
    widths_ = &fontWidths;
    atBottom_ = atBottom;
    glyphs_.clear();
    marks_.clear();
    pos_ = 0;
    x_ = FF6_TEXT_START_X;
    line_ = 0;
    pause_ = 0;
    cursor_ = 0;
    choice_ = 0;
    choiceCount_ = 0;
    lastDirection_ = FF6_NO_DIRECTION;
    active_ = codes_ != nullptr && length_ > 0;
    waiting_ = clearOnKey_ = closeOnKey_ = waitAfterPause_ = choosing_ = false;
}

void Dialogue::newLine() {
    x_ = FF6_TEXT_START_X;
    if (++line_ == FF6_TEXT_LINE_COUNT) {
        waiting_ = true;
        clearOnKey_ = true;
    }
}

uint8_t Dialogue::argument() const { return pos_ + 1 < length_ ? codes_[pos_ + 1] : 0; }

int Dialogue::wordWidth(size_t from) const {
    int width = 0;
    for (size_t i = from; i < length_; ++i) {
        const uint8_t code = codes_[i];
        if (code < FF6_FIRST_GLYPH_CODE || code == FF6_CODE_SPACE) break;
        width += (*widths_)[code];
    }
    return width;
}

void Dialogue::update(bool confirmPressed, int direction) {
    if (!active_) return;
    const int previousDirection = lastDirection_;
    lastDirection_ = direction;
    if (choosing_) {
        const int count = static_cast<int>(marks_.size());
        if (direction != previousDirection) {
            if (direction == Down) cursor_ = (cursor_ + 1) % count;
            if (direction == Up) cursor_ = (cursor_ + count - 1) % count;
        }
        if (confirmPressed) {
            choice_ = cursor_;
            choiceCount_ = count;
            choosing_ = false;
            active_ = false;
        }
        return;
    }
    if (waiting_) {
        if (!confirmPressed) return;
        waiting_ = false;
        if (closeOnKey_) {
            active_ = false;
            return;
        }
        if (clearOnKey_) {
            glyphs_.clear();
            marks_.clear();
            line_ = 0;
            x_ = FF6_TEXT_START_X;
            clearOnKey_ = false;
        }
        return;
    }
    if (pause_ > 0) {
        if (--pause_ == 0 && waitAfterPause_) {
            waitAfterPause_ = false;
            waiting_ = true;
        }
        return;
    }
    step();
}

void Dialogue::step() {
    if (pos_ >= length_) {
        waiting_ = true;
        closeOnKey_ = true;
        return;
    }
    const uint8_t code = codes_[pos_];
    switch (code) {
    case FF6_DLG_END:
        if (marks_.empty()) {
            waiting_ = true;
            closeOnKey_ = true;
        } else {
            choosing_ = true;
        }
        return;
    case FF6_DLG_NEWLINE:
        newLine();
        ++pos_;
        return;
    case FF6_DLG_WAIT_FIXED:
        pause_ = FF6_DLG_WAIT_FIXED_FRAMES;
        ++pos_;
        return;
    case FF6_DLG_WAIT:
        pause_ = argument() * FF6_DLG_WAIT_UNIT_FRAMES;
        pos_ += 2;
        return;
    case FF6_DLG_KEY:
        waiting_ = true;
        ++pos_;
        return;
    case FF6_DLG_PAGE:
        waiting_ = true;
        clearOnKey_ = true;
        ++pos_;
        return;
    case FF6_DLG_TAB:
        x_ += argument() * (*widths_)[FF6_CODE_SPACE];
        if (x_ >= FF6_TEXT_MAX_X) newLine();
        pos_ += 2;
        return;
    case FF6_DLG_CHOICE:
        marks_.push_back({x_, line_});
        x_ += (*widths_)[FF6_CODE_WIDE_SPACE];
        ++pos_;
        return;
    case FF6_DLG_KEY_TIMED:
        pause_ = argument() * FF6_DLG_WAIT_UNIT_FRAMES;
        waitAfterPause_ = pause_ > 0;
        waiting_ = pause_ == 0;
        pos_ += 2;
        return;
    default:
        break;
    }
    if (code < FF6_FIRST_GLYPH_CODE) {
        ++pos_;
        return;
    }
    if (code != FF6_CODE_SPACE && x_ > FF6_TEXT_START_X && x_ + wordWidth(pos_) >= FF6_TEXT_MAX_X) {
        newLine();
        return;
    }
    const int width = (*widths_)[code];
    if (x_ + width >= FF6_TEXT_MAX_X) {
        newLine();
        return;
    }
    glyphs_.push_back({x_, line_, code});
    x_ += width;
    ++pos_;
}

}  // namespace ff6
