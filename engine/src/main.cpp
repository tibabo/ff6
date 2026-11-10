#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "audio/audio_data.hpp"
#include "audio/sound_program.hpp"
#include "cutscene/ending_card.hpp"
#include "cutscene/floating.hpp"
#include "cutscene/opening.hpp"
#include "cutscene/ruin.hpp"
#include "cutscene/the_end.hpp"
#include "cutscene/title.hpp"
#include "data/attack_anim.hpp"
#include "data/save.hpp"
#include "data/title_data.hpp"
#include "game.hpp"
#include "state/party_ops.hpp"
#include "sdl/sdl_audio.hpp"
#include "sdl/sdl_input.hpp"
#include "sdl/sdl_renderer.hpp"

#define FF6_MAX_CATCHUP_FRAMES 5  // longest stall, in frames, that the loop tries to make up for
#define FF6_CAPTURE_MAX_STEP_FRAMES 32  // bound on the ticks spent finishing one captured step
#define FF6_CAPTURE_MAX_TALK_FRAMES 100000
#define FF6_CAPTURE_MENU_SETTLE_FRAMES 20  // idle frames after a press in the menu, longer than a scroll of a page
#define FF6_CAPTURE_KEY_FRAMES 2  // idle frames after a key of --keys
#define FF6_CAPTURE_PAUSE_FRAMES 10  // frames of a . of --keys
#define FF6_MOVE_LETTERS "URDL"  // --moves letters, in Facing order
#define FF6_DEFAULT_MAP 0x1C  // the Narshe inn
#define FF6_MAX_MAP_ARGUMENT 0x1FF
#define FF6_ANIM_SHEET_COLUMNS 8  // frames per row of the sheet of --anim
#define FF6_ANIM_SHEET_BACKGROUND 60

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
    std::string keys;  // buttons pressed one after the other, for a battle: A confirm, B cancel, URDL, . a pause
    int afterFrames = 0;  // idle frames after the keys, to catch an animation in the middle
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
        for (int frame = 0; frame < FF6_CAPTURE_MAX_STEP_FRAMES && game.partyMoving(); ++frame) {
            game.tick({FF6_NO_DIRECTION, false});
        }
    }
    if (capture.talkFrames >= 0) {
        game.tick({FF6_NO_DIRECTION, true});
        for (int frame = 0; frame < capture.talkFrames; ++frame) game.tick({FF6_NO_DIRECTION, false});
    }
    game.finishFade();
    for (const char key : capture.keys) {
        ff6::InputState input;
        const char* const found = key == '\0' ? nullptr : std::strchr(letters, key);
        if (key == '.') {
            for (int frame = 0; frame < FF6_CAPTURE_PAUSE_FRAMES; ++frame) game.tick({});
            continue;
        }
        if (key == 'A') input.confirm = true;
        else if (key == 'B') input.cancel = true;
        else if (found) input.direction = static_cast<int>(found - letters);
        else throw std::runtime_error("--keys only accepts A, B, U, R, D, L and .");
        game.tick(input);
        for (int frame = 0; frame < FF6_CAPTURE_KEY_FRAMES; ++frame) game.tick({});
    }
    for (int frame = 0; frame < capture.afterFrames; ++frame) game.tick({});
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
    std::printf("Saved %s (tile %d,%d)\n", capture.path.c_str(), game.partyTileX(), game.partyTileY());
}

