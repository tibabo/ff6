#include "cutscene/ending_stage.hpp"

#include <algorithm>

namespace ff6 {
namespace {

// The palettes of the first block of animations (C2), by their addresses.
constexpr int kBookPal = 0x967C, kTablePal = 0x969C, kLightPal = 0x96DC, kGlowPal = 0x96FC;
constexpr int kTextPal = 0x9754, kSpritePal = 0x9774, kSparklePal = 0x955C;
constexpr int kBg0 = 0, kBg1 = 16, kBg2 = 32, kBg5 = 80, kBg6 = 96;
constexpr int kSprite0 = 128, kSprite1 = 144, kSprite2 = 160, kSprite4 = 192;

// The animations that the scenes start: the address in the bank of the block that holds them.
constexpr int kShadowAppleAnim = 0x9497, kSetzerCardAnim = 0x92FA, kBookAnimStrago = 0x8B72;
constexpr int kGogoGlimmerAnim = 0xF450, kGauEyesAnim = 0xF647, kMogMiniMoogleAnim = 0xF515, kUmaroSkullAnim = 0xF56E, kTerraPendantAnim = 0xF5B4;
constexpr int kUmaroMiniMoogleAnim4 = 0xF47F;

// The big names are drawn two rows of tiles tall, at the bottom of the screen.
constexpr int kNameRow = 22;

// The pictures of the scenes of the characters: where they are in the graphics, the size of the rectangle and where it goes.
struct Picture {
    int source, width, rows, destination;
};
constexpr Picture kCyanPicture = {0x03E0, 0x26, 2, 0x3BA1}, kMogPicture = {0x0020, 0x0E, 8, 0x3A77}, kGogoPicture = {0x002E, 0x1C, 9, 0x3A2D},
                  kGauPicture = {0x004A, 0x0C, 7, 0x3AB7}, kTerraPicture = {0x0220, 0x0E, 5, 0x3B35}, kLockePicture = {0x0274, 0x18, 6, 0x3B69},
                  kRelmPicture = {0x0360, 0x14, 2, 0x3BF3}, kBookPicture = {0x0BC8, 0x1C, 6, 0x3AD9}, kUmaroPicture = {0x0056, 0x0A, 5, 0x3B37};
constexpr int kMapBase = 0x3849;  // where the first screen of the first background is in the memory of the game (wBG1Tiles::ScreenA)

// The characters (by their numbers in the game) that each scene is about: the first one, and the second of the scenes of two.
struct Characters {
    int first, second;
};
constexpr Characters kCharacters[] = {{3, -1}, {2, -1}, {4, 5}, {10, -1}, {12, -1}, {11, -1}, {0, -1}, {1, 6}, {8, -1}, {7, -1}, {9, -1}, {13, -1}};

enum Scene { kShadow, kCyan, kEdgarSabin, kMog, kGogo, kGau, kTerra, kLockeCeles, kRelm, kStrago, kSetzer, kUmaro, kScenes };

int sceneOf(int state) {
    switch (state) {
        case 40: return kShadow;
        case 50: return kCyan;
        case 60: return kMog;
        case 64: return kEdgarSabin;
        case 70: return kGogo;
        case 80: return kGau;
        case 90: return kTerra;
        case 100: return kLockeCeles;
        case 110: return kRelm;
        case 120: return kStrago;
        case 130: return kSetzer;
        case 135: return kUmaro;
        default: return -1;
    }
}

}  // namespace

bool EndingStage::handles(int state) { return sceneOf(state) >= 0; }

EndingStage::EndingStage(const TitleData& data, MenuScreen& screen, int state, std::vector<std::string> names)
    : data_(data), screen_(screen), tasks_(data, ppu_.cgram), scene_(sceneOf(state)), state_(state), first_(state), names_(std::move(names)) {
    ppu_.objsel = 0x03;
    ppu_.bgsc = {0x03, 0x11, 0x19};
    ppu_.nba12 = 0x33;
    ppu_.nba34 = 0x22;
    ppu_.tm = 0x17;
    ppu_.ts = 0x02;
    ppu_.brightness = 15;
    ppu_.forceBlank = false;
    // The characters of the background in three pieces (LoadEndingBGGfx), its second map, and the font of the big letters.
    const auto transfer = [&](size_t from, size_t bytes, size_t to) {
        for (size_t i = 0; i + 1 < bytes && from + i + 1 < data.endingGfx1.size(); i += 2) {
            ppu_.vram[to + i / 2] = static_cast<uint16_t>(data.endingGfx1[from + i] | data.endingGfx1[from + i + 1] << 8);
        }
    };
    transfer(0x0000, 0x1F60, 0x3000);
    transfer(0x1F60, 0x0B40, 0x4000);
    transfer(0x2AA0, 0x0E00, 0x5000);
    transfer(0x4448, 0x780, 0x1000);
    for (size_t tile = 0; tile * 16 + 16 <= data.endingFont.size(); ++tile) {
        for (size_t row = 0; row < 8; ++row) {
            const size_t at = tile * 16 + row * 2;
            ppu_.vram[0x7000 + tile * 16 + row] = static_cast<uint16_t>(data.endingFont[at] | data.endingFont[at + 1] << 8);
        }
    }
    // The first background: its map on both screens.
    for (int screenNumber = 0; screenNumber < 2; ++screenNumber) {
        for (int i = 0; i < 0x780 / 2; ++i) {
            const size_t at = static_cast<size_t>(0x38A0 + i * 2);
            map_[static_cast<size_t>(screenNumber * 0x400 + i)] = static_cast<uint16_t>(data.endingGfx1[at] | data.endingGfx1[at + 1] << 8);
        }
    }
    tasks_.copyTiles = [this](int source, int destination, int widthBytes, int rows) { copyRect(source, destination, widthBytes, rows); };
}

// _c3edcd: the rows of tiles of the graphics, from `source` (a row is 0x40 bytes), over the map of the first screen.
void EndingStage::copyRect(int source, int destination, int widthBytes, int rows) {
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < widthBytes / 2; ++column) {
            const size_t from = static_cast<size_t>(0x4000 + source + row * 0x40 + column * 2);
            const int to = (destination - kMapBase) / 2 + row * 32 + column;
            if (from + 1 >= data_.endingGfx1.size() || to < 0 || to >= static_cast<int>(map_.size())) continue;
            map_[static_cast<size_t>(to)] = static_cast<uint16_t>(data_.endingGfx1[from] | data_.endingGfx1[from + 1] << 8);
        }
    }
}

