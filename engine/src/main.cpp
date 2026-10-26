#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "audio/audio_data.hpp"
#include "audio/sound_program.hpp"
#include "data/save.hpp"
#include "game.hpp"
#include "sdl/sdl_audio.hpp"
#include "sdl/sdl_input.hpp"
#include "sdl/sdl_renderer.hpp"

#define FF6_MAX_CATCHUP_FRAMES 5  // longest stall, in frames, that the loop tries to make up for
#define FF6_CAPTURE_MAX_STEP_FRAMES 32  // bound on the ticks spent finishing one captured step
#define FF6_CAPTURE_MAX_TALK_FRAMES 100000
#define FF6_CAPTURE_MENU_SETTLE_FRAMES 20  // idle frames after a press in the menu, longer than a scroll of a page
#define FF6_MOVE_LETTERS "URDL"  // --moves letters, in Facing order
#define FF6_DEFAULT_MAP 0x1C  // the Narshe inn
#define FF6_MAX_MAP_ARGUMENT 0x1FF

namespace {

struct Capture {
    std::string path;
    std::string moves;
    int waitFrames = 0;  // idle frames before the moves, for npcs to walk
    int atX = -1, atY = -1;  // party tile, when it is not the start of the map
    int talkFrames = -1;
    int menuCursor = -1;  // open the menu with the cursor on this option
    int menuSelect = 0;  // and press the confirm button there this many times
    int menuDowns = 0;  // then press down this many times
};

// Keeps SDL initialised for as long as it lives.
struct SdlRuntime {
    SdlRuntime() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }
    ~SdlRuntime() { SDL_Quit(); }
    SdlRuntime(const SdlRuntime&) = delete;
    SdlRuntime& operator=(const SdlRuntime&) = delete;
};

// Plays the scripted moves and dialogue, then saves one frame.
void captureFrame(ff6::Game& game, ff6::SdlRenderer& renderer, const Capture& capture) {
    const char* const letters = FF6_MOVE_LETTERS;
    for (int frame = 0; frame < capture.waitFrames; ++frame) game.tick({FF6_NO_DIRECTION, false});
    for (const char move : capture.moves) {
        const char* found = std::strchr(letters, move);
        if (!found || move == '\0') throw std::runtime_error("--moves only accepts U, R, D and L");
        game.tick({static_cast<int>(found - letters), false});
        for (int frame = 0; frame < FF6_CAPTURE_MAX_STEP_FRAMES && game.player().moving(); ++frame) {
            game.tick({FF6_NO_DIRECTION, false});
        }
    }
    if (capture.talkFrames >= 0) {
        game.tick({FF6_NO_DIRECTION, true});
        for (int frame = 0; frame < capture.talkFrames; ++frame) game.tick({FF6_NO_DIRECTION, false});
    }
    game.finishFade();
    if (capture.menuCursor >= 0) {
        game.tick({FF6_NO_DIRECTION, false, false, true});
        for (int frame = 0; frame < FF6_CAPTURE_MAX_STEP_FRAMES * 2 && game.menuPhase() != ff6::MenuPhase::Open; ++frame) {
            game.tick({});
        }
        game.finishFade();  // the menu fades in
        for (int option = 0; option < capture.menuCursor; ++option) {
            game.tick({ff6::Down, false});
            game.tick({});
        }
        for (int press = 0; press < capture.menuSelect; ++press) {
            game.tick({FF6_NO_DIRECTION, true});
            game.tick({});
        }
        for (int press = 0; press < capture.menuDowns; ++press) {
            game.tick({ff6::Down, false});
            for (int frame = 0; frame < FF6_CAPTURE_MENU_SETTLE_FRAMES; ++frame) game.tick({});  // a page may scroll
        }
        game.finishFade();
    }
    game.draw();
    renderer.saveFrame(capture.path);
    std::printf("Saved %s (tile %d,%d)\n", capture.path.c_str(), game.player().tileX(), game.player().tileY());
}

