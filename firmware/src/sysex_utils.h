
#pragma once
#include <cstdint>
#include <cmath>

inline int32_t floatToQ16_16(float v) { return (int32_t)std::round(v * 65536.0f); }

inline void packQ16_16(int32_t value, uint8_t out[5])
{
    uint32_t u = (uint32_t)value;
    out[0] =  u        & 0x7F;
    out[1] = (u >> 7)  & 0x7F;
    out[2] = (u >> 14) & 0x7F;
    out[3] = (u >> 21) & 0x7F;
    out[4] = (u >> 28) & 0x7F;
}
