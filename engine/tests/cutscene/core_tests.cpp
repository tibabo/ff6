#include <vector>

#include "../check.hpp"
#include "../tests.hpp"
#include "cutscene/core.hpp"
#include "../test_helpers.hpp"

namespace {

// A cutscene program that is all zeros but for what the tests put in it, and tables that count.
ff6::TitleData makeData() {
    ff6::TitleData data;
    data.program.assign(FF6_TITLE_PROGRAM_BYTES, 0);
    data.gfx.assign(FF6_TITLE_GFX_BYTES, 0);
    for (auto& gfx : data.mapGfx) gfx.assign(FF6_TITLE_MAP_GFX_BYTES, 0);
    for (int i = 0; i < FF6_TITLE_TABLE_BYTES; ++i) {
        data.random.push_back(static_cast<uint8_t>(i * 3));
        data.sine.push_back(static_cast<uint8_t>(i));
    }
    return data;
}

void put(ff6::TitleData& data, uint32_t address, const std::vector<uint8_t>& bytes) {
    std::copy(bytes.begin(), bytes.end(), data.program.begin() + (address - FF6_TITLE_PROGRAM_BASE));
}

// A thread that notes when it runs, and what it draws.
struct NoteTask : ff6::CutsceneTask {
    NoteTask(std::vector<int>& order, int id, bool keep = true) : order(order), id(id), keep(keep) {}
    bool run(ff6::CutsceneCore& core) override {
        order.push_back(id);
        core.ppu.sprites.push_back({id, 0, 0, 0, false});
        return keep;
    }
    std::vector<int>& order;
    int id;
    bool keep;
};

}  // namespace

