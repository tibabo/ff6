#include "field/event.hpp"

#include <algorithm>
#include <cstdio>

#include "field/event_cmd.hpp"

namespace ff6 {
namespace {

constexpr uint32_t kEndOfEvent = 0xFFFFFFFF;  // return address of the first stack frame
constexpr int kPauseFrames[] = {15, 30, 45, 60, 120};  // commands 0x91 to 0x95

// The song commands: bit 7 of the song asks to keep the song that plays.
void playSong(Sound& sound, uint8_t song, uint8_t volume) {
    const uint8_t command = (song & ~FF6_SOUND_SONG_MASK) != 0 ? FF6_SOUND_PAUSE_SONG : FF6_SOUND_PLAY_SONG;
    sound.command(command, song & FF6_SOUND_SONG_MASK, volume);
}

}  // namespace

void EventBits::init(const CommonData& common) {
    bytes_.fill(0);
    battle_.fill(0);
    const size_t first = FF6_NPC_SWITCH_EVENT_BIT / 8;
    for (size_t i = 0; i < FF6_NPC_SWITCH_BYTES && i < common.npcSwitchDefaults.size() && first + i < bytes_.size(); ++i) {
        bytes_[first + i] = common.npcSwitchDefaults[i];
    }
}

bool EventBits::get(int id) const {
    if (id < 0 || id >= FF6_EVENT_BIT_COUNT) return false;
    return (bytes_[static_cast<size_t>(id) / 8] >> (id % 8)) & 1;
}

void EventBits::set(int id, bool value) {
    if (id < 0 || id >= FF6_EVENT_BIT_COUNT) return;
    const uint8_t mask = static_cast<uint8_t>(1 << (id % 8));
    uint8_t& byte = bytes_[static_cast<size_t>(id) / 8];
    byte = value ? static_cast<uint8_t>(byte | mask) : static_cast<uint8_t>(byte & ~mask);
}

bool EventBits::battle(int id) const {
    if (id < 0 || id >= FF6_BATTLE_FLAG_BYTES * 8) return false;
    return (battle_[static_cast<size_t>(id) / 8] >> (id % 8)) & 1;
}

void EventBits::setBattle(int id, bool value) {
    if (id < 0 || id >= FF6_BATTLE_FLAG_BYTES * 8) return;
    const uint8_t mask = static_cast<uint8_t>(1 << (id % 8));
    uint8_t& byte = battle_[static_cast<size_t>(id) / 8];
    byte = value ? static_cast<uint8_t>(byte | mask) : static_cast<uint8_t>(byte & ~mask);
}

std::vector<int> EventBits::setIds() const {
    std::vector<int> ids;
    for (int id = 0; id < FF6_EVENT_BIT_COUNT; ++id) {
        if (get(id)) ids.push_back(id);
    }
    return ids;
}

std::vector<int> EventBits::battleSetIds() const {
    std::vector<int> ids;
    for (int id = 0; id < FF6_BATTLE_FLAG_BYTES * 8; ++id) {
        if (battle(id)) ids.push_back(id);
    }
    return ids;
}

void EventBits::load(const std::vector<int>& bits, const std::vector<int>& battleFlags) {
    bytes_.fill(0);
    battle_.fill(0);
    for (const int id : bits) set(id, true);
    for (const int id : battleFlags) setBattle(id, true);
}

EventTrigger decodeEventTrigger(const uint8_t* bytes) {
    EventTrigger trigger;
    trigger.x = bytes[0];
    trigger.y = bytes[1];
    trigger.offset = static_cast<uint32_t>(bytes[2] | (bytes[3] << 8) | (bytes[4] << 16));
    return trigger;
}

void EventRunner::start(uint32_t offset) {
    running_ = true;
    pc_ = offset;
    pause_ = 0;
    waitObject_ = -1;
    waited_ = 0;
    soundWaited_ = 0;
    mapFadeStarted_ = false;
    stack_.clear();
    stack_.push_back({kEndOfEvent, offset, 1});  // returning from the event itself ends it
}

void EventRunner::call(uint32_t offset) {
    if (!pushFrame(pc_, offset, 1)) return;
    pc_ = offset;
}

void EventRunner::stop() {
    running_ = false;
    pause_ = 0;
    waitObject_ = -1;
    waited_ = 0;
    soundWaited_ = 0;
    mapFadeStarted_ = false;
}

bool EventRunner::pushFrame(uint32_t returnPc, uint32_t loopStart, int count) {
    if (stack_.size() >= FF6_EVENT_STACK_SIZE) return false;
    stack_.push_back({returnPc, loopStart, count});
    return true;
}

uint32_t EventRunner::addressAt(size_t at) const {
    const std::vector<uint8_t>& script = common_.eventScript;
    return static_cast<uint32_t>(script[at] | (script[at + 1] << 8) | (script[at + 2] << 16));
}

bool EventRunner::coinFlip() {
    seed_ = seed_ * 1664525u + 1013904223u;
    return ((seed_ >> 16) & 0xFF) >= FF6_RANDOM_HALF;
}

// Commands 0xC0-0xCF: "if any" / "if all" of up to 8 switches, each with the value that it must have.
bool EventRunner::conditionMet(uint8_t op, bool* ok) const {
    const std::vector<uint8_t>& script = common_.eventScript;
    const int count = (op & 7) + 1;
    const bool all = op >= FF6_EVENT_IF_ALL;
    *ok = pc_ + 1 + static_cast<size_t>(count) * 2 + FF6_EVENT_ADDRESS_SIZE <= script.size();
    if (!*ok) return false;
    bool result = all;
    for (int i = 0; i < count; ++i) {
        const size_t at = pc_ + 1 + static_cast<size_t>(i) * 2;
        const uint16_t word = static_cast<uint16_t>(script[at] | (script[at + 1] << 8));
        const bool matches = bits_.get(word & ~FF6_EVENT_SWITCH_EXPECT_SET) == ((word & FF6_EVENT_SWITCH_EXPECT_SET) != 0);
        result = all ? (result && matches) : (result || matches);
    }
    return result;
}

void EventRunner::update(EventHost& host) {
    if (!running_ || host.dialogueBusy()) return;
    for (int i = 0; i < FF6_EVENT_COMMANDS_PER_TICK && running_; ++i) {
        if (pause_ > 0) {
            --pause_;
            return;
        }
        if (waitObject_ >= 0) {
            if (host.objectBusy(waitObject_) && waited_ < FF6_EVENT_OBJECT_WAIT_LIMIT) {
                ++waited_;
                return;
            }
            // An object that cannot get where it goes (blocked, or not there) must not hold the event for ever.
            if (waited_ >= FF6_EVENT_OBJECT_WAIT_LIMIT) {
                std::fprintf(stderr, "event: gave up waiting for object 0x%02X\n", waitObject_);
            }
            waitObject_ = -1;
        }
        if (!step(host)) return;
    }
}

bool EventRunner::step(EventHost& host) {
    const std::vector<uint8_t>& script = common_.eventScript;
    if (pc_ >= script.size()) return stop(), true;
    const uint8_t op = script[pc_];
    const size_t length = eventCommandLength(script, pc_);
    if (length == 0 && op != FF6_EVENT_CHOICE) {
        if (op != 0xFF) std::fprintf(stderr, "event: unsupported command 0x%02X at 0x%05X, event ended\n", op, pc_);
        return stop(), true;
    }
    const auto byteAt = [&](size_t n) { return script[pc_ + n]; };
    const auto next = [&] { return pc_ += static_cast<uint32_t>(length), true; };
    const auto jump = [&](size_t at) { return pc_ = addressAt(at), true; };

    if (op <= FF6_EVENT_MAX_OBJ_OP) {  // an object script: the object runs it while the event goes on or waits
        const int object = op;
        const bool wait = (byteAt(1) & FF6_EVENT_OBJ_WAIT) != 0;
        const uint32_t scriptOffset = pc_ + 2;
        next();
        if (host.startObjectScript(object, scriptOffset) && wait) {
            waitObject_ = object;
            waited_ = 0;
        }
        return true;
    }
    if (op >= FF6_EVENT_LOAD_MAP_FIRST && op <= FF6_EVENT_LOAD_MAP_LAST) {
        if (op == FF6_EVENT_LOAD_MAP_FIRST) {  // the first of them waits for the screen to fade out
            if (!mapFadeStarted_) {
                host.fieldCommand(FF6_EVENT_FADE_OUT, nullptr, 0);
                mapFadeStarted_ = true;
                return false;
            }
            if (host.fading()) return false;
            mapFadeStarted_ = false;
        }
        const uint16_t word = static_cast<uint16_t>(byteAt(1) | (byteAt(2) << 8));
        const uint32_t map = word & FF6_ENTRANCE_MAP_MASK;
        const int x = byteAt(3), y = byteAt(4);
        const int facing = (word >> FF6_ENTRANCE_FACING_SHIFT) & FF6_NPC_DIR_MASK;
        const bool startup = (byteAt(5) & FF6_EVENT_MAP_STARTUP_FLAG) != 0;
        const bool fadeIn = (byteAt(5) & FF6_EVENT_MAP_NO_FADE_IN_FLAG) == 0;
        next();  // the event goes on after the map is loaded, once its startup event is over
        host.changeMap(map, x, y, facing, startup, fadeIn);
        return false;
    }
    if (op >= FF6_EVENT_PAUSE_15F && op <= FF6_EVENT_PAUSE_120F) {
        pause_ = kPauseFrames[op - FF6_EVENT_PAUSE_15F];
        return next();
    }
    if (op >= FF6_EVENT_IF_ANY && op <= FF6_EVENT_IF_LAST) {
        bool ok = false;
        const bool met = conditionMet(op, &ok);
        return met ? jump(pc_ + length - FF6_EVENT_ADDRESS_SIZE) : next();
    }
    if (op >= FF6_EVENT_SET_SWITCH && op <= FF6_EVENT_SWITCH_LAST) {
        const int id = ((op - FF6_EVENT_SET_SWITCH) >> 1) * 0x100 + byteAt(1);
        bits_.set(id, ((op - FF6_EVENT_SET_SWITCH) & 1) == 0);
        return next();
    }

    switch (op) {
        case FF6_EVENT_DLG:
        case FF6_EVENT_DLG_ASYNC: {
            const uint16_t word = static_cast<uint16_t>(byteAt(1) | (byteAt(2) << 8));
            next();
            host.openDialogue(word & FF6_DLG_ID_MASK, static_cast<uint16_t>(word & ~FF6_DLG_ID_MASK));
            return false;
        }
        case FF6_EVENT_CHOICE: {
            const int count = host.dialogueChoiceCount();
            const int choice = host.dialogueChoice();
            const size_t at = pc_ + 1 + static_cast<size_t>(choice) * FF6_EVENT_ADDRESS_SIZE;
            if (count <= 0 || choice < 0 || choice >= count || at + FF6_EVENT_ADDRESS_SIZE > script.size()) {
                return stop(), true;
            }
            return jump(at);
        }
        case FF6_EVENT_WAIT_OBJ:
            waitObject_ = byteAt(1);
            waited_ = 0;
            return next();
        case FF6_EVENT_OBJ_GFX:
        case FF6_EVENT_OBJ_PAL:
            host.objectCommand(op, byteAt(1), byteAt(2));
            return next();
        case FF6_EVENT_OBJ_EVENT:
            host.objectCommand(op, byteAt(1), static_cast<uint32_t>(byteAt(2) | (byteAt(3) << 8) | ((byteAt(4) & FF6_NPC_BANK_MASK) << 16)));
            return next();
        case FF6_EVENT_OBJ_PASS_OFF:
        case FF6_EVENT_OBJ_PASS_ON:
        case FF6_EVENT_OBJ_CREATE:
        case FF6_EVENT_OBJ_DELETE:
        case FF6_EVENT_OBJ_SHOW:
        case FF6_EVENT_OBJ_HIDE:
        case FF6_EVENT_OBJ_TOUCH_ON:
        case FF6_EVENT_OBJ_TOUCH_OFF:
            host.objectCommand(op, byteAt(1), 0);
            return next();
        case FF6_EVENT_PAUSE:
            pause_ = byteAt(1);
            return next();
        case FF6_EVENT_LOCK_SCREEN:
        case FF6_EVENT_UNLOCK_SCREEN:
        case FF6_EVENT_PARTY_USER:
        case FF6_EVENT_PARTY_SCRIPT:
        case FF6_EVENT_SET_PARTY:
        case FF6_EVENT_ADD_CHAR:
        case FF6_EVENT_ACTIVATE_PARTY:
        case FF6_EVENT_UPDATE_PARTY:
        case FF6_EVENT_SET_PALETTE:
        case FF6_EVENT_PARTY_MAP:
        case FF6_EVENT_MAP_DATA_NOW:
        case FF6_EVENT_MAP_DATA:
        case FF6_EVENT_FADE_IN:
        case FF6_EVENT_FADE_OUT:
        case FF6_EVENT_FADE_IN_SPEED:
        case FF6_EVENT_FADE_OUT_SPEED:
        case FF6_EVENT_FADE_STOP:
        case FF6_EVENT_MOVE_PARTY: {
            const size_t at = pc_ + 1;
            host.fieldCommand(op, at < script.size() ? &script[at] : nullptr,
                              std::min(length - 1, script.size() - std::min(at, script.size())));
            return next();
        }
        case FF6_EVENT_PAUSE_LONG:
            pause_ = byteAt(1) * FF6_EVENT_LONG_PAUSE_UNIT;
            return next();
        case FF6_EVENT_CALL:  // the subroutine ends with a return, which comes back to the command after the call
            if (!pushFrame(pc_ + static_cast<uint32_t>(length), addressAt(pc_ + 1), 1)) return stop(), true;
            return jump(pc_ + 1);
        case FF6_EVENT_CALL_LOOP:
            if (!pushFrame(pc_ + static_cast<uint32_t>(length), addressAt(pc_ + 2), byteAt(1))) return stop(), true;
            return jump(pc_ + 2);
        case FF6_EVENT_RETURN: {
            Frame& top = stack_.back();
            top.count = (top.count - 1) & 0xFF;
            if (top.count != 0) {  // another round of the subroutine
                pc_ = top.loopStart;
                return true;
            }
            const uint32_t returnPc = top.returnPc;
            stack_.pop_back();
            if (returnPc == kEndOfEvent || stack_.empty()) return stop(), true;
            pc_ = returnPc;
            return true;
        }
        case FF6_EVENT_LOOP:
            if (!pushFrame(0, pc_ + static_cast<uint32_t>(length), byteAt(1))) return stop(), true;
            return next();
        case FF6_EVENT_LOOP_END: {
            if (stack_.size() < 2) return next();
            Frame& top = stack_.back();
            top.count = (top.count - 1) & 0xFF;
            if (top.count != 0) {
                pc_ = top.loopStart;
                return true;
            }
            stack_.pop_back();
            return next();
        }
        case FF6_EVENT_LOOP_UNTIL: {  // used to wait for a keypress: loops until the event bit is set
            if (stack_.size() < 2) return next();
            if (!bits_.get(byteAt(1) | (byteAt(2) << 8))) {
                pc_ = stack_.back().loopStart;
                return true;
            }
            stack_.pop_back();
            return next();
        }
        case FF6_EVENT_RANDOM_JUMP:
            return coinFlip() ? jump(pc_ + 1) : next();
        case FF6_EVENT_BATTLE_JUMP:  // jumps while the battle flag is clear
            return bits_.battle(byteAt(1)) ? next() : jump(pc_ + 2);
        case FF6_EVENT_BATTLE_SET:
            bits_.setBattle(byteAt(1), true);
            return next();
        case FF6_EVENT_BATTLE_CLEAR:
            bits_.setBattle(byteAt(1), false);
            return next();
        case FF6_EVENT_FADE_WAIT:
            if (host.fading()) return false;
            return next();
        case FF6_EVENT_PLAY_SONG:
            playSong(host.sound(), byteAt(1), FF6_SOUND_FULL_VOLUME);
            return next();
        case FF6_EVENT_PLAY_SONG_VOLUME:
            playSong(host.sound(), byteAt(1), byteAt(2));
            return next();
        case FF6_EVENT_FADE_IN_SONG:  // starts quietly and rises to full volume
            playSong(host.sound(), byteAt(1), FF6_SOUND_FADE_IN_VOLUME);
            host.sound().command(FF6_SOUND_SET_SONG_VOLUME, byteAt(2), FF6_SOUND_FULL_VOLUME);
            return next();
        case FF6_EVENT_FADE_OUT_SONG:
            host.sound().command(FF6_SOUND_SET_SONG_VOLUME, byteAt(1), 0);
            return next();
        case FF6_EVENT_FADE_IN_PREVIOUS: {
            Sound& sound = host.sound();
            if (sound.previousSong() >= 0) {
                sound.command(FF6_SOUND_PLAY_SONG, static_cast<uint8_t>(sound.previousSong()), 0);
                sound.command(FF6_SOUND_SET_SONG_VOLUME, byteAt(1), FF6_SOUND_FULL_VOLUME);
            }
            return next();
        }
        case FF6_EVENT_PLAY_SFX:
            host.sound().command(FF6_SOUND_GAME_SFX, byteAt(1), FF6_SFX_PAN_CENTER);
            return next();
        case FF6_EVENT_PLAY_SFX_PAN:
            host.sound().command(FF6_SOUND_GAME_SFX, byteAt(1), FF6_SFX_PAN_CENTER);
            host.sound().command(FF6_SOUND_SET_SFX_PAN, byteAt(2), byteAt(3));  // the bytes go to the program as they are
            return next();
        case FF6_EVENT_SOUND_COMMAND:
            host.sound().command(byteAt(1), byteAt(2), byteAt(3));
            return next();
        case FF6_EVENT_CONTINUE_SONG:
            host.sound().command(FF6_SOUND_CONTINUE_SONG, 0, 0);
            return next();
        case FF6_EVENT_WAIT_SOUND:
        case FF6_EVENT_WAIT_SONG_POSITION:
        case FF6_EVENT_WAIT_SONG_END: {
            const Sound& sound = host.sound();
            const bool done = op == FF6_EVENT_WAIT_SOUND ? sound.port(FF6_SOUND_PORT_CONTINUE) == 0
                              : op == FF6_EVENT_WAIT_SONG_POSITION ? sound.port(FF6_SOUND_PORT_SONG_POSITION) == byteAt(1)
                                                                   : sound.port(FF6_SOUND_PORT_PLAYING) == 0;
            if (!done && soundWaited_ < FF6_EVENT_SOUND_WAIT_LIMIT) {
                ++soundWaited_;
                return false;
            }
            if (!done) std::fprintf(stderr, "event: gave up waiting for the sound program\n");
            soundWaited_ = 0;
            return next();
        }
        default: return next();  // a known command without an effect here
    }
}

}  // namespace ff6