// The graphics of the sprites at the start of the characters of the sprites (the second part for the skull is at 0x6800).
void EndingStage::loadSprites(const std::vector<uint8_t>& gfx, size_t bytes, size_t vramWord, bool twoBits) {
    if (!twoBits) {
        for (size_t i = 0; i + 1 < bytes && i + 1 < gfx.size(); i += 2) ppu_.vram[vramWord + i / 2] = static_cast<uint16_t>(gfx[i] | gfx[i + 1] << 8);
        return;
    }
    for (size_t tile = 0; tile * 16 + 16 <= std::min(bytes, gfx.size()); ++tile) {  // TfrGfx2bpp: the high planes are clear
        for (size_t row = 0; row < 8; ++row) {
            const size_t at = tile * 16 + row * 2;
            ppu_.vram[vramWord + tile * 16 + row] = static_cast<uint16_t>(gfx[at] | gfx[at + 1] << 8);
            ppu_.vram[vramWord + tile * 16 + 8 + row] = 0;
        }
    }
}

void EndingStage::fadeInBackground() {
    tasks_.fadePalette(kBg1, tasks_.palette(kBookPal), 2);
    tasks_.fadePalette(kBg2, tasks_.palette(kTablePal), 2);
    tasks_.fadePalette(kBg5, tasks_.palette(kLightPal), 2);
    tasks_.fadePalette(kBg6, tasks_.palette(kGlowPal), 2);
}

void EndingStage::fadeOutBackground() {
    tasks_.blackPalette(kBg5, 4);
    tasks_.blackPalette(kBg6, 4);
    tasks_.blackPalette(kBg1, 4);
    tasks_.blackPalette(kBg2, 4);
}

void EndingStage::fadeInText() {
    tasks_.fadePalette(kBg0, tasks_.palette(kTextPal), 4);
    tasks_.fadePalette(kSprite1, tasks_.palette(kTextPal), 4);
}

// _c3e206: the text goes down a pixel every four frames for four seconds while its palette goes out, and the sprites of the
// names come in.
void EndingStage::scrollText() {
    tasks_.frameCounter = 0;
    tasks_.bg3Target = 0xF0;
    tasks_.create(0, &EndingTasks::bg3ScrollTask);
    tasks_.blackPalette(kBg0, 4);
    tasks_.blackPalette(kSprite1, 4);
    tasks_.fadePalette(kSprite0, tasks_.palette(kTextPal), 4);
}

