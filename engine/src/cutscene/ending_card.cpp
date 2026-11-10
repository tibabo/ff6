#include "cutscene/ending_card.hpp"

#include <algorithm>
#include <map>

namespace ff6 {
namespace {

constexpr int kFadeIn = 30, kFadeOut = 15;
constexpr int kCreditsBase = 0x9894, kCreditsEnd = 0x9C44;  // where the data starts in its bank, and where the pages stop
constexpr int kWordFrames = 420, kWordRise = 64, kWordFadeIn = 32, kWordFadeOut = 128;

struct PageAt {
    int scene, page, address;
};
// The pages one after the other: a page ends where the next one starts.
constexpr PageAt kPages[] = {
    {1, 1, 0x9894}, {2, 1, 0x98A0}, {2, 2, 0x98B4}, {2, 3, 0x98CC}, {2, 4, 0x98E4}, {2, 5, 0x98FC}, {2, 6, 0x9908}, {2, 7, 0x9918},
    {3, 1, 0x9930}, {3, 2, 0x9950}, {3, 3, 0x9968}, {3, 4, 0x9980}, {4, 1, 0x9990}, {4, 2, 0x99A0}, {4, 3, 0x99B4}, {4, 4, 0x99D8},
    {4, 5, 0x99FC}, {4, 6, 0x9A20}, {4, 7, 0x9A34}, {5, 1, 0x9A48}, {5, 2, 0x9A58}, {5, 3, 0x9A70}, {5, 4, 0x9A88}, {5, 5, 0x9AAC},
    {5, 6, 0x9AC4}, {5, 7, 0x9ADC}, {6, 1, 0x9AF4}, {6, 2, 0x9B1C}, {6, 3, 0x9B44}, {6, 4, 0x9B68}, {6, 5, 0x9B88}, {6, 6, 0x9BA0},
    {6, 7, 0x9BB4}, {7, 1, 0x9BD8}, {7, 2, 0x9BF0}, {7, 3, 0x9C08}, {7, 4, 0x9C1C}, {7, 5, 0x9C30}};
// The tables of words of a scene: the address and size in bytes of the small ones, then of the big ones.
struct WordTables {
    int small, smallBytes, big, bigBytes;
};
constexpr WordTables kTables[7] = {{0x9DC0, 0x04, 0x9C44, 0x08}, {0x9DC4, 0x28, 0x9C4C, 0x60}, {0x9DEC, 0x18, 0x9CAC, 0x40}, {0x9E04, 0x20, 0x9CEC, 0x64},
                                   {0x9E24, 0x24, 0x9D50, 0x70}, {0x9E48, 0xE4, 0, 0},         {0x9F2C, 0x6C, 0, 0}};

// A letter of the credits font: the big capitals are 16x16 tiles of a sheet, the small letters take one tile each.
char creditsLetter(uint8_t code) {
    if (code >= 0xE0) return code <= 0xF9 ? static_cast<char>('a' + code - 0xE0) : code == 0xFA ? '.' : ' ';
    return static_cast<char>('A' + (code - 0x40) / 0x20 * 8 + (code - 0x40) % 0x20 / 2);
}

// The frame of the scene at which a page comes up (CreditsTextTaskScene1 to 7).
int pageStart(int scene, int page) {
    switch (scene) {
        case 1: return 60;
        case 6: return 240 + 480 * (page - 1);
        case 7: {
            static constexpr int kStarts[] = {60, 480, 900, 1320, 1740};
            return kStarts[page - 1];
        }
        default: return 10 + 420 * (page - 1);
    }
}

struct Scene {
    int state, frames;
    std::vector<int> characters;
};

// The time of each scene is the sum of the waits of its states in the ending code, and a fade out. The numbers of the
// characters are the ones of the game.
const std::vector<Scene>& scenes() {
    static const std::vector<Scene> table = {
        {2, 240, {}},   {4, 120, {}},   {5, 540, {}},    {9, 3240, {}},  {12, 1800, {}}, {16, 3000, {}}, {20, 3000, {}},
        {24, 3600, {}}, {28, 2220, {}}, {32, 270, {}},   {40, 960, {3}}, {45, 1800, {}}, {50, 960, {2}},  {60, 960, {10}},
        {64, 1320, {4, 5}}, {70, 960, {12}}, {74, 1020, {}}, {80, 960, {11}}, {90, 960, {0}}, {100, 1320, {1, 6}},
        {110, 960, {8}}, {120, 960, {7}}, {130, 1080, {9}}, {135, 960, {13}}};
    return table;
}

}  // namespace

std::vector<CreditsPage> creditsPages(int scene, const std::vector<uint8_t>& data) {
    std::vector<CreditsPage> pages;
    if (scene < 1 || scene > 7 || data.size() < FF6_TITLE_CREDITS_BYTES) return pages;
    const auto byteAt = [&](int address) { return address >= kCreditsBase && address - kCreditsBase < static_cast<int>(data.size()) ? data[static_cast<size_t>(address - kCreditsBase)] : 0; };
    const auto wordAt = [&](int address) { return byteAt(address) | byteAt(address + 1) << 8; };
    // Each word is where its table puts it in the memory of the game, which the pages ask for.
    std::map<int, std::string> words;
    const WordTables& tables = kTables[scene - 1];
    for (const auto& [address, bytes] : {std::pair{tables.small, tables.smallBytes}, std::pair{tables.big, tables.bigBytes}}) {
        for (int i = 0; i < bytes; i += 4) {
            std::string text;
            for (int at = wordAt(address + i); byteAt(at) != 0; ++at) text += creditsLetter(byteAt(at));
            words[wordAt(address + i + 2)] = text;
        }
    }
    for (size_t i = 0; i < sizeof(kPages) / sizeof(kPages[0]); ++i) {
        if (kPages[i].scene != scene) continue;
        const int end = i + 1 < sizeof(kPages) / sizeof(kPages[0]) ? kPages[i + 1].address : kCreditsEnd;
        CreditsPage page;
        page.start = pageStart(scene, kPages[i].page);
        for (int at = kPages[i].address; at < end; at += 4) {
            const auto found = words.find(wordAt(at));
            if (found != words.end()) page.words.push_back({byteAt(at + 2), byteAt(at + 3), found->second});
        }
        pages.push_back(std::move(page));
    }
    return pages;
}

int creditsScene(int state) {
    switch (state) {
        case 5: return 1;
        case 9: return 2;
        case 12: return 3;
        case 16: return 4;
        case 20: return 5;
        case 24: return 6;
        case 28: return 7;
        default: return 0;
    }
}

int endingFrames(int state) {
    for (const Scene& scene : scenes()) {
        if (scene.state == state) return scene.frames + kFadeOut;
    }
    return 0;
}

std::vector<int> endingCharacters(int state) {
    for (const Scene& scene : scenes()) {
        if (scene.state == state) return scene.characters;
    }
    return {};
}

void EndingCard::draw(uint8_t* rgba) {
    const int level = std::max(0, std::min({frame_ * 16 / kFadeIn, (frames_ - frame_) * 16 / kFadeOut, 16}));
    if (!pages_.empty() || backdrop_) {  // the credits on the ground of the flight, or on a sky that gets lighter toward the horizon
        if (backdrop_) {
            backdrop_->draw(rgba);
            for (size_t i = 0; i < static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL; ++i) {
                if (i % FF6_BYTES_PER_PIXEL != FF6_ALPHA_OFFSET) rgba[i] = static_cast<uint8_t>(rgba[i] * level / 16);
            }
        }
        for (int y = 0; y < FF6_SCREEN_HEIGHT && !backdrop_; ++y) {
            for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
                uint8_t* pixel = rgba + (static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL;
                pixel[0] = static_cast<uint8_t>((8 + y / 8) * level / 16);
                pixel[1] = static_cast<uint8_t>((16 + y / 4) * level / 16);
                pixel[2] = static_cast<uint8_t>((48 + y / 2) * level / 16);
                pixel[FF6_ALPHA_OFFSET] = 255;
            }
        }
        for (const CreditsPage& page : pages_) {
            const int age = frame_ - page.start;
            if (age < 0 || age >= kWordFrames) continue;
            screen_.clear();
            const int rise = std::max(0, (kWordRise - age) / 2);
            for (const CreditsWord& word : page.words) {
                screen_.setTextOffset(word.y % FF6_MENU_TILE_SIZE + rise);
                screen_.text((word.x + FF6_MENU_TILE_SIZE / 2) / FF6_MENU_TILE_SIZE, word.y / FF6_MENU_TILE_SIZE, word.text, MenuScreen::Layer::Bg3, FF6_MENU_BG3_DEFAULT);
            }
            const int shown = std::min({age * 16 / kWordFadeIn, (kWordFrames - age) * 16 / kWordFadeOut, 16}) * level / 16;
            const std::vector<uint8_t>& text = screen_.pixels();
            for (size_t i = 0; i + FF6_BYTES_PER_PIXEL <= text.size(); i += FF6_BYTES_PER_PIXEL) {
                if (text[i] == 0 && text[i + 1] == 0 && text[i + 2] == 0) continue;
                if (backdrop_ && backdrop_->light()) {
                    static constexpr int kGold[3] = {240, 184, 40};
                    const int strength = std::max({text[i], text[i + 1], text[i + 2]});
                    for (int c = 0; c < FF6_ALPHA_OFFSET; ++c) rgba[i + static_cast<size_t>(c)] = static_cast<uint8_t>(kGold[c] * strength / 255 * shown / 16);
                    continue;
                }
                for (int c = 0; c < FF6_ALPHA_OFFSET; ++c) rgba[i + static_cast<size_t>(c)] = static_cast<uint8_t>(text[i + static_cast<size_t>(c)] * shown / 16);
            }
        }
        screen_.setTextOffset(0);
        return;
    }
    screen_.clear();
    screen_.setTextOffset(0);
    for (size_t i = 0; i < portraits_.size(); ++i) {
        const int width = FF6_MENU_PORTRAIT_SIZE + 16, left = (FF6_SCREEN_WIDTH - static_cast<int>(portraits_.size()) * width + 16) / 2;
        screen_.portrait(left + static_cast<int>(i) * width, FF6_SCREEN_HEIGHT / 2 - 56, menuPortrait(portraits_[i]));
    }
    int row = FF6_SCREEN_HEIGHT / FF6_MENU_TILE_SIZE / 2 - static_cast<int>(lines_.size()) / 2;
    for (const std::string& line : lines_) {
        screen_.text((FF6_SCREEN_WIDTH / FF6_MENU_TILE_SIZE - static_cast<int>(line.size())) / 2, row++, line, MenuScreen::Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    }
    for (size_t i = 0; i < screen_.pixels().size(); ++i) {
        rgba[i] = (i % FF6_BYTES_PER_PIXEL) == FF6_ALPHA_OFFSET ? screen_.pixels()[i] : static_cast<uint8_t>(screen_.pixels()[i] * level / 16);
    }
    if (names_) {
        std::vector<uint8_t> letters(screen_.pixels().size());
        names_->draw(letters.data());
        for (size_t i = 0; i + FF6_BYTES_PER_PIXEL <= letters.size(); i += FF6_BYTES_PER_PIXEL) {
            if (letters[i] == 0 && letters[i + 1] == 0 && letters[i + 2] == 0) continue;
            for (int c = 0; c < FF6_ALPHA_OFFSET; ++c) rgba[i + static_cast<size_t>(c)] = static_cast<uint8_t>(letters[i + static_cast<size_t>(c)] * level / 16);
        }
    }
}

}  // namespace ff6
