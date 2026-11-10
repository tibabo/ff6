#include <cstdlib>
#include <string>
#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "cutscene/opening.hpp"
#include "cutscene/ending_card.hpp"
#include "cutscene/ending_names.hpp"
#include "cutscene/flight.hpp"
#include "cutscene/floating.hpp"
#include "cutscene/ruin.hpp"
#include "cutscene/the_end.hpp"
#include "cutscene/title.hpp"
#include "data/title_data.hpp"

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
        for (int i = 0; i < 20; ++i) opening.tick({});
        const ff6::Ppu& ppu = opening.core().ppu;
        CHECK(ppu.lines[10].bgMode == 0x09 && ppu.lines[100].bgMode == 0x07);
        CHECK(ppu.lines[100].m7a == 0x4000 / (((100 - 24) * 292 >> 8) & 0xFF) && ppu.lines[100].m7a == ppu.lines[100].m7d);
        CHECK(ppu.lines[200].m7a < ppu.lines[100].m7a && ppu.lines[100].m7b == 0 && ppu.lines[100].m7c == 0);
        CHECK(ppu.lines[90].fixed[0] > ppu.lines[200].fixed[0] && ppu.lines[10].fixed[0] == 0);
        for (int i = 20; i < 260; ++i) opening.tick({});
        CHECK(litPixels(opening) > 3000);  // the ground, the armors and the first names
        CHECK(opening.core().ram[0x4458] == 0x2C || opening.core().ram[0x4458] == 0x01);  // "producer" begins with a p
        const int frames = 260 + tickUntilFinished(opening, 4000);
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
        for (const int state : {2, 4, 9, 12, 16, 20, 24, 28}) {
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
    {  // the big letters of a name: nothing for five seconds, then they rise in and the palette fades in; Edgar, then Sabin
        const auto lit = [](const ff6::EndingNames& names) {
            std::vector<uint8_t> pixels(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
            names.draw(pixels.data());
            size_t count = 0;
            for (size_t i = 0; i < pixels.size(); i += FF6_BYTES_PER_PIXEL) count += pixels[i] > 20 || pixels[i + 1] > 20 || pixels[i + 2] > 20;
            return count;
        };
        const std::unique_ptr<ff6::EndingNames> cyan = ff6::makeEndingNames(data, 50);
        CHECK(cyan != nullptr && ff6::makeEndingNames(data, 45) == nullptr);
        for (int i = 0; i < 100; ++i) cyan->tick();
        CHECK(lit(*cyan) == 0);  // the letters wait, and the palette is still black
        for (int i = 0; i < 600; ++i) cyan->tick();
        CHECK(lit(*cyan) > 300);
        const std::unique_ptr<ff6::EndingNames> brothers = ff6::makeEndingNames(data, 64);
        for (int i = 0; i < 1150; ++i) brothers->tick();
        CHECK(lit(*brothers) > 300);
    }
}
