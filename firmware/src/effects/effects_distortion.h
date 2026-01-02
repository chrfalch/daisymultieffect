
#pragma once
#include "effects/base_effect.h"
#include <cmath>

struct OverdriveEffect : BaseEffect
{
    static constexpr uint8_t TypeId = 10;

    float drive_ = 2.0f; // id0 (1..20)
    float tone_ = 0.5f;  // id1
    float lpL_ = 0, lpR_ = 0;

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    static inline float softclip(float x) { return std::tanh(x); }

    static const NumberParamRange kDriveRange;
    static const NumberParamRange kToneRange;
    static const ParamInfo kParams[2];
    static const EffectMeta kMeta;
    const EffectMeta &GetMetadata() const override { return kMeta; }

    void Init(float) override { lpL_ = lpR_ = 0; }

    void SetParam(uint8_t id, float v) override
    {
        if (id == 0)
            drive_ = 1.0f + v * 19.0f;
        else if (id == 1)
            tone_ = v;
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 2)
            return 0;
        out[0] = {0, (uint8_t)(((drive_ - 1.0f) / 19.0f) * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(tone_ * 127.0f + 0.5f)};
        return 2;
    }

    void ProcessStereo(float &l, float &r) override
    {
        float xL = softclip(l * drive_);
        float xR = softclip(r * drive_);
        float a = 0.02f + 0.45f * tone_;
        lpL_ += a * (xL - lpL_);
        lpR_ += a * (xR - lpR_);
        l = (1.0f - tone_) * xL + tone_ * lpL_;
        r = (1.0f - tone_) * xR + tone_ * lpR_;
    }
};