// Fixed-step loop: the game advances FF6_FRAMES_PER_SECOND times per second whatever the display rate.
void playGame(ff6::Game& game, ff6::SdlRenderer& renderer, const std::string& savePath) {
    ff6::SdlInput input;
    const double ticksPerFrame = static_cast<double>(SDL_GetPerformanceFrequency()) / FF6_FRAMES_PER_SECOND;
    Uint64 previous = SDL_GetPerformanceCounter();
    double accumulator = 0;
    std::printf("Arrows/WASD or controller to move, Enter/Space/A to talk (or to end the title screen),\n"
                "F5 to save to %s, F11 or Alt+Enter for fullscreen, F10 for integer scaling, Escape to quit.\n",
                savePath.c_str());
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
                if (key == SDLK_F5 && !game.cutsceneOpen() && !game.loadMenuOpen()) {
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
    int battle = -1, battleBackground = -1;  // a battle that starts at once, over the field
    int playAnimation = -1;  // an attack whose animation plays in that battle
    unsigned partyStatus = 0;  // statuses that the party has when the battle starts, to see them
    int scene = -1;  // a battle event that plays when the battle opens
    bool stock = false;  // weapons, tools and every Lore, to try the commands that need them
    bool newGame = false;  // begin like the console: the title screen, the story, the opening
    std::string cutscene;  // "title" or "opening": only that scene, for a capture
};

std::unique_ptr<ff6::Game> makeGame(ff6::CommonData common, const std::string& packDir, const Start& start,
                                    ff6::SdlRenderer& renderer, ff6::Sound& sound) {
    std::unique_ptr<ff6::Game> game = start.newGame ? std::make_unique<ff6::Game>(std::move(common), packDir, ff6::NewGame{}, renderer, sound)
                                      : start.save ? std::make_unique<ff6::Game>(std::move(common), packDir, *start.save, renderer, sound)
                                                   : std::make_unique<ff6::Game>(std::move(common), packDir, start.map, renderer, sound, start.party);
    if (start.stock) {
        for (int item = FF6_STOCK_FIRST_ITEM; item < FF6_STOCK_FIRST_ITEM + FF6_STOCK_ITEMS; ++item) ff6::giveItem(game->party(), item);
        for (int item = FF6_STOCK_FIRST_TOOL; item < FF6_STOCK_FIRST_TOOL + FF6_STOCK_TOOLS; ++item) ff6::giveItem(game->party(), item);
        game->party().lores = (1u << FF6_LORE_COUNT) - 1;
    }
    return game;
}

// Plays one scene (the title screen or the opening) without a window for `--wait` frames, then saves the screen.
void captureCutscene(const std::string& packDir, const std::string& name, const Capture& capture, ff6::SdlRenderer& renderer) {
    const ff6::TitleData data = ff6::loadTitleData(packDir);
    ff6::NoSound sound;
    std::unique_ptr<ff6::Cutscene> scene;
    // "ending" and the state of the `ending` command: that scene of the ending.
    const ff6::CommonData common = ff6::loadCommonData(packDir);
    const ff6::MenuData menuData = ff6::loadMenuData(packDir);
    ff6::MenuScreen menuScreen(common, menuData);
    if (name.rfind("ending", 0) == 0) {
        const int state = std::atoi(name.c_str() + 6);
        std::vector<std::string> names;
        for (const int id : ff6::endingCharacters(state)) names.push_back(ff6::characterName(id));
        scene = std::make_unique<ff6::EndingCard>(menuScreen, names, std::max(ff6::endingFrames(state), 1),
                                                  ff6::creditsPages(ff6::creditsScene(state), data.credits), ff6::endingCharacters(state),
                                                  ff6::makeFlightBackdrop(data, state), ff6::makeEndingNames(data, state));
    } else if (name == "title") {
        scene = std::make_unique<ff6::TitleScene>(data, sound);
    } else if (name == "opening") {
        scene = std::make_unique<ff6::OpeningScene>(data, sound);
    } else if (name == "floating") {
        scene = std::make_unique<ff6::FloatingScene>(data, sound);
    } else if (name == "end") {
        scene = std::make_unique<ff6::TheEndScene>(data, sound);
    } else if (name == "ruin") {
        scene = std::make_unique<ff6::RuinScene>(data, sound);
    } else {
        throw std::runtime_error("--cutscene takes title, opening, ruin, floating, end or endingN");
    }
    for (int frame = 0; frame < capture.waitFrames && !scene->finished(); ++frame) scene->tick({});
    std::vector<uint8_t> pixels(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * FF6_BYTES_PER_PIXEL);
    scene->draw(pixels.data());
    const ff6::TextureId texture = renderer.createTexture(FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT, nullptr, ff6::TextureUsage::Dynamic);
    const ff6::Rect screen{0, 0, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT};
    renderer.updateTexture(texture, pixels.data());
    renderer.beginFrame();
    renderer.draw(texture, screen, screen);
    renderer.endFrame();
    renderer.saveFrame(capture.path);
    std::printf("Saved %s (%s, %d frames)\n", capture.path.c_str(), name.c_str(), capture.waitFrames);
}

int run(const std::string& packDir, const Start& start, const std::string& savePath, const Capture* capture, long music) {
    ff6::CommonData common = ff6::loadCommonData(packDir);
    SdlRuntime sdl;
    ff6::SdlRenderer renderer(capture != nullptr);
    if (capture && !start.cutscene.empty()) {
        captureCutscene(packDir, start.cutscene, *capture, renderer);
        return 0;
    }
    if (capture) {  // no sound device for a capture
        ff6::NoSound sound;
        const std::unique_ptr<ff6::Game> game = makeGame(std::move(common), packDir, start, renderer, sound);
        if (capture->atX >= 0) game->placeParty(capture->atX, capture->atY);
        if (start.battle >= 0) {
            for (ff6::CharacterState& character : game->party().characters) character.status |= start.partyStatus;
            game->startBattleNumber(start.battle, start.battleBackground);
            for (int frame = 0; frame < FF6_CAPTURE_MAX_STEP_FRAMES * 2 && game->menuPhase() != ff6::MenuPhase::Open; ++frame) {
                game->tick({});
            }
            game->finishFade();
            if (start.playAnimation >= 0) game->battle().playAnimation(start.playAnimation);
            if (start.scene >= 0) game->battle().playScene(start.scene);
        }
        captureFrame(*game, renderer, *capture);
        return 0;
    }
    const ff6::AudioData audio = ff6::loadAudioData(packDir);
    if (music >= 0 && static_cast<size_t>(music) >= audio.songs.size()) throw std::runtime_error("invalid --music");
    ff6::SoundProgram program(audio);
    ff6::SdlAudio sound(program);
    const std::unique_ptr<ff6::Game> game = makeGame(std::move(common), packDir, start, renderer, sound);
    if (start.battle >= 0) {
        for (ff6::CharacterState& character : game->party().characters) character.status |= start.partyStatus;
        game->startBattleNumber(start.battle, start.battleBackground);
        if (start.playAnimation >= 0 || start.scene >= 0) {  // the battle opens first, then the animation plays at once
            for (int frame = 0; frame < FF6_CAPTURE_MAX_STEP_FRAMES * 2 && game->menuPhase() != ff6::MenuPhase::Open; ++frame) {
                game->tick({});
            }
            game->finishFade();
            if (start.playAnimation >= 0) game->battle().playAnimation(start.playAnimation);
            if (start.scene >= 0) game->battle().playScene(start.scene);
        }
    }
    if (music >= 0) sound.command(FF6_SOUND_PLAY_SONG, static_cast<uint8_t>(music), FF6_SOUND_FULL_VOLUME);  // over the song of the map
    game->setSaveHandler([&savePath](const ff6::SaveState& state) {
        try {
            ff6::writeSave(savePath, state);
            std::printf("Saved the game to %s\n", savePath.c_str());
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Could not save: %s\n", error.what());
        }
    });
    game->setLoadHandler([&savePath]() -> std::unique_ptr<ff6::SaveState> {
        if (!std::ifstream(savePath).good()) return nullptr;
        try {
            return std::make_unique<ff6::SaveState>(ff6::readSave(savePath));
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Could not load %s: %s\n", savePath.c_str(), error.what());
            return nullptr;
        }
    });
    playGame(*game, renderer, savePath);
    return 0;
}

// Draws the frames of the script of a layer of an attack (0 sprites, 1 bg1, 2 bg3) side by side, each around its center, into a bitmap.
void writeAnimSheet(const std::string& packDir, int attack, int layer, const std::string& path) {
    const ff6::AttackAnim anim = ff6::loadAttackAnim(packDir);
    const ff6::AttackAnimProp prop = anim.prop(attack);
    const int chosen = layer == 1 ? prop.bg1 : layer == 2 ? prop.bg3 : prop.sprite;
    const int palette = layer == 1 ? prop.bg1Pal : layer == 2 ? prop.bg3Pal : prop.spritePal;
    const int script = chosen & ~FF6_ATTACK_GFX_ONLY_FLAG;
    if (chosen == FF6_ATTACK_NO_SCRIPT) throw std::runtime_error("this attack has no animation in that layer");
    const ff6::AttackGfxProp gfx = anim.gfx(script);
    const int cell = std::max(gfx.width, gfx.height) * FF6_ATTACK_CELL * 2;
    const int columns = FF6_ANIM_SHEET_COLUMNS, rows = (std::max(gfx.frames, 1) + columns - 1) / columns;
    const int width = columns * cell, height = rows * cell;
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 3, FF6_ANIM_SHEET_BACKGROUND);  // RGB
    for (int index = 0; index < gfx.frames; ++index) {
        const int centerX = index % columns * cell + cell / 2, centerY = index / columns * cell + cell / 2;
        pixels[(static_cast<size_t>(centerY) * width + centerX) * 3] = 255;  // a red dot where the thread is
        const ff6::AttackFrame& frame = anim.frame(script, index, palette, false);
        for (int y = 0; y < frame.image.height; ++y) {
            for (int x = 0; x < frame.image.width; ++x) {
                const uint8_t* source = &frame.image.rgba[(static_cast<size_t>(y) * frame.image.width + x) * FF6_BYTES_PER_PIXEL];
                if (source[FF6_ALPHA_OFFSET] == 0) continue;
                const int targetX = centerX + frame.x + x, targetY = centerY + frame.y + y;
                if (targetX < 0 || targetY < 0 || targetX >= width || targetY >= height) continue;
                std::copy(source, source + 3, &pixels[(static_cast<size_t>(targetY) * width + targetX) * 3]);
            }
        }
    }
    std::ofstream out(path, std::ios::binary);
    const uint32_t bytes = static_cast<uint32_t>(pixels.size()), offset = 54;
    const auto put = [&](uint32_t value, int count) { for (int i = 0; i < count; ++i) out.put(static_cast<char>(value >> (8 * i))); };
    out.write("BM", 2);
    put(offset + bytes, 4); put(0, 4); put(offset, 4); put(40, 4);
    put(static_cast<uint32_t>(width), 4); put(static_cast<uint32_t>(-height), 4);  // top down
    put(1, 2); put(24, 2); put(0, 4); put(bytes, 4); put(0, 8); put(0, 8);
    for (size_t i = 0; i < pixels.size(); i += 3) { out.put(static_cast<char>(pixels[i + 2])); out.put(static_cast<char>(pixels[i + 1])); out.put(static_cast<char>(pixels[i])); }
    std::printf("Saved %s: %d frames of script %d, palette %d\n", path.c_str(), gfx.frames, script, palette);
}

}  // namespace

