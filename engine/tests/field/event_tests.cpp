#include <vector>

#include "../check.hpp"
#include "../test_helpers.hpp"
#include "../tests.hpp"
#include "field/event.hpp"

namespace {

// Plays an event to the end, closing every dialogue at once.
std::vector<uint32_t> play(const std::vector<uint8_t>& script, test::FakeHost& host, ff6::EventBits& bits, uint32_t start = 0) {
    const ff6::CommonData common = test::makeCommon(script);
    ff6::EventRunner runner(common, bits);
    runner.start(start);
    for (int i = 0; i < 50 && runner.running(); ++i) {
        runner.update(host);
        host.busy = false;
    }
    CHECK(!runner.running());
    return host.opened;
}

ff6::EventBits freshBits() {
    ff6::EventBits bits;
    bits.init(test::makeCommon());
    return bits;
}

}  // namespace

void testEvent() {
    {  // a dialogue, then the end of the event; waiting for the dialogue to close
        const ff6::CommonData common = test::makeCommon({0x4B, 0x0D, 0x06, 0x4B, 0x01, 0x00, 0xFE});
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        ff6::EventRunner runner(common, bits);
        runner.start(0);
        runner.update(host);
        CHECK(host.opened == std::vector<uint32_t>({0x060D}) && runner.running());
        runner.update(host);  // the dialogue is still open: nothing happens
        CHECK(host.opened.size() == 1);
        host.busy = false;
        runner.update(host);
        CHECK(host.opened.size() == 2 && host.opened[1] == 1);
        host.busy = false;
        runner.update(host);
        CHECK(!runner.running());
    }
    {  // switches and conditional jumps: set switch 5, then jump if it is set
        const std::vector<uint8_t> script = {0xD0, 0x05, 0xC0, 0x05, 0x80, 0x0F, 0x00, 0x00, 0x4B, 0x01, 0x00, 0xFE, 0xFF, 0xFF, 0xFF,
                                             0x4B, 0x02, 0x00, 0xFE};
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        CHECK(play(script, host, bits) == std::vector<uint32_t>({2}));
        CHECK(bits.get(5));
        // The same condition with the switch clear falls through.
        const std::vector<uint8_t> clear = {0xD1, 0x05, 0xC0, 0x05, 0x80, 0x0F, 0x00, 0x00, 0x4B, 0x01, 0x00, 0xFE, 0xFF, 0xFF, 0xFF,
                                            0x4B, 0x02, 0x00, 0xFE};
        test::FakeHost other;
        bits.set(5, true);
        CHECK(play(clear, other, bits) == std::vector<uint32_t>({1}));
        CHECK(!bits.get(5));
        // Switches above 0xFF use the later opcodes, and "if all" needs every switch to match.
        const std::vector<uint8_t> all = {0xD2, 0x01, 0xC9, 0x01, 0x81, 0x05, 0x80, 0x12, 0x00, 0x00, 0x4B, 0x07, 0x00, 0xFE,
                                          0xFF, 0xFF, 0xFF, 0xFF, 0x4B, 0x08, 0x00, 0xFE};
        test::FakeHost third;
        ff6::EventBits fresh = freshBits();
        CHECK(play(all, third, fresh) == std::vector<uint32_t>({7}));  // 0x101 is set, 5 is not: not all, so it falls through
        CHECK(fresh.get(0x101));
    }
    {  // a choice jumps to the address of the selected entry
        const std::vector<uint8_t> script = {0xB6, 0x0A, 0x00, 0x00, 0x0E, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
                                             0x4B, 0x10, 0x00, 0xFE, 0x4B, 0x20, 0x00, 0xFE};
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        host.count = 2;
        host.choice = 1;
        CHECK(play(script, host, bits) == std::vector<uint32_t>({0x20}));
        test::FakeHost none;  // a choice without a dialogue with choices ends the event
        CHECK(play(script, none, bits).empty());
    }
    {  // calls return to the command after them
        const std::vector<uint8_t> script = {0xB2, 0x08, 0x00, 0x00, 0x4B, 0x03, 0x00, 0xFE, 0x4B, 0x04, 0x00, 0xFE};
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        CHECK(play(script, host, bits) == std::vector<uint32_t>({4, 3}));
    }
    {  // object scripts and known commands are skipped
        const std::vector<uint8_t> script = {0x00, 0x82, 0xAA, 0xBB, 0x37, 0x01, 0x02, 0x4B, 0x08, 0x00, 0xFE};
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        CHECK(play(script, host, bits) == std::vector<uint32_t>({8}));
    }
    {  // unknown commands end the event instead of guessing their size
        const std::vector<uint8_t> script = {0x66, 0x00, 0x00, 0x4B, 0x01, 0x00, 0xFE};
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        CHECK(play(script, host, bits).empty());
    }
    {  // changing the map tells the game where to go, and the event goes on after it
        const std::vector<uint8_t> script = {0x6A, 0x20, 0x20, 0x29, 0x26, 0x80, 0x4B, 0x01, 0x00, 0xFE};
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        CHECK(play(script, host, bits) == std::vector<uint32_t>({1}));
        CHECK(host.mapChanges.size() == 1);
        CHECK(host.mapChanges[0].map == 0x20 && host.mapChanges[0].x == 0x29 && host.mapChanges[0].y == 0x26);
        CHECK(host.mapChanges[0].facing == ff6::Down && host.mapChanges[0].startupEvent && host.mapChanges[0].fadeIn);
        CHECK(host.fieldCommands.size() == 1 && host.fieldCommands[0].op == 0x97);  // the screen fades out first
        // The startup event only runs when the command asks for it, and the flag 0x40 keeps the screen dark.
        test::FakeHost quiet;
        play({0x6B, 0x20, 0x20, 0x29, 0x26, 0x40}, quiet, bits);
        CHECK(quiet.mapChanges.size() == 1 && !quiet.mapChanges[0].startupEvent && !quiet.mapChanges[0].fadeIn);
        CHECK(quiet.fieldCommands.empty());  // and this command does not fade out
        // The first command waits for the fade out to be over.
        const ff6::CommonData common = test::makeCommon(script);
        ff6::EventRunner runner(common, bits);
        test::FakeHost fading;
        fading.fadeBusy = true;
        runner.start(0);
        for (int i = 0; i < 5; ++i) runner.update(fading);
        CHECK(fading.mapChanges.empty());
        fading.fadeBusy = false;
        runner.update(fading);
        CHECK(fading.mapChanges.size() == 1);
    }
    {  // the fades are handed to the game, and the event can wait for them
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        play({0x59, 0x08, 0x5A, 0x04, 0x5B, 0x96, 0x97, 0xFE}, host, bits);
        CHECK(host.fieldCommands.size() == 5);
        CHECK(host.fieldCommands[0].op == 0x59 && host.fieldCommands[0].bytes == std::vector<uint8_t>({0x08}));
        CHECK(host.fieldCommands[1].op == 0x5A && host.fieldCommands[1].bytes == std::vector<uint8_t>({0x04}));
        CHECK(host.fieldCommands[2].op == 0x5B && host.fieldCommands[3].op == 0x96 && host.fieldCommands[4].op == 0x97);
        const ff6::CommonData common = test::makeCommon({0x5C, 0x4B, 0x01, 0x00, 0xFE});
        ff6::EventRunner runner(common, bits);
        test::FakeHost waiting;
        waiting.fadeBusy = true;
        runner.start(0);
        for (int i = 0; i < 5; ++i) runner.update(waiting);
        CHECK(waiting.opened.empty());
        waiting.fadeBusy = false;
        runner.update(waiting);
        CHECK(waiting.opened.size() == 1);
    }
    {  // truncated scripts and bad offsets end the event
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        CHECK(play({0x4B, 0x01}, host, bits).empty());
        CHECK(play({0xC0, 0x01}, host, bits).empty());
        CHECK(play({0xFE}, host, bits, 100).empty());
    }
    {  // pauses: the event goes on after as many frames as they last
        const auto framesUntilDialogue = [](const std::vector<uint8_t>& script) {
            const ff6::CommonData common = test::makeCommon(script);
            ff6::EventBits bits = freshBits();
            ff6::EventRunner runner(common, bits);
            test::FakeHost host;
            runner.start(0);
            for (int frame = 1; frame <= 400; ++frame) {
                runner.update(host);
                if (!host.opened.empty()) return frame;
            }
            return -1;
        };
        CHECK(framesUntilDialogue({0x92, 0x4B, 0x01, 0x00, 0xFE}) == 31);  // 30 frames
        CHECK(framesUntilDialogue({0x91, 0x4B, 0x01, 0x00, 0xFE}) == 16);
        CHECK(framesUntilDialogue({0x95, 0x4B, 0x01, 0x00, 0xFE}) == 121);
        CHECK(framesUntilDialogue({0xB4, 0x05, 0x4B, 0x01, 0x00, 0xFE}) == 6);
        CHECK(framesUntilDialogue({0xB5, 0x02, 0x4B, 0x01, 0x00, 0xFE}) == 31);  // 2 times 15 frames
        CHECK(framesUntilDialogue({0x4B, 0x01, 0x00, 0xFE}) == 1);
    }
    {  // loops run their body as many times as asked; a subroutine called several times as well
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        CHECK(play({0xB0, 0x03, 0x4B, 0x07, 0x00, 0xB1, 0xFE}, host, bits) == std::vector<uint32_t>({7, 7, 7}));
        test::FakeHost calls;
        const std::vector<uint8_t> script = {0xB3, 0x02, 0x09, 0x00, 0x00, 0x4B, 0x09, 0x00, 0xFE,
                                             0x4B, 0x05, 0x00, 0xFE};
        CHECK(play(script, calls, bits) == std::vector<uint32_t>({5, 5, 9}));
    }
    {  // a loop that waits for an event bit
        const std::vector<uint8_t> script = {0xB0, 0xFF, 0xBC, 0x05, 0x00, 0x4B, 0x01, 0x00, 0xFE};
        const ff6::CommonData common = test::makeCommon(script);
        ff6::EventBits bits = freshBits();
        ff6::EventRunner runner(common, bits);
        test::FakeHost host;
        runner.start(0);
        for (int i = 0; i < 3; ++i) runner.update(host);
        CHECK(host.opened.empty() && runner.running());
        bits.set(5, true);
        runner.update(host);
        CHECK(host.opened == std::vector<uint32_t>({1}));
    }
    {  // battle flags: the jump is taken while the flag is clear
        const std::vector<uint8_t> script = {0xB7, 0x03, 0x0A, 0x00, 0x00, 0x4B, 0x01, 0x00, 0xFE, 0xFF,
                                             0x4B, 0x02, 0x00, 0xFE};
        ff6::EventBits bits = freshBits();
        test::FakeHost clear;
        CHECK(play(script, clear, bits) == std::vector<uint32_t>({2}));
        bits.setBattle(3, true);
        test::FakeHost set;
        CHECK(play(script, set, bits) == std::vector<uint32_t>({1}));
        const std::vector<uint8_t> toggle = {0xB8, 0x04, 0xB9, 0x05, 0xFE};
        bits.setBattle(5, true);
        test::FakeHost other;
        play(toggle, other, bits);
        CHECK(bits.battle(4) && !bits.battle(5) && !bits.battle(-1) && !bits.battle(256));
    }
    {  // object scripts: the object runs the script that follows the command, and the event may wait for it
        const std::vector<uint8_t> script = {0x10, 0x82, 0xAA, 0xBB, 0x4B, 0x08, 0x00, 0xFE};
        const ff6::CommonData common = test::makeCommon(script);
        ff6::EventBits bits = freshBits();
        ff6::EventRunner runner(common, bits);
        test::FakeHost host;
        host.supportsScripts = true;
        host.busyObjects = {0x10};
        runner.start(0);
        runner.update(host);
        runner.update(host);
        CHECK(host.scripts.size() == 1 && host.scripts[0].object == 0x10 && host.scripts[0].offset == 2);
        CHECK(host.opened.empty());  // waiting for the script to end
        host.busyObjects.clear();
        runner.update(host);
        CHECK(host.opened == std::vector<uint32_t>({8}));
        // Without the wait flag the event goes on at once, and an object that cannot run scripts is not waited for.
        const ff6::CommonData async = test::makeCommon({0x10, 0x02, 0xAA, 0xBB, 0x4B, 0x08, 0x00, 0xFE});
        ff6::EventRunner asyncRunner(async, bits);
        test::FakeHost busy;
        busy.supportsScripts = true;
        busy.busyObjects = {0x10};
        asyncRunner.start(0);
        asyncRunner.update(busy);
        CHECK(busy.opened.size() == 1);
        const ff6::CommonData unsupported = test::makeCommon({0x10, 0x82, 0xAA, 0xBB, 0x4B, 0x08, 0x00, 0xFE});
        ff6::EventRunner unsupportedRunner(unsupported, bits);
        test::FakeHost cannot;
        cannot.busyObjects = {0x10};
        unsupportedRunner.start(0);
        unsupportedRunner.update(cannot);
        CHECK(cannot.opened.size() == 1);
    }
    {  // waiting for an object, also when it was started earlier; the wait ends after a long time
        const ff6::CommonData common = test::makeCommon({0x35, 0x11, 0x4B, 0x03, 0x00, 0xFE});
        ff6::EventBits bits = freshBits();
        ff6::EventRunner runner(common, bits);
        test::FakeHost host;
        host.busyObjects = {0x11};
        runner.start(0);
        for (int i = 0; i < 100; ++i) runner.update(host);
        CHECK(host.opened.empty());
        host.busyObjects.clear();
        runner.update(host);
        CHECK(host.opened.size() == 1);
        ff6::EventRunner stuck(common, bits);
        test::FakeHost never;
        never.busyObjects = {0x11};
        stuck.start(0);
        for (int i = 0; i < FF6_EVENT_OBJECT_WAIT_LIMIT + 5; ++i) stuck.update(never);
        CHECK(never.opened.size() == 1);
    }
    {  // commands that change an object are handed to the game
        const std::vector<uint8_t> script = {0x37, 0x12, 0x09, 0x43, 0x12, 0x03, 0x7A, 0x12, 0x34, 0x12, 0x01, 0x3E, 0x12,
                                             0x41, 0x13, 0xFE};
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        play(script, host, bits);
        CHECK(host.objectCommands.size() == 5);
        CHECK(host.objectCommands[0].op == 0x37 && host.objectCommands[0].object == 0x12 && host.objectCommands[0].arg == 9);
        CHECK(host.objectCommands[1].op == 0x43 && host.objectCommands[1].arg == 3);
        CHECK(host.objectCommands[2].op == 0x7A && host.objectCommands[2].arg == 0x11234);
        CHECK(host.objectCommands[3].op == 0x3E && host.objectCommands[3].object == 0x12);
        CHECK(host.objectCommands[4].op == 0x41 && host.objectCommands[4].object == 0x13);
    }
    {  // the commands of the party, the screen and the palettes are handed to the game with their bytes
        const std::vector<uint8_t> script = {0x38, 0x3C, 0x00, 0x02, 0xFF, 0xFF, 0x3F, 0x05, 0x02, 0x60, 0x03, 0x07,
                                             0x79, 0x01, 0x34, 0x02, 0x7E, 0x0A, 0x0B, 0x3A, 0x47, 0x46, 0x03, 0xFE};
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        play(script, host, bits);
        CHECK(host.fieldCommands.size() == 9);
        CHECK(host.fieldCommands[0].op == 0x38 && host.fieldCommands[0].bytes.empty());
        CHECK(host.fieldCommands[1].op == 0x3C && host.fieldCommands[1].bytes == std::vector<uint8_t>({0, 2, 0xFF, 0xFF}));
        CHECK(host.fieldCommands[2].op == 0x3F && host.fieldCommands[2].bytes == std::vector<uint8_t>({5, 2}));
        CHECK(host.fieldCommands[3].op == 0x60 && host.fieldCommands[3].bytes == std::vector<uint8_t>({3, 7}));
        CHECK(host.fieldCommands[4].op == 0x79 && host.fieldCommands[4].bytes == std::vector<uint8_t>({1, 0x34, 2}));
        CHECK(host.fieldCommands[5].op == 0x7E && host.fieldCommands[5].bytes == std::vector<uint8_t>({0x0A, 0x0B}));
        CHECK(host.fieldCommands[8].op == 0x46 && host.fieldCommands[8].bytes == std::vector<uint8_t>({3}));
    }
    {  // the npc switches that are on in a new game are copied into the event bits
        ff6::CommonData common = test::makeCommon();
        common.npcSwitchDefaults[0] = 0x03;
        common.npcSwitchDefaults[1] = 0x80;
        ff6::EventBits bits;
        bits.init(common);
        CHECK(bits.get(FF6_NPC_SWITCH_EVENT_BIT) && bits.get(FF6_NPC_SWITCH_EVENT_BIT + 1));
        CHECK(!bits.get(FF6_NPC_SWITCH_EVENT_BIT + 2) && bits.get(FF6_NPC_SWITCH_EVENT_BIT + 15));
        CHECK(!bits.get(5) && !bits.get(-1) && !bits.get(FF6_EVENT_BIT_COUNT));
    }
    {  // the song commands become commands of the sound program
        const auto sent = [](const std::vector<uint8_t>& script) {
            ff6::EventBits bits = freshBits();
            test::FakeHost host;
            host.recording.previous = 7;
            play(script, host, bits);
            return host.recording.commands;
        };
        using Commands = std::vector<test::RecordingSound::Command>;
        const auto same = [](const Commands& a, const Commands& b) {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i) {
                if (a[i].command != b[i].command || a[i].arg1 != b[i].arg1 || a[i].arg2 != b[i].arg2) return false;
            }
            return true;
        };
        CHECK(same(sent({0xF0, 0x17, 0xFE}), {{0x10, 0x17, 0xFF}}));
        CHECK(same(sent({0xF0, 0x97, 0xFE}), {{0x14, 0x17, 0xFF}}));  // bit 7: the song that plays is kept
        CHECK(same(sent({0xEF, 0x05, 0x80, 0xFE}), {{0x10, 0x05, 0x80}}));
        CHECK(same(sent({0xF1, 0x05, 0x30, 0xFE}), {{0x10, 0x05, 0x20}, {0x81, 0x30, 0xFF}}));
        CHECK(same(sent({0xF2, 0x20, 0xFE}), {{0x81, 0x20, 0x00}}));
        CHECK(same(sent({0xF3, 0x10, 0xFE}), {{0x10, 0x07, 0x00}, {0x81, 0x10, 0xFF}}));  // the song that was stopped
        CHECK(same(sent({0xF6, 0xFC, 0x03, 0x00, 0xF7, 0xFE}), {{0xFC, 0x03, 0x00}, {0x89, 0x00, 0x00}}));
        CHECK(same(sent({0xF4, 0x05, 0xFE}), {{0x18, 0x05, 0x80}}));  // a sound effect, centered
        CHECK(same(sent({0xF5, 0x06, 0x20, 0x07, 0xFB, 0xFE}), {{0x18, 0x06, 0x80}, {0x83, 0x20, 0x07}}));
    }
    {  // the waits for the sound program look at its ports
        const auto framesUntilDialogue = [](const std::vector<uint8_t>& script, test::RecordingSound& sound) {
            const ff6::CommonData common = test::makeCommon(script);
            ff6::EventBits bits = freshBits();
            ff6::EventRunner runner(common, bits);
            test::FakeHost host;
            host.recording = sound;
            runner.start(0);
            for (int frame = 1; frame <= FF6_EVENT_SOUND_WAIT_LIMIT + 5; ++frame) {
                runner.update(host);
                if (!host.opened.empty()) return frame;
            }
            return -1;
        };
        test::RecordingSound idle, playing, waiting, position;
        playing.ports[3] = 0x01;
        waiting.ports[2] = 0x01;
        position.ports[1] = 0x04;
        CHECK(framesUntilDialogue({0xFA, 0x4B, 0x01, 0x00, 0xFE}, idle) == 1);
        CHECK(framesUntilDialogue({0xF8, 0x4B, 0x01, 0x00, 0xFE}, idle) == 1);
        CHECK(framesUntilDialogue({0xF9, 0x04, 0x4B, 0x01, 0x00, 0xFE}, position) == 1);
        // A wait that never ends is given up after a minute.
        CHECK(framesUntilDialogue({0xFA, 0x4B, 0x01, 0x00, 0xFE}, playing) == FF6_EVENT_SOUND_WAIT_LIMIT + 1);
        CHECK(framesUntilDialogue({0xF8, 0x4B, 0x01, 0x00, 0xFE}, waiting) == FF6_EVENT_SOUND_WAIT_LIMIT + 1);
        CHECK(framesUntilDialogue({0xF9, 0x05, 0x4B, 0x01, 0x00, 0xFE}, position) == FF6_EVENT_SOUND_WAIT_LIMIT + 1);
    }
    {  // changing the data of the map is handed to the game with the tiles
        ff6::EventBits bits = freshBits();
        test::FakeHost host;
        play({0x73, 0x04, 0x24, 0x01, 0x02, 0x04, 0x14, 0x75, 0xFE}, host, bits);
        CHECK(host.fieldCommands.size() == 1 && host.fieldCommands[0].op == 0x73);
        CHECK(host.fieldCommands[0].bytes == std::vector<uint8_t>({0x04, 0x24, 0x01, 0x02, 0x04, 0x14}));
    }
    const uint8_t trigger[FF6_EVENT_TRIGGER_SIZE] = {8, 46, 0xFF, 0x38, 0x02};
    const ff6::EventTrigger decoded = ff6::decodeEventTrigger(trigger);
    CHECK(decoded.x == 8 && decoded.y == 46 && decoded.offset == 0x238FF);
}
