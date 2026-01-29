
#pragma once
#include "patch/patch_protocol.h"
#include <cstddef>

struct BaseEffect;

// Single-pole DC blocking filter state.
// Transfer function: y[n] = x[n] - x[n-1] + R * y[n-1]
// R = 0.995 gives a ~7.6 Hz cutoff at 48 kHz — well below the lowest
// guitar fundamental (~82 Hz) so no audible content is affected.
struct DcBlocker
{
    float prevIn = 0.0f;
    float prevOut = 0.0f;

    static constexpr float R = 0.995f;

    float Process(float x)
    {
        float y = x - prevIn + R * prevOut;
        prevIn = x;
        prevOut = y;
        return y;
    }

    void Reset()
    {
        prevIn = 0.0f;
        prevOut = 0.0f;
    }
};

struct SlotRuntime
{
    BaseEffect *effect = nullptr;
    uint8_t typeId = 0;
    bool enabled = true;

    // Click-free bypass: crossfade between dry input and processed output.
    // 1.0 = fully processed, 0.0 = fully bypassed.
    float enabledFade = 1.0f;

    uint8_t inputL = ROUTE_INPUT;
    uint8_t inputR = ROUTE_INPUT;
    bool sumToMono = false;

    float dry = 0.0f;
    float wet = 1.0f;
    ChannelPolicy policy = ChannelPolicy::Auto;

    // Per-slot DC blocking (removes accumulated DC offset between effects)
    DcBlocker dcL, dcR;
};

template <size_t MaxSlots>
struct PedalBoardRuntime
{
    float sampleRate = 48000.0f;
    SlotRuntime slots[MaxSlots];
    float outL[MaxSlots];
    float outR[MaxSlots];

    void ResetFrameBuffers()
    {
        for (size_t i = 0; i < MaxSlots; i++)
        {
            outL[i] = 0.0f;
            outR[i] = 0.0f;
        }
    }
};