// Fixed-step loop: the game advances FF6_FRAMES_PER_SECOND times per second whatever the display rate.
void playGame(ff6::Game& game, ff6::SdlRenderer& renderer, const std::string& savePath) {
    ff6::SdlInput input;
    const double ticksPerFrame = static_cast<double>(SDL_GetPerformanceFrequency()) / FF6_FRAMES_PER_SECOND;
    Uint64 previous = SDL_GetPerformanceCounter();
    double accumulator = 0;
    std::printf("Map %u. Arrows/WASD or controller to move, Enter/Space/A to talk,\n"
                "F5 to save to %s, F11 or Alt+Enter for fullscreen, F10 for integer scaling, Escape to quit.\n",
                static_cast<unsigned>(game.map().id), savePath.c_str());
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input.handle(event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                const SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE) running = false;
                if (key == SDLK_F11 || (key == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT))) {
                    renderer.toggleFullscreen();
                }
                if (key == SDLK_F10) renderer.toggleIntegerScale();
                if (key == SDLK_F5) {
                    try {
                        ff6::writeSave(savePath, game.saveState());
                        std::printf("Saved the game to %s\n", savePath.c_str());
                    } catch (const std::exception& error) {
                        std::fprintf(stderr, "Could not save: %s\n", error.what());
                    }
                }
            }
        }
        const Uint64 now = SDL_GetPerformanceCounter();
        accumulator = std::min(accumulator + static_cast<double>(now - previous),
                               ticksPerFrame * FF6_MAX_CATCHUP_FRAMES);
        previous = now;
        while (accumulator >= ticksPerFrame) {
            accumulator -= ticksPerFrame;
            game.tick(input.sample());
        }
        game.draw();
    }
}

// A list of characters, by name or number: "terra,locke,edgar" or "0,1,4".
std::vector<int> parseParty(const std::string& text) {
    static const char* const kNames[] = FF6_CHARACTER_NAMES;
    std::vector<int> party;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = std::min(text.find(',', start), text.size());
        std::string word = text.substr(start, end - start);
        std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        int id = -1;
        for (int i = 0; i < static_cast<int>(sizeof(kNames) / sizeof(kNames[0])); ++i) {
            if (word == kNames[i]) id = i;
        }
        if (id < 0 && !word.empty() && std::all_of(word.begin(), word.end(), [](unsigned char c) { return std::isdigit(c); })) {
            id = std::stoi(word);
        }
        if (id < 0 || id >= FF6_CHAR_COUNT) throw std::runtime_error("invalid --party character: " + word);
        party.push_back(id);
        start = end + 1;
    }
    if (party.size() > FF6_PARTY_SIZE) throw std::runtime_error("--party takes at most 4 characters");
    return party;
}

// Where the game begins: a map and a party, or a saved game.
struct Start {
    uint32_t map = FF6_DEFAULT_MAP;
    std::vector<int> party;
    std::unique_ptr<ff6::SaveState> save;
};

std::unique_ptr<ff6::Game> makeGame(ff6::CommonData common, const std::string& packDir, const Start& start,
                                    ff6::SdlRenderer& renderer, ff6::Sound& sound) {
    if (start.save) return std::make_unique<ff6::Game>(std::move(common), packDir, *start.save, renderer, sound);
    return std::make_unique<ff6::Game>(std::move(common), packDir, start.map, renderer, sound, start.party);
}

