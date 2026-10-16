#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ff6 {

// Size in bytes of the event command at `at`, from the command macros of event.asm. Returns 0 for a command that is
// unknown or variable in a way that its caller must handle (choices), and for one that the script cuts off.
size_t eventCommandLength(const std::vector<uint8_t>& script, size_t at);

}  // namespace ff6
