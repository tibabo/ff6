#pragma once

#include <array>
#include <string>
#include <vector>

#include "input.hpp"
#include "menu/character_menu.hpp"
#include "menu/menu_input.hpp"
#include "menu/menu_screen.hpp"
#include "sound.hpp"

namespace ff6 {

// A character as the party menu needs it: whether it is an object of the game, its party, how it looks on the map and what
// the menu says about it.
struct PartyMenuCharacter {
    bool enabled = false;
    int party = 0;  // 0 for none
    int gfx = 0, palette = 0;  // the sprite of the map, and the sprite palette
    uint32_t status = 0;  // FF6_ST_*
    MenuCharacter info;
    std::vector<uint8_t> esper;  // the name of the esper that it carries, in the codes of the menu font
};

struct PartyMenuSetup {
    std::array<PartyMenuCharacter, FF6_CHAR_COUNT> characters;
    unsigned forced = 0;  // the characters that cannot be moved
    int parties = 1;
    bool clear = false;  // the parties are emptied first
};

// The party menu of the game (src/menu/party.asm): the characters that are in no party stand in a block of two rows of eight, the
// parties have a window of four places each below it. A place is chosen and then another one: they swap. Start or B ends the
// menu once every party has someone in it. Choosing the same place twice opens the status screen of the character.
class PartyMenu {
public:
    static constexpr int kBlockSlots = 16, kPartyPlaces = 4, kMaxParties = 3;

    PartyMenu(MenuScreen& screen, Sound& sound) : screen_(screen), sound_(sound), status_(screen, sound) {}

    void open(const PartyMenuSetup& setup);
    void update(const InputState& input);
    void draw();
    bool closed() const { return closed_; }
    // The party of each character (0 for none) and its place in it, once the menu is closed.
    std::array<int, FF6_CHAR_COUNT> assignment() const;
    std::array<int, FF6_CHAR_COUNT> places() const;
    // The character in a place of the block, or of a party (from 0): -1 for none.
    int inBlock(int index) const { return slots_[static_cast<size_t>(index)]; }
    int inParty(int party, int place) const { return slots_[static_cast<size_t>(kBlockSlots + party * kPartyPlaces + place)]; }
    // Where the hand is: the index of its place (the block first, then the places of the parties), and whether a place is picked.
    int cursor() const { return slotIndex(area_, col_, row_); }
    bool picked() const { return picking_; }
    int message() const { return error_; }
    bool viewing() const { return viewing_; }  // the status screen of a character is open

private:
    static int slotIndex(int area, int col, int row);
    int count(int party) const;
    void move(int direction);
    void finish();
    void drawText(int x, int y, const std::string& text, int color);
    void drawHand(int index);
    void drawIcon(int index);
    void drawStatus(const PartyMenuCharacter& character);

    MenuScreen& screen_;
    Sound& sound_;
    CharacterMenu status_;
    bool viewing_ = false;
    std::array<PartyMenuCharacter, FF6_CHAR_COUNT> characters_;
    std::array<int, kBlockSlots + kMaxParties * kPartyPlaces> slots_{};  // the character in each place
    std::array<int, FF6_CHAR_COUNT> kept_{};  // the party of the characters that the menu does not show
    unsigned forced_ = 0;
    int parties_ = 1;
    int area_ = 0, col_ = 0, row_ = 0;  // 0 the block, 1 the parties
    bool picking_ = false;
    int picked_ = 0;
    int error_ = 0, errorFrames_ = 0;  // the message that is shown (1 not enough groups, 2 nobody), and for how long
    bool closed_ = false;
    DirectionRepeat repeat_;
};

}  // namespace ff6
