#include <cstdio>

#include "check.hpp"
#include "tests.hpp"

int main() {
    testPack();
    testSave();
    testBattleData();
    testBattleGfx();
    testAttackAnim();
    testAttackAnimPlayer();
    testBattle();
    testBattleActions();
    testAudioData();
    testBrr();
    testDspVoice();
    testAkao();
    testSoundProgram();
    testMap();
    testBg();
    testColor();
    testSpriteData();
    testAnim();
    testScroll();
    testPlayer();
    testObj();
    testObjScript();
    testObjParty();
    testText();
    testEntrance();
    testEvent();
    testEventCmd();
    testMenu();
    testLoadMenu();
    testPartyMenu();
    testPpu();
    testCutsceneCore();
    testCutsceneScenes();
    testWorld();
    testParty();
    testGame();
    if (test::failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", test::failures);
        return 1;
    }
    std::printf("All engine tests passed\n");
    return 0;
}
