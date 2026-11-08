#include <cstdlib>
#include <string>
#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "cutscene/opening.hpp"
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
}
