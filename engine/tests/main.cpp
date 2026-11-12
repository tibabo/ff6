#include <cstdio>
#include <cstring>

#include "check.hpp"
#include "tests.hpp"

// `ff6_tests menu` runs only the tests whose name contains the argument.
#define RUN(name) \
    if (!filter || std::strstr(#name, filter)) name()

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    RUN(testPack);
    RUN(testSave);
    RUN(testBattleData);
    RUN(testBattleGfx);
    RUN(testAttackAnim);
    RUN(testAttackAnimPlayer);
    RUN(testBattle);
    RUN(testBattleActions);
    RUN(testAudioData);
    RUN(testBrr);
    RUN(testDspVoice);
    RUN(testAkao);
    RUN(testSoundProgram);
    RUN(testMap);
    RUN(testBg);
    RUN(testColor);
    RUN(testSpriteData);
    RUN(testAnim);
    RUN(testScroll);
    RUN(testStage);
    RUN(testPlayer);
    RUN(testObj);
    RUN(testObjScript);
    RUN(testObjParty);
    RUN(testText);
    RUN(testEntrance);
    RUN(testEvent);
    RUN(testEventCmd);
    RUN(testMenu);
    RUN(testLoadMenu);
    RUN(testPartyMenu);
    RUN(testPpu);
    RUN(testCutsceneCore);
    RUN(testCutsceneScenes);
    RUN(testWorld);
    RUN(testParty);
    RUN(testGame);
    RUN(testCamping);
    RUN(testSpecialNpcs);
    if (test::failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", test::failures);
        return 1;
    }
    std::printf("All engine tests passed\n");
    return 0;
}
