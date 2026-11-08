#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "data/pack.hpp"
#include "sound.hpp"

namespace ff6 {

// The event bits (switches) of the story, as kept at $1e80 by the game, and the battle flags at $1dc9.
class EventBits {
public:
    // Clears every bit, then sets the npc switches that are on in a new game.
    void init(const CommonData& common);
    bool get(int id) const;
    void set(int id, bool value);
    bool battle(int id) const;
    void setBattle(int id, bool value);
    // The ids of the bits that are set, and the bits of a saved game: only those are set.
    std::vector<int> setIds() const;
    std::vector<int> battleSetIds() const;
    void load(const std::vector<int>& bits, const std::vector<int>& battleFlags);

private:
    std::array<uint8_t, (FF6_EVENT_BIT_COUNT + 7) / 8> bytes_{};
    std::array<uint8_t, FF6_BATTLE_FLAG_BYTES> battle_{};
};

// What the event interpreter asks of the game.
class EventHost {
public:
    virtual ~EventHost() = default;
    virtual void openDialogue(uint32_t id, uint16_t flags) = 0;
    virtual bool dialogueBusy() const = 0;
    virtual int dialogueChoiceCount() const = 0;  // of the last dialogue
    virtual int dialogueChoice() const = 0;  // the choice the player made in it
    // `startupEvent`: the map runs its startup event once it is loaded; `fadeIn`: the screen fades in after it.
    virtual void changeMap(uint32_t map, int x, int y, int facing, bool startupEvent, bool fadeIn) = 0;
    // The object runs the script that starts at `offset` of the event script. False when it cannot.
    virtual bool startObjectScript(int object, uint32_t offset) = 0;
    virtual bool objectBusy(int object) const = 0;  // it runs a script or walks
    // Event commands 0x36 to 0x7D that change an object; `arg` is its graphics, palette or event offset.
    virtual void objectCommand(uint8_t op, int object, uint32_t arg) = 0;
    // Event commands of the party, the screen and the sprite palettes (see FF6_EVENT_LOCK_SCREEN to FF6_EVENT_MOVE_PARTY);
    // `bytes` are the command bytes after the opcode.
    virtual void fieldCommand(uint8_t op, const uint8_t* bytes, size_t size) = 0;
    virtual Sound& sound() = 0;
    virtual bool fading() const = 0;  // the screen fades in or out
};

struct EventTrigger {
    int x = 0, y = 0;
    uint32_t offset = 0;  // into the event script
};

EventTrigger decodeEventTrigger(const uint8_t* bytes);

// Port of ExecEvent for the commands that matter to dialogue, objects and branching: dialogue, choices, switches,
// conditional jumps, loops, calls, pauses, object scripts and map changes. Commands that only skip a known number of
// bytes do nothing; any other command ends the event.
class EventRunner {
public:
    EventRunner(const CommonData& common, EventBits& bits) : common_(common), bits_(bits) {}
    void start(uint32_t offset);
    // Runs the event at `offset` before the next command, as a subroutine of the running event.
    void call(uint32_t offset);
    void stop();
    bool running() const { return running_; }
    // Call once per game frame.
    void update(EventHost& host);

private:
    // One level of the event stack: where a call returns to, where a loop starts and how many rounds are left.
    struct Frame {
        uint32_t returnPc;
        uint32_t loopStart;
        int count;
    };

    bool step(EventHost& host);  // false when the event must wait for the next frame
    bool pushFrame(uint32_t returnPc, uint32_t loopStart, int count);
    void endRound();  // the loop or subroutine of the top frame ran once
    uint32_t addressAt(size_t at) const;
    bool conditionMet(uint8_t op, bool* ok) const;
    bool coinFlip();

    const CommonData& common_;
    EventBits& bits_;
    bool running_ = false;
    uint32_t pc_ = 0;
    std::vector<Frame> stack_;
    int pause_ = 0;  // frames that the event still waits
    bool waitDialogue_ = true;  // the event waits for the dialogue that is open
    int waitObject_ = -1;  // the object whose script the event waits for
    int waited_ = 0;  // frames spent waiting for it
    int soundWaited_ = 0;  // frames spent waiting for the sound program
    bool mapFadeStarted_ = false;  // the screen fades out before a map is loaded
    uint32_t seed_ = 0x9E3779B9;
};

}  // namespace ff6
