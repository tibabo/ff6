#include <cstdlib>
#include <string>
#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "cutscene/opening.hpp"
#include "cutscene/ending_book.hpp"
#include "cutscene/ending_card.hpp"
#include "cutscene/ending_stage.hpp"
#include "cutscene/falcon.hpp"
#include "cutscene/ending_tasks.hpp"
#include "cutscene/flight.hpp"
#include "cutscene/floating.hpp"
#include "cutscene/ruin.hpp"
#include "cutscene/the_end.hpp"
#include "cutscene/title.hpp"
#include "data/title_data.hpp"
#include "data/world_data.hpp"
#include "menu/menu_data.hpp"
#include "menu/menu_screen.hpp"

namespace {

size_t litPixels(ff6::Cutscene& scene) {
    std::vector<uint8_t> pixels(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
    scene.draw(pixels.data());
    size_t lit = 0;
    for (size_t i = 0; i < pixels.size(); i += FF6_BYTES_PER_PIXEL) lit += pixels[i] > 40 || pixels[i + 1] > 40 || pixels[i + 2] > 40;
    return lit;
}

int tickUntilFinished(ff6::Cutscene& scene, int limit) {
    int frames = 0;
    while (!scene.finished() && frames < limit) {
        scene.tick({});
        ++frames;
    }
    return frames;
}

}  // namespace

// Plays the title screen and the opening of the real data pack when FF6_PACK points to one.
void testCutsceneScenes() {
    const char* pack = std::getenv("FF6_PACK");
    if (!pack) return;
    const ff6::TitleData data = ff6::loadTitleData(pack);
    CHECK(data.program.size() == FF6_TITLE_PROGRAM_BYTES && data.gfx.size() == FF6_TITLE_GFX_BYTES);
    CHECK(data.byteAt(0x5000) == 0x4C && data.wordAt(0x5001) == 0x500C);  // the jump to the title
    CHECK(data.wordAt(0x6598) == 0x4458 && data.sine[64] == 0x7F && data.random[0] == 0x07);

    {  // the title screen: the license first, the music a little later, the whole thing in about 4700 frames
        test::RecordingSound sound;
        ff6::TitleScene title(data, sound);
        CHECK(sound.commands.size() == 1 && sound.commands[0].command == FF6_SOUND_PLAY_SONG && sound.commands[0].arg1 == 2 &&
              sound.commands[0].arg2 == 0);
        for (int i = 0; i < 20; ++i) title.tick({});
        CHECK(!title.finished() && litPixels(title) > 1000);  // the logo and the text of the license, on a light screen
        for (int i = 20; i < 340; ++i) title.tick({});
        CHECK(title.core().state == 1 && sound.commands.size() == 1);  // the license is over, and the clouds scroll in silence
        for (int i = 340; i < 420; ++i) title.tick({});
        CHECK(title.core().state == 2 && sound.commands.size() == 2 && sound.commands[1].arg2 == FF6_SOUND_FULL_VOLUME);
        const int frames = 420 + tickUntilFinished(title, 6000);
        CHECK(title.finished() && !title.skipped() && frames > 4500 && frames < 5000);
        CHECK(sound.commands.back().command == FF6_SOUND_STOP_ALL);
    }
    {  // a press of the button ends it and is remembered
        test::RecordingSound sound;
        ff6::TitleScene title(data, sound);
        for (int i = 0; i < 600; ++i) title.tick({});
        title.tick({FF6_NO_DIRECTION, true});
        CHECK(!title.finished());
        CHECK(tickUntilFinished(title, 60) <= 20 && title.finished() && title.skipped());
        // During the license, a press does nothing.
        ff6::TitleScene splash(data, sound);
        for (int i = 0; i < 10; ++i) splash.tick({FF6_NO_DIRECTION, true});
        CHECK(!splash.finished() && !splash.skipped());
    }
    {  // the opening: the ground in perspective below the horizon, the credits above it
        test::RecordingSound sound;
        ff6::OpeningScene opening(data, sound);
        CHECK(sound.commands.size() == 1 && sound.commands[0].arg1 == 4 && sound.commands[0].arg2 == FF6_SOUND_FULL_VOLUME);
        constexpr int kStep = FF6_OPENING_FRAMES_PER_STEP;  // the opening advances every other frame
        for (int i = 0; i < 20 * kStep; ++i) opening.tick({});
        const ff6::Ppu& ppu = opening.core().ppu;
        CHECK(ppu.lines[10].bgMode == 0x09 && ppu.lines[100].bgMode == 0x07);
        CHECK(ppu.lines[100].m7a == 0x4000 / (((100 - 24) * 292 >> 8) & 0xFF) && ppu.lines[100].m7a == ppu.lines[100].m7d);
        CHECK(ppu.lines[200].m7a < ppu.lines[100].m7a && ppu.lines[100].m7b == 0 && ppu.lines[100].m7c == 0);
        CHECK(ppu.lines[90].fixed[0] > ppu.lines[200].fixed[0] && ppu.lines[10].fixed[0] == 0);
        for (int i = 20 * kStep; i < 260 * kStep; ++i) opening.tick({});
        CHECK(litPixels(opening) > 3000);  // the ground, the armors and the first names
        CHECK(opening.core().ram[0x4458] == 0x2C || opening.core().ram[0x4458] == 0x01);  // "producer" begins with a p
        const int frames = (260 * kStep + tickUntilFinished(opening, 4000 * kStep)) / kStep;
        CHECK(opening.finished() && !opening.skipped() && frames > 3400 && frames < 3700);
        CHECK(sound.commands.back().command == FF6_SOUND_STOP_ALL);
    }
    {  // a press of the button ends the opening too
        test::RecordingSound sound;
        ff6::OpeningScene opening(data, sound);
        for (int i = 0; i < 100; ++i) opening.tick({});
        opening.tick({FF6_NO_DIRECTION, true});
        CHECK(tickUntilFinished(opening, 60) <= 20 && opening.finished() && opening.skipped());
    }
    {  // the world falls apart: the planet and the lands show, the sparks come, and it ends after the fade
        test::RecordingSound sound;
        ff6::RuinScene ruin(data, sound);
        for (int i = 0; i < 400; ++i) ruin.tick({});
        CHECK(!ruin.finished() && litPixels(ruin) > 500);
        const int frames = 400 + tickUntilFinished(ruin, 3000);
        CHECK(ruin.finished() && !ruin.skipped() && frames > 1400 && frames < 1600);
    }
    {  // the continent comes in view as the plane goes away, the color covers it, and the scene ends after the fade
        test::RecordingSound sound;
        ff6::FloatingScene floating(data, sound);
        for (int i = 0; i < 300; ++i) floating.tick({});
        CHECK(!floating.finished() && litPixels(floating) > 1000);
        const int frames = 300 + tickUntilFinished(floating, 3000);
        CHECK(floating.finished() && !floating.skipped() && frames > 680 && frames < 720);
    }
    {  // THE END waits for the music, shows the letters through the window, and the stars fly out of the middle
        test::RecordingSound sound;
        ff6::TheEndScene end(data, sound);
        for (int i = 0; i < 20; ++i) end.tick({});
        CHECK(end.phase() == 1 && litPixels(end) == 0);  // nothing shows before the music gives its sign
        sound.ports[1] = 1;
        for (int i = 0; i < 120; ++i) end.tick({});
        CHECK(end.phase() == 3 && litPixels(end) > 100);  // the letters of the window, in the color of the palette
        sound.ports[1] = 2;
        for (int i = 0; i < 140; ++i) end.tick({});
        const size_t reflected = litPixels(end);
        sound.ports[3] = 0;
        for (int i = 0; i < 1200 + 70; ++i) end.tick({});
        CHECK(end.phase() == 8 && litPixels(end) > reflected + 50);  // the stars are on
        CHECK(sound.commands.size() == 2 && sound.commands[1].arg2 == FF6_SOUND_FULL_VOLUME);
        end.tick({FF6_NO_DIRECTION, true});
        CHECK(end.finished() && !end.skipped());
    }
    {  // the credits of the flight: the words that each page has, from the tables of the ROM
        const std::vector<ff6::CreditsPage> tiny = ff6::creditsPages(ff6::creditsScene(9), data.credits);
        CHECK(tiny.size() == 7 && tiny[0].start == 10 && tiny[1].start == 430 && tiny[0].words.size() == 5);
        CHECK(tiny[0].words[0].text == "director" && tiny[0].words[0].x == 0x60 && tiny[0].words[0].y == 0x88);
        CHECK(tiny[0].words[1].text == "YOSHINORI" && tiny[0].words[2].text == "KITASE");
        const std::vector<ff6::CreditsPage> land = ff6::creditsPages(ff6::creditsScene(24), data.credits);
        CHECK(land.size() == 7 && land[1].start == 720 && !land[6].words.empty());
        CHECK(ff6::creditsPages(ff6::creditsScene(50), data.credits).empty());
        CHECK(ff6::endingFrames(50) > 840 && ff6::endingCharacters(100).size() == 2 && ff6::endingFrames(3) == 0);
    }
    {  // the ground of the flights: land, sea and clouds seen by the camera of the script, with a sky above the horizon for some
        for (const int state : {2, 4, 5, 9, 12, 16, 20, 24, 28, 32}) {
            const std::unique_ptr<ff6::FlightBackdrop> ground = ff6::makeFlightBackdrop(data, state);
            CHECK(ground != nullptr);
            if (!ground) continue;
            for (int i = 0; i < 120; ++i) ground->tick();
            std::vector<uint8_t> pixels(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
            ground->draw(pixels.data());
            size_t lit = 0;
            for (size_t i = 0; i < pixels.size(); i += FF6_BYTES_PER_PIXEL) lit += pixels[i] > 20 || pixels[i + 1] > 20 || pixels[i + 2] > 20;
            CHECK(lit > 20000);
        }
        CHECK(ff6::makeFlightBackdrop(data, 50) == nullptr);
        const std::unique_ptr<ff6::FlightBackdrop> sea = ff6::makeFlightBackdrop(data, 16);
        CHECK(sea->ppu().lines[10].bgMode == 1 && sea->ppu().lines[100].bgMode == 7);  // the sky above the horizon
        CHECK(sea->ppu().lines[200].m7a != 0 && sea->ppu().lines[100].m7a != sea->ppu().lines[200].m7a);  // the scale changes down the screen
    }
    {  // the book: closed on the table, its pages turn after twelve seconds, and "and you" comes with letters
        const std::unique_ptr<ff6::EndingBook> book = ff6::makeEndingBook(data, 45);
        CHECK(book != nullptr && ff6::makeEndingBook(data, 50) == nullptr);
        const auto picture = [](const ff6::EndingBook& scene) {
            std::vector<uint8_t> pixels(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
            scene.draw(pixels.data());
            return pixels;
        };
        for (int i = 0; i < 300; ++i) book->tick();
        const std::vector<uint8_t> closed = picture(*book);
        size_t lit = 0;
        for (size_t i = 0; i < closed.size(); i += FF6_BYTES_PER_PIXEL) lit += closed[i] > 20 || closed[i + 1] > 20 || closed[i + 2] > 20;
        CHECK(lit > 3000);  // the book and the table
        for (int i = 0; i < 470; ++i) book->tick();
        CHECK(picture(*book) != closed);  // a page turns
        const std::unique_ptr<ff6::EndingBook> you = ff6::makeEndingBook(data, 74);
        for (int i = 0; i < 100; ++i) you->tick();
        const std::vector<uint8_t> before = picture(*you);
        for (int i = 0; i < 500; ++i) you->tick();
        CHECK(picture(*you) != before);
        const std::vector<uint8_t> shut = picture(*you);
        for (int i = 0; i < 100; ++i) you->tick();
        const std::vector<uint8_t> later = picture(*you);
        // The book stays shut once it has closed (only the letters below move).
        const size_t bookBytes = static_cast<size_t>(FF6_SCREEN_WIDTH) * 150 * FF6_BYTES_PER_PIXEL;
        CHECK(std::equal(shut.begin(), shut.begin() + static_cast<std::ptrdiff_t>(bookBytes), later.begin()));
    }
    {  // the palettes fade as the game has it: a color climbs for the last steps only, and goes down from the first one
        std::array<uint16_t, 256> cgram{};
        ff6::EndingTasks tasks(data, cgram);
        std::vector<ff6::PpuSprite> sprites;
        std::array<uint16_t, 16> target{};
        target[1] = 0x7FFF;  // 31 in each color
        target[2] = 0x000F;  // 15 in red only
        cgram[3] = 0x7FFF;
        tasks.fadePalette(0, target, 1);
        for (int i = 0; i < 16; ++i) tasks.exec(sprites);
        CHECK((cgram[1] & 31) == 15 && (cgram[2] & 31) == 0 && (cgram[3] & 31) == 16);  // 15 steps: the first color climbs from the start, the second has waited
        for (int i = 0; i < 20; ++i) tasks.exec(sprites);
        CHECK(cgram[1] == 0x7FFF && cgram[2] == 0x000F && cgram[3] == 0 && tasks.active() == 0);
    }
    {  // the scenes of the characters last what the waits of the game add up to, with the sprites that they start
        ff6::CommonData common = ff6::loadCommonData(pack);
        const ff6::MenuData menuData = ff6::loadMenuData(pack);
        ff6::MenuScreen menuScreen(common, menuData);
        struct Expected {
            int state, frames;
        };
        for (const Expected& scene : {Expected{40, 961}, Expected{50, 961}, Expected{60, 961}, Expected{64, 1321}, Expected{70, 961}, Expected{80, 961},
                                      Expected{90, 961}, Expected{100, 1321}, Expected{110, 961}, Expected{120, 961}, Expected{130, 961},
                                      Expected{135, 961}}) {
            CHECK(ff6::EndingStage::handles(scene.state));
            ff6::EndingStage stage(data, menuScreen, scene.state, {"Terra", "Locke"});
            int frames = 0;
            while (!stage.finished() && frames < 3000) {
                stage.tick();
                ++frames;
            }
            CHECK(frames == scene.frames);
        }
        CHECK(!ff6::EndingStage::handles(45) && !ff6::EndingStage::handles(41));
        // what each scene puts over its picture, with the data of the ROM: the apple, the sparkles, the coin, the mini-moogles...
        for (const int state : {40, 50, 60, 64, 70, 80, 90, 110, 130, 135}) {
            ff6::EndingStage stage(data, menuScreen, state, {"Terra"});
            for (int i = 0; i < (state == 80 ? 560 : 380); ++i) stage.tick();  // the eyes of Gau open after three seconds more
            CHECK(!stage.ppu().sprites.empty());
        }
        ff6::EndingStage coin(data, menuScreen, 64, {"Edgar", "Sabin"});
        for (int i = 0; i < 150; ++i) coin.tick();
        const auto isCoin = [](const ff6::PpuSprite& sprite) { return sprite.x == 93 && sprite.y == 101; };
        CHECK(std::any_of(coin.ppu().sprites.begin(), coin.ppu().sprites.end(), isCoin));  // the coin, half a pixel a frame from 0x10
        // Cyan: the table and the sword from the start, the sparkles on its blade after six seconds, the name once its palette is in
        ff6::EndingStage cyan(data, menuScreen, 50, {"Cyan"});
        const auto picture = [](ff6::EndingStage& stage) {
            std::vector<uint8_t> pixels(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
            stage.draw(pixels.data());
            return pixels;
        };
        const auto litIn = [](const std::vector<uint8_t>& pixels, int y0, int y1) {
            size_t count = 0;
            for (int y = y0; y < y1; ++y) {
                for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
                    const size_t at = (static_cast<size_t>(y) * FF6_SCREEN_WIDTH + static_cast<size_t>(x)) * FF6_BYTES_PER_PIXEL;
                    count += pixels[at] > 20 || pixels[at + 1] > 20 || pixels[at + 2] > 20;
                }
            }
            return count;
        };
        for (int i = 0; i < 80; ++i) cyan.tick();
        CHECK(litIn(picture(cyan), 100, 176) > 1000 && litIn(picture(cyan), 176, 224) == 0);  // the name is not there yet
        for (int i = 80; i < 250; ++i) cyan.tick();
        CHECK(litIn(picture(cyan), 176, 190) > 20 && litIn(picture(cyan), 192, 210) > 10);  // the name, on its palette, and the "as" under it
        const size_t before = litIn(picture(cyan), 84, 112);
        for (int i = 250; i < 372; ++i) cyan.tick();
        CHECK(litIn(picture(cyan), 84, 112) > before);  // a sparkle over the sword
        for (int i = 372; i < 600; ++i) cyan.tick();
        CHECK(litIn(picture(cyan), 176, 224) > 100 && litIn(picture(cyan), 224 - 8, 224) == 0);  // the name and the "as" are gone, the letters of the full name have risen
    }
    {  // the sprites of the flights: the birds and the airship of the sea, the tiny airship that comes after sixteen seconds, the boat
        const auto count = [&](int state, int frames) {
            const std::unique_ptr<ff6::FlightBackdrop> ground = ff6::makeFlightBackdrop(data, state);
            for (int i = 0; i < frames; ++i) ground->tick();
            return ground->ppu().sprites.size();
        };
        CHECK(count(16, 5) >= 12 && count(20, 5) >= 12 && count(28, 5) >= 5);
        CHECK(count(9, 5) == 0 && count(9, 15) > 0);  // the words of the credits are sprites, and the first page comes at frame 10
        {
            const std::unique_ptr<ff6::FlightBackdrop> tiny = ff6::makeFlightBackdrop(data, 9);
            for (int i = 0; i < 20; ++i) tiny->tick();
            bool font = !tiny->ppu().sprites.empty();
            for (const ff6::PpuSprite& sprite : tiny->ppu().sprites) font = font && (sprite.attr == 0x09 || sprite.attr == 0x0B) && sprite.tile >= 0x100;
            CHECK(font);  // the big font on the palette 4, the small one on the 5, in the second table of the tiles
            bool bigTile = false;
            for (size_t row = 0; row < 8; ++row) bigTile = bigTile || tiny->ppu().vram[0x7000 + 0x40 * 16 + row] != 0;
            CHECK(bigTile);  // the first letter of the big font is in its sheet
        }
        CHECK(count(12, 5) >= 6 && count(12, 1200) > count(12, 1000));
        CHECK(count(24, 100) == 0);
        const std::unique_ptr<ff6::FlightBackdrop> sea = ff6::makeFlightBackdrop(data, 16);
        sea->tick();
        CHECK(sea->ppu().lines[10].wobjsel == 0x33 && sea->ppu().lines[10].cgwsel == 0x80 && sea->ppu().lines[100].cgwsel == 0x82);  // the color window
    }
    {  // the airship above the clouds and the jet: the scales go on their own, and the jet is the only thing on the sky of its plane
        const std::unique_ptr<ff6::FlightBackdrop> airship = ff6::makeFlightBackdrop(data, 5);
        for (int i = 0; i < 100; ++i) airship->tick();
        CHECK(airship->ppu().lines[10].m7Own && !airship->ppu().lines[150].m7Own && airship->ppu().lines[10].m7a == 0x10 + 50);
        const std::unique_ptr<ff6::FlightBackdrop> jet = ff6::makeFlightBackdrop(data, 32);
        for (int i = 0; i < 10; ++i) jet->tick();
        CHECK(jet->ppu().m7sel == 0x80 && jet->ppu().lines[0].m7a == 1 + 2 * 10 && jet->ppu().lines[200].m7a == jet->ppu().lines[0].m7a);
    }
    {  // the flight of the Falcon: the ground in perspective, one sprite that crosses the screen, and a fade at each end
        const ff6::WorldData world = ff6::loadWorldData(pack);
        ff6::FalconScene falcon(data, world);
        for (int i = 0; i < 100; ++i) falcon.tick({});
        CHECK(!falcon.ppu().sprites.empty() && falcon.ppu().brightness == 15 && falcon.ppu().lines[0].m7a > falcon.ppu().lines[200].m7a);
        CHECK(falcon.ppu().sprites.size() == 15);  // the Falcon alone: the particles have nothing to show once the first ones are gone
        CHECK(falcon.ppu().lines[0].fixed[2] == 18 && falcon.ppu().lines[150].fixed[2] == 0);  // the haze, blue first
        ff6::FalconScene whole(data, world);
        const int frames = tickUntilFinished(whole, 1000);
        CHECK(frames > 260 && frames < 300);  // 254 frames for the Falcon to leave, 15 of fade out
    }
}