void EndingStage::fadeOutSprites0() { tasks_.blackPalette(kSprite0, 4); }
void EndingStage::fadeInSprites4() { tasks_.fadePalette(kSprite4, tasks_.palette(kSpritePal), 2); }
void EndingStage::fadeOutSprites4() { tasks_.blackPalette(kSprite4, 4); }

void EndingStage::scrollBackground(int frames) {
    EndingTasks::Task& task = tasks_.create(0, &EndingTasks::bg1ScrollTask);
    task.counter = frames;
}

// What every scene does after its first state: wait, and go on to the fade out.
void EndingStage::commonStates() {
    if (state_ == kFadeCredits) {
        if (wait_ != 0) return;
        setWait(120);
        state_ = kWaitFade;
        fadeOutSprites0();
    } else if (state_ == kWaitFade) {
        if (wait_ == 0) state_ = kTerminate;
    }
}

void EndingStage::startScene() {
    // InitEndingGfx: the palettes are black, and what the scene puts over the table comes in.
    for (const int first : {kBg0, kBg1, kBg2, kBg5, kBg6, kSprite0, kSprite1, kSprite4}) {
        for (int i = 0; i < 16; ++i) ppu_.cgram[static_cast<size_t>(first + i)] = 0;
    }
    const auto picture = [&](const Picture& p) { copyRect(p.source, p.destination, p.width, p.rows); };
    const auto animation = [&](int priority, int bank, int address, int x, int y, int flags) -> EndingTasks::Task& {
        EndingTasks::Task& task = tasks_.create(priority, &EndingTasks::animTask);
        tasks_.setAnimation(task, bank, address);
        task.posX = x << 8;
        task.posY = y << 8;
        task.flags = flags;
        return task;
    };
    switch (scene_) {
        case kShadow:
            loadSprites(data_.endingGfx2, data_.endingGfx2.size(), 0x6000);
            fadeInBackground();
            fadeInSprites4();
            scrollBackground(0x138);
            animation(3, EndingTasks::kBankEnding1, kShadowAppleAnim, 0xC0, 0x57, 1);
            break;
        case kCyan:
            loadSprites(data_.endingGfx3, 0xDF0, 0x6000);
            picture(kCyanPicture);
            fadeInBackground();
            tasks_.fadePalette(kSprite4, tasks_.palette(kSparklePal), 1);
            tasks_.bg1HScroll = 0xFFB8;
            scrollBackground(0x4B0);
            tasks_.create(0, &EndingTasks::cyanSwordTask);
            break;
        case kEdgarSabin:
            loadSprites(data_.endingGfx4, data_.endingGfx4.size(), 0x6000);
            fadeInBackground();
            fadeInSprites4();
            tasks_.create(2, &EndingTasks::coinTask);
            break;
        case kMog:
            loadSprites(data_.endingGfx3, 0xDF0, 0x6000);
            picture(kMogPicture);
            fadeInBackground();
            fadeInSprites4();
            scrollBackground(0x138);
            animation(3, EndingTasks::kBankEnding2, kMogMiniMoogleAnim, 0xE0, 0x6F, 1);
            for (const int x : {0xBA, 0xC6, 0xD1, 0xDC}) tasks_.create(2, &EndingTasks::miniMoogleTask).posX = x << 8;
            break;
        case kGogo: {
            loadSprites(data_.endingGfx3, 0xDF0, 0x6000);
            picture(kGogoPicture);
            fadeInBackground();
            scrollBackground(0x4B0);
            animation(3, EndingTasks::kBankEnding2, kGogoGlimmerAnim, 0xC8, 0x61, 1);
            tasks_.create(2, &EndingTasks::gogoGlimmerTask);
            break;
        }
        case kGau:
            loadSprites(data_.endingGfx5, 0xC0, 0x6000, true);
            picture(kGauPicture);
            fadeInBackground();
            fadeInSprites4();
            scrollBackground(0x138);
            break;
        case kTerra:
            loadSprites(data_.endingGfx3, 0xDF0, 0x6000);
            picture(kTerraPicture);
            fadeInBackground();
            tasks_.fadePalette(kSprite4, tasks_.palette(kSparklePal), 1);
            scrollBackground(0x138);
            break;
        case kLockeCeles:
            picture(kLockePicture);
            fadeInBackground();
            scrollBackground(0x138);
            tasks_.bg1HScroll = 0xFFE0;
            break;
        case kRelm:
            loadSprites(data_.endingGfx3, 0xDF0, 0x6000);
            picture(kRelmPicture);
            fadeInBackground();
            tasks_.fadePalette(kSprite4, tasks_.palette(kSparklePal), 1);
            scrollBackground(0x138);
            tasks_.create(0, &EndingTasks::relmBrushTask);
            break;
        case kStrago: {
            picture(kBookPicture);
            fadeInBackground();
            EndingTasks::Task& book = tasks_.create(1, &EndingTasks::bookTask);
            tasks_.setAnimation(book, EndingTasks::kBankEnding1, kBookAnimStrago);
            break;
        }
        case kSetzer: {
            loadSprites(data_.endingGfx3, 0xDF0, 0x6000);
            fadeInBackground();
            tasks_.fadePalette(kSprite2, tasks_.palette(kSpritePal), 2);
            scrollBackground(0x4B0);
            // InitSetzerCardsAnim: four cards that drift.
            struct Card {
                int x, y, speedX, speedY;
            };
            for (const Card& card : {Card{0x50, 0, 0x20, 0x80}, Card{0x48, 0xC0, 0x40, 0x60}, Card{0x98, 0x10, -0x20, 0x80}, Card{0xD0, 0x80, 0xA0, 0x80}}) {
                EndingTasks::Task& task = animation(0, EndingTasks::kBankEnding1, kSetzerCardAnim, card.x, card.y, 0);
                task.speedX = card.speedX & 0xFFFF;
                task.speedY = card.speedY & 0xFFFF;
            }
            break;
        }
        case kUmaro: {
            loadSprites(data_.endingGfx3, 0xDF0, 0x6000);
            loadSprites(data_.endingGfx4, 0x800, 0x6800);
            picture(kUmaroPicture);
            fadeInBackground();
            fadeInSprites4();
            scrollBackground(0x138);
            animation(3, EndingTasks::kBankEnding2, kUmaroSkullAnim, 0xC4, 0x4F, 1);
            // _c3ea7c: four mini-moogles that walk to the right, and stop where their animations say.
            struct Walker {
                int x, counter, animation;
            };
            for (const Walker& walker : {Walker{0x3A, 0x1A4, kUmaroMiniMoogleAnim4}, Walker{0x2E, 0x1B8, kUmaroMiniMoogleAnim4 + 6},
                                         Walker{0x21, 0x1CC, kUmaroMiniMoogleAnim4 + 12}, Walker{0x14, 0x1E0, kUmaroMiniMoogleAnim4 + 18}}) {
                EndingTasks::Task& task = tasks_.create(2, &EndingTasks::miniMoogleTask);
                task.state = 5;
                task.posX = walker.x << 8;
                task.counter = walker.counter;
                task.saved = walker.animation;
            }
            break;
        }
        default: break;
    }
    // DrawEndingCharText: the "as" and the letters of the full name, which the tasks bring in.
    tasks_.bigNameStop = false;
    tasks_.bigNameWait = 300;
    tasks_.createAs();
    tasks_.createBigName(kCharacters[scene_].first);
}

