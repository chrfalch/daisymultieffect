
#pragma once
#include "patch_protocol.h"
#include <cstddef>

struct BaseEffect;

struct SlotRuntime
{
    BaseEffect* effect = nullptr;
    uint8_t typeId = 0;
    bool enabled = true;

    uint8_t inputL = ROUTE_INPUT;
    uint8_t inputR = ROUTE_INPUT;
    bool sumToMono = false;

    float dry = 0.0f;
    float wet = 1.0f;
    ChannelPolicy policy = ChannelPolicy::Auto;
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
        for(size_t i=0;i<MaxSlots;i++){ outL[i]=0.0f; outR[i]=0.0f; }
    }
};