void testCutsceneCore() {
    test::RecordingSound sound;
    {  // the table of frames of an animation: each pair is shown for its duration, 0xFE stays, 0xFF starts again
        ff6::TitleData data = makeData();
        put(data, 0x5100, {0x00, 0x52, 2, 0x10, 0x52, 1, 0x00, 0x52, 0xFE});
        put(data, 0x5110, {0x00, 0x52, 1, 0x10, 0x52, 0xFF});
        ff6::CutsceneCore core(data, sound);
        ff6::AnimState anim;
        core.initAnim(anim, 0x5100);
        std::vector<int> indexes;
        for (int i = 0; i < 6; ++i) {
            core.updateAnim(anim);
            indexes.push_back(anim.index);
        }
        CHECK(indexes == std::vector<int>({0, 0, 3, 6, 6, 6}));
        core.initAnim(anim, 0x5110);
        indexes.clear();
        for (int i = 0; i < 4; ++i) {
            core.updateAnim(anim);
            indexes.push_back(anim.index);
        }
        CHECK(indexes == std::vector<int>({0, 0, 0, 0}));  // the second pair sends it back to the first
    }
    {  // the sprites of a frame: the position of the thread is added, the large flag and the name bit are read
        ff6::TitleData data = makeData();
        put(data, 0x5100, {0x00, 0x52, 0xFE});
        put(data, 0x5200, {2, 0x90, 0x08, 0x40, 0x31, 0x05, 0x00, 0x02, 0x72});
        ff6::CutsceneCore core(data, sound);
        ff6::AnimState anim;
        core.initAnim(anim, 0x5100);
        core.drawAnim(anim, 0xF8, 0xFA);
        CHECK(core.ppu.sprites.size() == 2);
        const ff6::PpuSprite& first = core.ppu.sprites[0];
        CHECK(first.large && first.x == 0x08 && first.y == 0x02 && first.tile == 0x140 && first.attr == 0x31);
        const ff6::PpuSprite& second = core.ppu.sprites[1];
        CHECK(!second.large && second.x == 0xFD && second.y == 0xFA && second.tile == 2 && second.attr == 0x72);
    }
    {  // the threads run by slot, and the group is the first slot that a thread can take
        ff6::TitleData data = makeData();
        ff6::CutsceneCore core(data, sound);
        std::vector<int> order;
        core.createTask<NoteTask>(2, order, 1);
        core.createTask<NoteTask>(0, order, 2);
        core.createTask<NoteTask>(2, order, 3, false);  // ends after its first run
        core.createTask<NoteTask>(0, order, 4);
        CHECK(core.taskCount() == 4);
        core.execTasks();
        CHECK(order == std::vector<int>({2, 4, 1, 3}));
        CHECK(core.taskCount() == 3);
        CHECK(core.ppu.sprites.size() == 4 && core.ppu.sprites[0].x == 2);  // the sprites are in the order of the threads
        order.clear();
        core.execTasks();
        CHECK(order == std::vector<int>({2, 4, 1}) && core.ppu.sprites.size() == 3);
        core.resetTasks();
        CHECK(core.taskCount() == 0);
        // A group that is full goes on in the next one.
        for (int i = 0; i < 18; ++i) core.createTask<NoteTask>(0, order, i);
        order.clear();
        core.execTasks();
        CHECK(order.size() == 18 && order[17] == 17);
    }
    {  // the palette fades by steps: a color goes down at once and goes up when it is far enough, and it gets there
        ff6::TitleData data = makeData();
        put(data, 0x7000, {0x1F, 0x00, 0x10, 0x00, 0xE0, 0x03, 0x00, 0x7C});  // red 31, red 16, green 31, blue 31
        ff6::CutsceneCore core(data, sound);
        constexpr size_t kFirst = 0x30;  // the palette at $3060
        core.palette[kFirst] = 0x7FFF;  // white, which goes down to the target at once
        core.createFadePal(1, 0x3060, 0x7000);
        core.execTasks();  // the first frame only starts the fade
        CHECK(core.palette[kFirst] == 0x7FFF && core.taskCount() == 1);
        core.execTasks();
        CHECK(core.palette[kFirst] == (31 | 30 << 5 | 30 << 10));  // green and blue go down by one
        // The colors go up when they are as far from the target as the step says: the one that is 31 away moves, 16 away does not.
        CHECK(core.palette[kFirst + 1] == 0 && core.palette[kFirst + 2] == 0x0020);
        for (int frame = 0; frame < 40; ++frame) core.execTasks();
        CHECK(core.taskCount() == 0);
        CHECK(core.palette[kFirst] == 0x001F && core.palette[kFirst + 1] == 0x0010);
        CHECK(core.palette[kFirst + 2] == 0x03E0 && core.palette[kFirst + 3] == 0x7C00);
        // A slower fade makes a step every few frames: 31 steps, the first one on the second frame.
        core.palette[kFirst] = 0;
        core.createFadePal(4, 0x3060, 0x7000);
        for (int frame = 0; frame < 1 + 4 * 30; ++frame) core.execTasks();
        CHECK(core.taskCount() == 1 && core.palette[kFirst] != 0x001F);
        core.execTasks();
        CHECK(core.taskCount() == 0 && core.palette[kFirst] == 0x001F);
    }
    {  // the fade out brings the brightness down while the timer counts, then blanks the screen
        ff6::TitleData data = makeData();
        ff6::CutsceneCore core(data, sound);
        core.timer = 15;
        core.createTask<ff6::FadeOutTask>(0);
        std::vector<int> levels;
        for (int frame = 0; frame < 16; ++frame) {
            core.execTasks();
            levels.push_back(core.brightness);
            core.vblank();
        }
        CHECK(levels.front() == 15 && levels[1] == 14 && levels[14] == 1 && levels[15] == 0x80);
        CHECK(core.ppu.forceBlank && core.taskCount() == 0);
    }
    {  // the end of a frame: scroll to the lines, the palette, the copy to the video memory, the counters
        ff6::TitleData data = makeData();
        ff6::CutsceneCore core(data, sound);
        core.scroll = {1, 2, 3, 4, 5, 6};
        core.palette[7] = 0x1234;
        core.timer = 2;
        core.brightness = 9;
        const uint8_t block[4] = {0x34, 0x12, 0x78, 0x56};
        core.dmaDestination = 0x0100;
        core.dmaSource = block;
        core.dmaBytes = 4;
        core.hdma = [](ff6::Ppu& ppu) { ppu.lines[3].vofs[1] = 99; };
        core.vblank();
        CHECK(core.ppu.lines[0].hofs[0] == 1 && core.ppu.lines[0].vofs[0] == 2 && core.ppu.lines[200].hofs[2] == 5);
        CHECK(core.ppu.lines[3].vofs[1] == 99 && core.ppu.lines[4].vofs[1] == 4);  // the tables come after the registers
        CHECK(core.ppu.cgram[7] == 0x1234 && core.ppu.vram[0x100] == 0x1234 && core.ppu.vram[0x101] == 0x5678);
        CHECK(core.timer == 1 && core.frame == 1 && core.ppu.brightness == 9 && !core.ppu.forceBlank);
        core.vblank();
        core.vblank();
        CHECK(core.timer == 0 && core.frame == 3);
    }
    {  // random numbers: the table, indexed by the frame and a counter; a carry when the index goes past 255
        ff6::TitleData data = makeData();
        ff6::CutsceneCore core(data, sound);
        CHECK(core.rand() == data.random[1] && !core.randCarry());  // the counter is 1, the frame 0
        CHECK(core.rand() == data.random[2]);
        core.frame = 254;
        CHECK(core.rand() == data.random[(254 + 3) & 255] && core.randCarry());
    }
    {  // palettes are loaded from the program, 16 colors at the address in the copy of the palette
        ff6::TitleData data = makeData();
        std::vector<uint8_t> colors;
        for (int i = 0; i < 16; ++i) colors.insert(colors.end(), {static_cast<uint8_t>(i), 0x01});
        put(data, 0x6000, colors);
        ff6::CutsceneCore core(data, sound);
        core.loadPal(0x30E0, 0x6000);
        CHECK(core.palette[0x70] == 0x0100 && core.palette[0x7F] == 0x010F && core.palette[0x80] == 0);
        // The lights of the town blink: one palette on even frames, the other on odd ones.
        put(data, ff6::kTownPal1, {0x11, 0x00});
        put(data, ff6::kTownPal2, {0x22, 0x00});
        core.createTask<ff6::TownLightsTask>(0);
        core.execTasks();
        CHECK(core.palette[0x70] == 0x11);
        core.vblank();
        core.execTasks();
        CHECK(core.palette[0x70] == 0x22);
    }
}
