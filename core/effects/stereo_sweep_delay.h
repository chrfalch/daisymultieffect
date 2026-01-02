
#pragma once
#include "effects/time_synced_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>

struct StereoSweepDelayEffect : TimeSyncedEffect
{
    static constexpr uint8_t TypeId = Effects::SweepDelay::TypeId;
    static constexpr int MAX_SAMPLES = 48000 * 2;

    float *bufL_ = nullptr;
    float *bufR_ = nullptr;
    int wp = 0;

    float feedback_ = 0.4f;  // id3
    float mix_ = 0.6f;       // id4
    float panDepth_ = 1.0f;  // id5
    float panRateHz_ = 0.5f; // id6
    float phase_ = 0.0f;

    const EffectMeta &GetMetadata() const override { return Effects::SweepDelay::kMeta; }

    StereoSweepDelayEffect(TempoSource &t) : TimeSyncedEffect(t) {}

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
        phase_ = 0;
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
        else if (id == 5)
            panDepth_ = v;
        else if (id == 6)
            panRateHz_ = 0.05f + v * 4.95f;
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 7)
            return 0;
        uint8_t n = TimeSyncedEffect::GetParamsSnapshot(out, max);
        out[n++] = {3, (uint8_t)(feedback_ / 0.95f * 127 + 0.5f)};
        out[n++] = {4, (uint8_t)(mix_ * 127 + 0.5f)};
        out[n++] = {5, (uint8_t)(panDepth_ * 127 + 0.5f)};
        float rn = (panRateHz_ - 0.05f) / 4.95f;
        if (rn < 0)
            rn = 0;
        if (rn > 1)
            rn = 1;
        out[n++] = {6, (uint8_t)(rn * 127 + 0.5f)};
        return n;
    }

    void ProcessStereo(float &l, float &r) override
    {
        if (!bufL_ || !bufR_)
            return;
        float inL = l, inR = r;

        int d = GetPeriodSamples();
        if (d >= MAX_SAMPLES)
            d = MAX_SAMPLES - 1;
        int rp = wp - d;
        if (rp < 0)
            rp += MAX_SAMPLES;

        float dl = bufL_[rp], dr = bufR_[rp];

        // pan LFO
        float dt = 1.0f / sampleRate_;
        phase_ += panRateHz_ * dt;
        if (phase_ >= 1.0f)
            phase_ -= 1.0f;
        float pan = 0.5f * (1.0f + std::sin(2 * 3.14159265f * phase_)); // 0..1
        float baseL = 1.0f - pan, baseR = pan;
        float panL = (1.0f - panDepth_) * 0.5f + panDepth_ * baseL;
        float panR = (1.0f - panDepth_) * 0.5f + panDepth_ * baseR;

        float inMono = 0.5f * (inL + inR);
        bufL_[wp] = inMono + dl * feedback_;
        bufR_[wp] = inMono + dr * feedback_;
        if (++wp >= MAX_SAMPLES)
            wp = 0;

        float wetL = dl * panL, wetR = dr * panR;
        float dry = 1.0f - mix_, wet = mix_;
        l = inL * dry + wetL * wet;
        r = inR * dry + wetR * wet;
    }
};
