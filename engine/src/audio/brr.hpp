#pragma once

#include <cstdint>

namespace ff6 {

// Decodes one BRR block (the 9 bytes at `block`) into 16 samples at the 16-bit scale of the DSP.
// `history` holds the two previous samples, newest first, and is updated for the next block.
void decodeBrrBlock(const uint8_t* block, int16_t* history, int16_t* out);

}  // namespace ff6
