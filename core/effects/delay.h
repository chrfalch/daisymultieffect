
#pragma once
#include "effects/time_synced_effect.h"
#include "effects/effect_metadata.h"

struct DelayEffect : TimeSyncedEffect
{
    static constexpr uint8_t TypeId = Effects::Delay::TypeId;
    static constexpr int MAX_SAMPLES = 48000 * 2;

    float *bufL_ = nullptr;
    float *bufR_ = nullptr;
    int wp = 0;

    float feedback_ = 0.4f; // id3
    float mix_ = 0.5f;      // id4

    const EffectMeta &GetMetadata() const override { return Effects::Delay::kMeta; }

    DelayEffect(TempoSource &t) : TimeSyncedEffect(t) {}

    void BindBuffers(float *l, float *r)
    {
        bufL_ = l;
        bufR_ = r;
    }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    void Init(float sr) override
    {
        TimeSyncedEffect::Init(sr);
        wp = 0;
        if (bufL_ && bufR_)
        {
            for (int i = 0; i < MAX_SAMPLES; i++)
            {
                bufL_[i] = 0;
                bufR_[i] = 0;
            }
        }
    }

    void SetParam(uint8_t id, float v) override
    {
        if (id <= 2)
            TimeSyncedEffect::SetParam(id, v);
        else if (id == 3)
            feedback_ = 0.95f * v;
        else if (id == 4)
            mix_ = v;
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 5)
            return 0;
        uint8_t n = TimeSyncedEffect::GetParamsSnapshot(out, max);
        out[n++] = {3, (uint8_t)(feedback_ / 0.95f * 127 + 0.5f)};
        out[n++] = {4, (uint8_t)(mix_ * 127 + 0.5f)};
        return n;
    }

    void ProcessStereo(float &l, float &r) override
    {
        if (!bufL_ || !bufR_)
            return;
        int d = GetPeriodSamples();
        if (d >= MAX_SAMPLES)
            d = MAX_SAMPLES - 1;
        int rp = wp - d;
        if (rp < 0)
            rp += MAX_SAMPLES;
        float dl = bufL_[rp], dr = bufR_[rp];
        float inL = l, inR = r;
        bufL_[wp] = inL + dl * feedback_;
        bufR_[wp] = inR + dr * feedback_;
        if (++wp >= MAX_SAMPLES)
            wp = 0;
        float dry = 1.0f - mix_, wet = mix_;
        l = inL * dry + dl * wet;
        r = inR * dry + dr * wet;
    }
};
