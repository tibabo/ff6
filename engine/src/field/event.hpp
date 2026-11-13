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

    // The event variables (words), the mask of characters or of a comparison that 0xBE chooses by, and the timers.
    int variable(int id) const { return variables_[static_cast<size_t>(id) % variables_.size()]; }
    void setVariable(int id, int value) { variables_[static_cast<size_t>(id) % variables_.size()] = static_cast<uint16_t>(value); }
    // $1f80: the last song that an event asked for, which the map plays again when the event bit FF6_EVENT_BIT_SONG_OVERRIDE is set.
    int eventSong() const { return eventSong_; }
    void setEventSong(int song) { eventSong_ = song; }
    unsigned characterMask() const { return mask_; }
    void setCharacterMask(unsigned mask) { mask_ = static_cast<uint16_t>(mask); }
    // A timer counts frames down; when it is at 0 its event is due, once.
    void setTimer(int number, int counter, uint32_t pointer, int flags);
    void resetTimer(int number);
    void tickTimers(bool menuOrBattle);  // the timers that pause in a menu or a battle stay as they are then
    bool takeTimerEvent(uint32_t* offset);
    // The frames left on the timer that is shown on the field, or -1 when there is none.
    int shownTimer() const;
    // For a saved game: every variable, and the flags, counter and event of each timer; empty when all are 0.
    std::vector<int> variableValues() const;
    std::vector<int> timerValues() const;
    void loadVariables(const std::vector<int>& variables, const std::vector<int>& timers);

private:
    struct Timer {
        int flags = 0, counter = 0;
        uint32_t pointer = 0;  // into the event script, 0 for none
    };
    std::array<uint16_t, FF6_EVENT_VAR_COUNT> variables_{};
    int eventSong_ = 0;
    uint16_t mask_ = 0;
    std::array<Timer, FF6_EVENT_TIMER_COUNT> timers_{};
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
    // The vehicle that the party is on when the map that changeMap loads is a world map (FF6_EVENT_MAP_AIRSHIP, ...).
    virtual void setVehicle(int /*vehicle*/) {}
    // The object runs the script that starts at `offset` of the event script. False when it cannot.
    virtual bool startObjectScript(int object, uint32_t offset) = 0;
    virtual bool objectBusy(int object) const = 0;  // it runs a script or walks
    // Event commands 0x36 to 0x7D that change an object; `arg` is its graphics, palette or event offset.
    virtual void objectCommand(uint8_t op, int object, uint32_t arg) = 0;
    // Event commands of the party, the screen and the sprite palettes (see FF6_EVENT_LOCK_SCREEN to FF6_EVENT_MOVE_PARTY);
    // `bytes` are the command bytes after the opcode.
    virtual void fieldCommand(uint8_t op, const uint8_t* bytes, size_t size) = 0;
    virtual Sound& sound() = 0;
    // Event commands 0xDE, 0xDF, 0xE2, 0xE3 and 0xE4: a bit for each character that is in the active party, is an enabled
    // object, leads the party, is in any party; or the bit of the active party number.
    virtual unsigned characterMask(uint8_t /*op*/) const { return 0; }
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
    // Where the event goes on, for the story that the world map runs and gives back.
    uint32_t pc() const { return pc_; }
    void setPc(uint32_t offset) { pc_ = offset; }
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
