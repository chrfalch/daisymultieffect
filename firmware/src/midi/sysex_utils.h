
#pragma once
#include <cstdint>
#include <cmath>

inline int32_t floatToQ16_16(float v) { return (int32_t)std::round(v * 65536.0f); }

inline void packQ16_16(int32_t value, uint8_t out[5])
{
    uint32_t u = (uint32_t)value;
    out[0] = u & 0x7F;
    out[1] = (u >> 7) & 0x7F;
    out[2] = (u >> 14) & 0x7F;
    out[3] = (u >> 21) & 0x7F;
    out[4] = (u >> 28) & 0x7F;
}

inline float unpackQ16_16(const uint8_t in[5])
{
    int32_t val = ((int32_t)(in[0] & 0x7F)) | ((int32_t)(in[1] & 0x7F) << 7) | ((int32_t)(in[2] & 0x7F) << 14) | ((int32_t)(in[3] & 0x7F) << 21) | ((int32_t)(in[4] & 0x0F) << 28);
    return val / 65536.0f;
}