int run(const std::string& packDir, const Start& start, const std::string& savePath, const Capture* capture, long music) {
    ff6::CommonData common = ff6::loadCommonData(packDir);
    SdlRuntime sdl;
    ff6::SdlRenderer renderer(capture != nullptr);
    if (capture) {  // no sound device for a capture
        ff6::NoSound sound;
        const std::unique_ptr<ff6::Game> game = makeGame(std::move(common), packDir, start, renderer, sound);
        if (capture->atX >= 0) game->placeParty(capture->atX, capture->atY);
        captureFrame(*game, renderer, *capture);
        return 0;
    }
    const ff6::AudioData audio = ff6::loadAudioData(packDir);
    if (music >= 0 && static_cast<size_t>(music) >= audio.songs.size()) throw std::runtime_error("invalid --music");
    ff6::SoundProgram program(audio);
    ff6::SdlAudio sound(program);
    const std::unique_ptr<ff6::Game> game = makeGame(std::move(common), packDir, start, renderer, sound);
    if (music >= 0) sound.command(FF6_SOUND_PLAY_SONG, static_cast<uint8_t>(music), FF6_SOUND_FULL_VOLUME);  // over the song of the map
    playGame(*game, renderer, savePath);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const char* usage = "Usage: %s <data-pack-dir> [--map n] [--party terra,locke,...] [--load save.json] [--save save.json] [--music song] [--capture out.bmp [--at x,y] [--wait frames] [--moves URDL...] [--talk frames]]\n";
    if (argc < 2) {
        std::fprintf(stderr, usage, argc > 0 ? argv[0] : "ff6");
        return 2;
    }
    try {
        Capture capture;
        uint32_t startMap = FF6_DEFAULT_MAP;
        long music = -1;
        std::vector<int> party;
        std::string loadPath;
        std::string savePath = FF6_SAVE_DEFAULT_FILE;
        for (int i = 2; i < argc; i += 2) {
            const std::string option = argv[i];
            if (i + 1 >= argc) throw std::runtime_error("missing value for " + option);
            if (option == "--map") {
                const long map = std::stol(argv[i + 1], nullptr, 0);
                if (map < 0 || map > FF6_MAX_MAP_ARGUMENT) throw std::runtime_error("invalid --map");
                startMap = static_cast<uint32_t>(map);
            } else if (option == "--music") {
                music = std::stol(argv[i + 1]);
                if (music < 0) throw std::runtime_error("invalid --music");
            } else if (option == "--party") {
                party = parseParty(argv[i + 1]);
            } else if (option == "--load") {
                loadPath = argv[i + 1];
            } else if (option == "--save") {
                savePath = argv[i + 1];
            } else if (option == "--capture") {
                capture.path = argv[i + 1];
            } else if (option == "--moves") {
                capture.moves = argv[i + 1];
            } else if (option == "--at") {
                if (std::sscanf(argv[i + 1], "%d,%d", &capture.atX, &capture.atY) != 2 || capture.atX < 0 || capture.atY < 0) {
                    throw std::runtime_error("invalid --at, expected x,y");
                }
            } else if (option == "--wait") {
                capture.waitFrames = std::stoi(argv[i + 1]);
                if (capture.waitFrames < 0 || capture.waitFrames > FF6_CAPTURE_MAX_TALK_FRAMES) {
                    throw std::runtime_error("invalid --wait");
                }
            } else if (option == "--menu") {
                capture.menuCursor = std::stoi(argv[i + 1]);
                if (capture.menuCursor < 0 || capture.menuCursor >= FF6_MENU_ITEM_COUNT) throw std::runtime_error("invalid --menu");
            } else if (option == "--select") {
                capture.menuSelect = std::stoi(argv[i + 1]);
            } else if (option == "--down") {
                capture.menuDowns = std::stoi(argv[i + 1]);
            } else if (option == "--talk") {
                capture.talkFrames = std::stoi(argv[i + 1]);
                if (capture.talkFrames < 0 || capture.talkFrames > FF6_CAPTURE_MAX_TALK_FRAMES) {
                    throw std::runtime_error("invalid --talk");
                }
            } else {
                throw std::runtime_error("unknown option " + option);
            }
        }
        if (capture.path.empty() && (!capture.moves.empty() || capture.talkFrames >= 0 || capture.menuCursor >= 0 || capture.waitFrames > 0 || capture.atX >= 0)) {
            throw std::runtime_error("--at, --wait, --moves and --talk require --capture");
        }
        Start start;
        start.map = startMap;
        start.party = party;
        if (!loadPath.empty()) start.save = std::make_unique<ff6::SaveState>(ff6::readSave(loadPath));
        return run(argv[1], start, savePath, capture.path.empty() ? nullptr : &capture, music);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
}