int main(int argc, char** argv) {
    const char* usage = "Usage: %s <data-pack-dir> [--map n] [--party terra,locke,...] [--battle n [--bg n]] [--play attack] [--status bits] [--scene n] [--stock 1] [--anim attack [--layer 0|1|2]] [--load save.json] [--save save.json] [--music song] [--new-game 1] [--cutscene title|opening|ruin|floating|end --capture out.bmp --wait frames] [--capture out.bmp [--at x,y] [--wait frames] [--moves URDL...] [--talk frames] [--keys ABURDL.] [--after frames]]\n";
    if (argc < 2) {
        std::fprintf(stderr, usage, argc > 0 ? argv[0] : "ff6");
        return 2;
    }
    try {
        Capture capture;
        uint32_t startMap = FF6_DEFAULT_MAP;
        long music = -1;
        std::vector<int> party;
        int battle = -1, battleBackground = -1, animAttack = -1, animLayer = 0, playAnimation = -1;
        unsigned partyStatus = 0;
        int scene = -1;
        bool stock = false;
        bool mapGiven = false;
        bool forceNewGame = false;
        std::string cutscene;
        std::string loadPath;
        std::string savePath = FF6_SAVE_DEFAULT_FILE;
        for (int i = 2; i < argc; i += 2) {
            const std::string option = argv[i];
            if (i + 1 >= argc) throw std::runtime_error("missing value for " + option);
            if (option == "--map") {
                const long map = std::stol(argv[i + 1], nullptr, 0);
                if (map < 0 || map > FF6_MAX_MAP_ARGUMENT) throw std::runtime_error("invalid --map");
                startMap = static_cast<uint32_t>(map);
                mapGiven = true;
            } else if (option == "--cutscene") {
                cutscene = argv[i + 1];
            } else if (option == "--new-game") {
                forceNewGame = std::stoi(argv[i + 1]) != 0;
            } else if (option == "--music") {
                music = std::stol(argv[i + 1]);
                if (music < 0) throw std::runtime_error("invalid --music");
            } else if (option == "--party") {
                party = parseParty(argv[i + 1]);
            } else if (option == "--battle") {
                battle = std::stoi(argv[i + 1]);
                if (battle < 0 || battle >= FF6_BATTLE_COUNT) throw std::runtime_error("invalid --battle");
            } else if (option == "--bg") {
                battleBackground = std::stoi(argv[i + 1]);
                if (battleBackground < 0 || battleBackground >= FF6_BG_COUNT) throw std::runtime_error("invalid --bg");
            } else if (option == "--scene") {
                scene = std::stoi(argv[i + 1]);
                if (scene < 0 || scene >= FF6_BATTLE_EVENT_COUNT) throw std::runtime_error("invalid --scene");
            } else if (option == "--status") {
                partyStatus = static_cast<unsigned>(std::stoul(argv[i + 1], nullptr, 0));
            } else if (option == "--play") {
                playAnimation = std::stoi(argv[i + 1]);
                if (playAnimation < 0 || playAnimation >= FF6_ATTACK_PROP_COUNT) throw std::runtime_error("invalid --play");
            } else if (option == "--stock") {
                stock = std::stoi(argv[i + 1]) != 0;
            } else if (option == "--layer") {
                animLayer = std::stoi(argv[i + 1]);
                if (animLayer < 0 || animLayer > 2) throw std::runtime_error("invalid --layer");
            } else if (option == "--anim") {
                animAttack = std::stoi(argv[i + 1]);
                if (animAttack < 0 || animAttack >= FF6_ATTACK_PROP_COUNT) throw std::runtime_error("invalid --anim");
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
            } else if (option == "--keys") {
                capture.keys = argv[i + 1];
            } else if (option == "--after") {
                capture.afterFrames = std::stoi(argv[i + 1]);
                if (capture.afterFrames < 0 || capture.afterFrames > FF6_CAPTURE_MAX_TALK_FRAMES) throw std::runtime_error("invalid --after");
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
        if (capture.path.empty() && (!capture.moves.empty() || capture.talkFrames >= 0 || capture.menuCursor >= 0 || capture.waitFrames > 0 || capture.atX >= 0 || !capture.keys.empty() || capture.afterFrames > 0)) {
            throw std::runtime_error("--at, --wait, --moves, --talk, --keys and --after require --capture");
        }
        if (!cutscene.empty() && capture.path.empty()) throw std::runtime_error("--cutscene requires --capture");
        if (animAttack >= 0) {
            if (capture.path.empty()) throw std::runtime_error("--anim requires --capture");
            writeAnimSheet(argv[1], animAttack, animLayer, capture.path);
            return 0;
        }
        Start start;
        start.map = startMap;
        start.battle = battle;
        start.battleBackground = battleBackground;
        start.playAnimation = playAnimation;
        start.partyStatus = partyStatus;
        start.scene = scene;
        start.stock = stock;
        start.party = party;
        start.cutscene = cutscene;
        // Without a map, a party, a battle or a save to start from, the game begins like the console does.
        start.newGame = forceNewGame || (capture.path.empty() && !mapGiven && party.empty() && battle < 0 && loadPath.empty() && !stock && playAnimation < 0);
        if (!loadPath.empty()) start.save = std::make_unique<ff6::SaveState>(ff6::readSave(loadPath));
        return run(argv[1], start, savePath, capture.path.empty() ? nullptr : &capture, music);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
}