// The state code of the game for one frame: each state waits for its counter, then does what comes next.
void EndingStage::step() {
    if (state_ == first_) {
        startScene();
        setWait(120);
        ++state_;
        return;
    }
    if (state_ == kFadeCredits || state_ == kWaitFade) {
        commonStates();
        return;
    }
    if (wait_ != 0) return;
    // The second name of the scenes of two characters replaces the first one on the background of the text.
    const auto secondName = [&] {
        tasks_.createAs();
        tasks_.bg3VScroll = 0;
        nameIndex_ = std::min<size_t>(1, names_.size() - 1);
        fadeOutSprites0();
        fadeInText();
        ++state_;
        setWait(120);
    };
    const auto secondLetters = [&] {  // the letters of the first name end, then the ones of the second come in sooner
        tasks_.bigNameStop = false;
        tasks_.bigNameWait = 0x14;
        tasks_.createBigName(kCharacters[scene_].second);
    };
    const auto fadeOut = [&](bool scrolls) {
        state_ = kFadeCredits;
        setWait(120);
        fadeOutBackground();
        fadeOutSprites4();
        if (scrolls) scrollBackground(0xB4);
    };
    const int phase = state_ - first_;
    if (scene_ == kEdgarSabin) {
        switch (state_) {
            case 65: ++state_; fadeInText(); setWait(240); break;
            case 66: ++state_; scrollText(); setWait(240); break;
            case 67: secondName(); break;
            case 68: ++state_; setWait(180); tasks_.bigNameStop = true; break;
            case 69: state_ = 79; setWait(180); secondLetters(); scrollText(); break;
            default: fadeOut(false); break;
        }
    } else if (scene_ == kLockeCeles) {
        switch (state_) {
            case 101: ++state_; fadeInText(); setWait(240); break;
            case 102: ++state_; scrollText(); setWait(240); break;
            case 103: secondName(); break;
            case 104: ++state_; setWait(180); tasks_.bigNameStop = true; break;
            case 105: ++state_; setWait(180); secondLetters(); scrollText(); break;
            default: fadeOut(true); break;
        }
    } else if (scene_ == kSetzer) {
        switch (state_) {
            case 131: ++state_; fadeInText(); setWait(240); break;
            case 132: ++state_; scrollText(); setWait(360); break;
            case 133: ++state_; setWait(120); fadeOutBackground(); break;
            default:
                setWait(120);
                state_ = kWaitFade;
                fadeOutSprites0();
                tasks_.blackPalette(kSprite2, 4);
                break;
        }
    } else if (phase == 1) {  // the text comes in
        ++state_;
        fadeInText();
        setWait(240);
    } else if (phase == 2) {  // the text goes down, and the last of the animations of the scene starts
        ++state_;
        const auto sprite = [&](int address, int x, int y, int flags) {
            EndingTasks::Task& task = tasks_.create(3, &EndingTasks::animTask);
            tasks_.setAnimation(task, EndingTasks::kBankEnding2, address);
            task.posX = x << 8;
            task.posY = y << 8;
            task.flags = flags;
        };
        if (scene_ == kGau) sprite(kGauEyesAnim, 0xCB, 0x5F, 1);
        if (scene_ == kTerra) sprite(kTerraPendantAnim, 0x80, 0x60, 0);
        scrollText();
        setWait(360);
    } else {  // the fade out
        fadeOut(scene_ == kShadow || scene_ == kMog || scene_ == kGau || scene_ == kTerra || scene_ == kRelm || scene_ == kUmaro);
    }
}

