#include <array>
#include <string>
#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/text.hpp"

namespace {

std::array<uint8_t, FF6_FONT_WIDTH_COUNT> makeWidths() {
    std::array<uint8_t, FF6_FONT_WIDTH_COUNT> widths;
    widths.fill(8);
    widths[FF6_CODE_SPACE] = 4;
    widths[FF6_CODE_WIDE_SPACE] = 6;
    return widths;
}

std::string text(const ff6::Dialogue& dialogue, int line) {
    std::string out;
    for (const ff6::PlacedGlyph& glyph : dialogue.glyphs()) {
        if (glyph.line == line) out.push_back(glyph.code == FF6_CODE_SPACE ? ' ' : static_cast<char>('A' + glyph.code - FF6_FIRST_GLYPH_CODE));
    }
    return out;
}

void run(ff6::Dialogue& dialogue, int frames) {
    for (int i = 0; i < frames && !dialogue.waitingForKey() && !dialogue.choosing(); ++i) {
        dialogue.update(false, FF6_NO_DIRECTION);
    }
}

}  // namespace

static void testTextChoices();

void testText() {
    const auto widths = makeWidths();
    // 25 letters and a space reach x=208, so the 48-pixel second word wraps.
    std::vector<uint8_t> codes(25, 0x20);
    codes.push_back(FF6_CODE_SPACE);
    for (int i = 0; i < 6; ++i) codes.push_back(0x21);
    codes.insert(codes.end(), {FF6_DLG_NEWLINE, 0x22, FF6_DLG_NEWLINE, 0x23, FF6_DLG_NEWLINE, 0x24, FF6_DLG_END});

    ff6::Dialogue dialogue;
    dialogue.open({codes.data(), codes.size()}, widths, false);
    CHECK(dialogue.active() && !dialogue.atBottom());
    run(dialogue, 200);
    CHECK(text(dialogue, 0) == std::string(25, 'A') + " ");
    CHECK(text(dialogue, 1) == "BBBBBB");
    CHECK(text(dialogue, 2) == "C" && text(dialogue, 3) == "D");
    CHECK(dialogue.waitingForKey());
    dialogue.update(true, FF6_NO_DIRECTION);
    CHECK(dialogue.glyphs().empty());
    run(dialogue, 50);
    CHECK(text(dialogue, 0) == "E");
    CHECK(dialogue.waitingForKey() && dialogue.active());
    dialogue.update(true, FF6_NO_DIRECTION);
    CHECK(!dialogue.active());

    std::vector<uint8_t> waits = {0x20, FF6_DLG_WAIT, 2, 0x21, FF6_DLG_END};
    dialogue.open({waits.data(), waits.size()}, widths, true);
    CHECK(dialogue.atBottom());
    dialogue.update(false, FF6_NO_DIRECTION);
    dialogue.update(false, FF6_NO_DIRECTION);
    for (int i = 0; i < 29; ++i) dialogue.update(false, FF6_NO_DIRECTION);
    CHECK(text(dialogue, 0) == "A");
    dialogue.update(false, FF6_NO_DIRECTION);
    dialogue.update(false, FF6_NO_DIRECTION);
    CHECK(text(dialogue, 0) == "AB");

    std::vector<uint8_t> page = {0x20, FF6_DLG_PAGE, 0x21, FF6_DLG_END};
    dialogue.open({page.data(), page.size()}, widths, false);
    run(dialogue, 10);
    CHECK(text(dialogue, 0) == "A" && dialogue.waitingForKey());
    dialogue.update(true, FF6_NO_DIRECTION);
    run(dialogue, 10);
    CHECK(text(dialogue, 0) == "B");

    // A dialogue that does not exist or is empty never opens.
    dialogue.open({}, widths, false);
    CHECK(!dialogue.active());

    testTextChoices();
}

static void testTextChoices() {
    const auto widths = makeWidths();
    // "<choice>A" on the first line and "<choice>B" on the second: the cursor picks one of them.
    std::vector<uint8_t> codes = {FF6_DLG_CHOICE, 0x20, FF6_DLG_NEWLINE, FF6_DLG_CHOICE, 0x21, FF6_DLG_END};
    ff6::Dialogue dialogue;
    dialogue.open({codes.data(), codes.size()}, widths, false);
    run(dialogue, 100);
    CHECK(dialogue.choosing() && !dialogue.waitingForKey());
    CHECK(dialogue.choiceMarks().size() == 2);
    CHECK(dialogue.choiceMarks()[0].x == FF6_TEXT_START_X && dialogue.choiceMarks()[0].line == 0);
    CHECK(dialogue.choiceMarks()[1].line == 1);
    CHECK(dialogue.cursor() == 0);
    dialogue.update(false, ff6::Down);  // a held direction moves the cursor only once
    dialogue.update(false, ff6::Down);
    CHECK(dialogue.cursor() == 1);
    dialogue.update(false, FF6_NO_DIRECTION);
    dialogue.update(false, ff6::Down);  // wraps around to the first choice
    CHECK(dialogue.cursor() == 0);
    dialogue.update(false, FF6_NO_DIRECTION);
    dialogue.update(false, ff6::Up);  // and back to the last
    CHECK(dialogue.cursor() == 1);
    dialogue.update(true, FF6_NO_DIRECTION);
    CHECK(!dialogue.active() && dialogue.choice() == 1 && dialogue.choiceCount() == 2);

    // Text without choices reports none.
    std::vector<uint8_t> plain = {0x20, FF6_DLG_END};
    dialogue.open({plain.data(), plain.size()}, widths, false);
    run(dialogue, 20);
    dialogue.update(true, FF6_NO_DIRECTION);
    CHECK(dialogue.choiceCount() == 0);
}
