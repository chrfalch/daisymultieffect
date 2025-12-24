
#pragma once
#include "base_effect.h"
#include <cmath>

struct StereoMixerEffect : BaseEffect
{
    static constexpr uint8_t TypeId = 13;

    float mixA_ = 0.5f;  // id0
    float mixB_ = 0.5f;  // id1
    float cross_ = 0.0f; // id2

    static const NumberParamRange kMixRange, kCrossRange;
    static const ParamInfo kParams[3];
    static const EffectMeta kMeta;
    const EffectMeta &GetMetadata() const override { return kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }

    void Init(float) override {}

    void SetParam(uint8_t id, float v) override
    {
        if (id == 0)
            mixA_ = v;
        else if (id == 1)
            mixB_ = v;
        else if (id == 2)
            cross_ = v;
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 3)
            return 0;
        out[0] = {0, (uint8_t)(mixA_ * 127 + 0.5f)};
        out[1] = {1, (uint8_t)(mixB_ * 127 + 0.5f)};
        out[2] = {2, (uint8_t)(cross_ * 127 + 0.5f)};
        return 3;
    }

    // interpret L=A input, R=B input (fed via routing)
    void ProcessStereo(float &l, float &r) override
    {
        float a = l * mixA_;
        float b = r * mixB_;
        float outL = (1.0f - cross_) * a + cross_ * b;
        float outR = (1.0f - cross_) * b + cross_ * a;

        float maxAbs = std::fmax(std::fabs(outL), std::fabs(outR));
        if (maxAbs > 1.0f)
        {
            float g = 1.0f / maxAbs;
            outL *= g;
            outR *= g;
        }

        l = outL;
        r = outR;
    }
};
