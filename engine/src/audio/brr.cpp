#include "audio/brr.hpp"

#include <algorithm>

#include "constants.hpp"

namespace ff6 {

void decodeBrrBlock(const uint8_t* block, int16_t* history, int16_t* out) {
    const int header = block[0];
    const int range = header >> FF6_BRR_SHIFT_SHIFT;
    const int filter = (header >> FF6_BRR_FILTER_SHIFT) & FF6_BRR_FILTER_MASK;
    int newest = history[0], older = history[1];
    for (int i = 0; i < FF6_BRR_BLOCK_SAMPLES; ++i) {
        const uint8_t byte = block[1 + i / 2];
        int nibble = (i & 1) ? (byte & 0x0F) : (byte >> 4);
        if (nibble >= 8) nibble -= 16;
        int sample = range <= FF6_BRR_MAX_RANGE ? (nibble << range) >> 1 : (nibble < 0 ? -2048 : 0);
        // The filters work on the samples at half scale, like the DSP.
        const int p1 = newest >> 1, p2 = older >> 1;
        switch (filter) {
            case 1: sample += p1 + ((-p1) >> 4); break;
            case 2: sample += (p1 << 1) + ((-(p1 * 3)) >> 5) - p2 + (p2 >> 4); break;
            case 3: sample += (p1 << 1) + ((-(p1 * 13)) >> 6) - p2 + ((p2 * 3) >> 4); break;
            default: break;
        }
        sample = std::max(-32768, std::min(32767, sample));
        const int16_t stored = static_cast<int16_t>(static_cast<uint16_t>(sample * 2));  // wraps like the hardware
        out[i] = stored;
        older = newest;
        newest = stored;
    }
    history[0] = static_cast<int16_t>(newest);
    history[1] = static_cast<int16_t>(older);
}

}  // namespace ff6
