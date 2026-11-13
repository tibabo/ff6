#pragma once

#include <functional>

namespace ff6 {

// An entry of the menu that the menu bar of macOS shows: its title, what it does, and whether it is checked (none: never).
struct MenuToggle {
    const char* title;
    std::function<void()> toggle;
    std::function<bool()> checked;
};

// Adds a menu of these entries to the menu bar of macOS (after the one of the application). Nothing elsewhere.
#ifdef __APPLE__
void installMenu(const char* title, const MenuToggle* toggles, int count);
#else
inline void installMenu(const char*, const MenuToggle*, int) {}
#endif

}  // namespace ff6
