#include "../check.hpp"
#include "../tests.hpp"
#include "audio/brr.hpp"

void testBrr() {
    {  // filter 0 keeps the nibbles: range 4 gives (n << 4) >> 1 at half scale, so n * 16 at full scale
        const uint8_t block[9] = {0x40, 0x1F, 0x70, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00};
        int16_t history[2] = {0, 0}, out[16];
        ff6::decodeBrrBlock(block, history, out);
        CHECK(out[0] == 16 && out[1] == -16 && out[2] == 112 && out[3] == 0 && out[4] == -128);
        CHECK(history[0] == out[15] && history[1] == out[14]);
    }
    {  // ranges above 12 only keep the sign
        const uint8_t block[9] = {0xD0, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        int16_t history[2] = {0, 0}, out[16];
        ff6::decodeBrrBlock(block, history, out);
        CHECK(out[0] == 0 && out[1] == -4096);
    }
    {  // filter 1 adds 15/16 of the previous sample; the history crosses blocks
        const uint8_t block[9] = {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        int16_t history[2] = {1600, 0}, out[16];
        ff6::decodeBrrBlock(block, history, out);
        CHECK(out[0] == 1500);  // 800 - 800/16 = 750 at half scale
        CHECK(out[1] > 1300 && out[1] < out[0]);  // it decays
    }
    {  // results wrap to 16 bits like the hardware instead of saturating the filter state
        const uint8_t block[9] = {0xC0, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
        int16_t history[2] = {0, 0}, out[16];
        ff6::decodeBrrBlock(block, history, out);
        CHECK(out[0] == 28672 && out[15] == 28672);
    }
}
