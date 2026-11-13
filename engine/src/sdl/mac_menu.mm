#include "sdl/mac_menu.hpp"

#import <Cocoa/Cocoa.h>

#include <vector>

namespace {
std::vector<ff6::MenuToggle>& toggles() {
    static std::vector<ff6::MenuToggle> list;
    return list;
}
}  // namespace

// The target of the entries: the tag of an entry is its place in the list; its check mark is refreshed when the menu opens.
@interface FF6MenuTarget : NSObject <NSMenuItemValidation>
- (void)toggle:(NSMenuItem*)item;
@end

@implementation FF6MenuTarget
- (void)toggle:(NSMenuItem*)item {
    const auto& list = toggles();
    if (item.tag >= 0 && item.tag < static_cast<NSInteger>(list.size())) list[static_cast<size_t>(item.tag)].toggle();
}
- (BOOL)validateMenuItem:(NSMenuItem*)item {
    const auto& list = toggles();
    if (item.tag >= 0 && item.tag < static_cast<NSInteger>(list.size()) && list[static_cast<size_t>(item.tag)].checked) {
        item.state = list[static_cast<size_t>(item.tag)].checked() ? NSControlStateValueOn : NSControlStateValueOff;
    }
    return YES;
}
@end

namespace ff6 {

void installMenu(const char* title, const MenuToggle* entries, int count) {
    @autoreleasepool {
        NSMenu* bar = [NSApp mainMenu];
        if (!bar) return;
        static FF6MenuTarget* target = [[FF6MenuTarget alloc] init];
        NSString* name = [NSString stringWithUTF8String:title];
        NSMenu* menu = [[NSMenu alloc] initWithTitle:name];
        for (int i = 0; i < count; ++i) {
            toggles().push_back(entries[i]);
            NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:entries[i].title]
                                                          action:@selector(toggle:)
                                                   keyEquivalent:@""];
            item.target = target;
            item.tag = static_cast<NSInteger>(toggles().size() - 1);
            [menu addItem:item];
        }
        NSMenuItem* holder = [[NSMenuItem alloc] initWithTitle:name action:nil keyEquivalent:@""];
        holder.submenu = menu;
        [bar insertItem:holder atIndex:bar.numberOfItems > 0 ? 1 : 0];
    }
}

}  // namespace ff6