void EndingStage::tick() {
    step();
    tasks_.exec(ppu_.sprites);
    ++tasks_.frameCounter;
    ++frame_;
    if (wait_ > 0) --wait_;
    for (size_t i = 0; i < map_.size(); ++i) ppu_.vram[i] = map_[i];
    for (PpuLine& line : ppu_.lines) {
        line.bgMode = 0x09;
        line.cgwsel = 0x82;
        line.cgadsub = 0xD1;
        line.hofs[0] = static_cast<uint16_t>(tasks_.bg1HScroll);
        line.w12sel = 0x33;
        line.wobjsel = 0x33;
        line.tmw = 0x0F;
        line.window = {0x08, 0xF7, 0x08, 0xF7};
    }
}

// The name goes in the big letters at the bottom of the screen, from the palette that fades in and out, and goes down by the scroll.
void EndingStage::drawText(uint8_t* rgba) {
    if (nameIndex_ >= names_.size()) return;
    const int level = std::min(31, static_cast<int>(ppu_.cgram[5] & 31));
    if (level == 0) return;
    const int scroll = static_cast<int16_t>(tasks_.bg3VScroll);
    const int down = -scroll;
    if (down < 0 || kNameRow * FF6_MENU_TILE_SIZE + down >= FF6_SCREEN_HEIGHT) return;
    screen_.clear();
    screen_.setTextOffset(down % FF6_MENU_TILE_SIZE);
    const std::string& name = names_[nameIndex_];
    screen_.text((FF6_SCREEN_WIDTH / FF6_MENU_TILE_SIZE - static_cast<int>(name.size())) / 2, kNameRow + down / FF6_MENU_TILE_SIZE, name,
                 MenuScreen::Layer::Bg3, FF6_MENU_BG3_DEFAULT);
    screen_.setTextOffset(0);
    const std::vector<uint8_t>& text = screen_.pixels();
    for (size_t i = 0; i + FF6_BYTES_PER_PIXEL <= text.size(); i += FF6_BYTES_PER_PIXEL) {
        if (text[i] == 0 && text[i + 1] == 0 && text[i + 2] == 0) continue;
        for (int c = 0; c < FF6_ALPHA_OFFSET; ++c) rgba[i + static_cast<size_t>(c)] = static_cast<uint8_t>(text[i + static_cast<size_t>(c)] * level / 31);
    }
}

void EndingStage::draw(uint8_t* rgba) {
    ppu_.render(rgba);
    drawText(rgba);
}

}  // namespace ff6
