
#pragma once
#include "effects/base_effect.h"
#include "daisysp.h"

struct ChorusEffect : BaseEffect
{
    static constexpr uint8_t TypeId = 16;

    daisysp::Chorus chorus_;
    float rate_ = 0.3f;     // id0: LFO rate (0.1-2 Hz)
    float depth_ = 0.4f;    // id1: modulation depth
    float feedback_ = 0.0f; // id2: feedback amount
    float delay_ = 0.4f;    // id3: base delay (0-1 maps to 5-25ms)
    float mix_ = 0.5f;      // id4: wet/dry mix

    static const NumberParamRange kRateRange, kDepthRange, kFeedbackRange, kDelayRange, kMixRange;
    static const ParamInfo kParams[5];
    static const EffectMeta kMeta;
    const EffectMeta &GetMetadata() const override { return kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }

    void Init(float sr) override
    {
        chorus_.Init(sr);
        ApplyParams();
    }

    void ApplyParams()
    {
        // Rate: 0.1 to 2 Hz (classic slow chorus)
        chorus_.SetLfoFreq(0.1f + rate_ * 1.9f);
        chorus_.SetLfoDepth(depth_ * 0.8f); // Scale depth for subtler modulation
        chorus_.SetFeedback(feedback_);
        // Delay: 5-25ms range (classic chorus territory)
        chorus_.SetDelayMs(5.0f + delay_ * 20.0f);
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0:
            rate_ = v;
            break;
        case 1:
            depth_ = v;
            break;
        case 2:
            feedback_ = v;
            break;
        case 3:
            delay_ = v;
            break;
        case 4:
            mix_ = v;
            break;
        }
        ApplyParams();
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 5)
            return 0;
        out[0] = {0, (uint8_t)(rate_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(depth_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(feedback_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(delay_ * 127.0f + 0.5f)};
        out[4] = {4, (uint8_t)(mix_ * 127.0f + 0.5f)};
        return 5;
    }

    void ProcessStereo(float &l, float &r) override
    {
        float dryL = l, dryR = r;

        // Process left channel, then get stereo output
        chorus_.Process(l);
        float wetL = chorus_.GetLeft();
        float wetR = chorus_.GetRight();

        // Process right channel for true stereo
        chorus_.Process(r);
        wetR = chorus_.GetRight();

        // Mix dry/wet
        l = dryL * (1.0f - mix_) + wetL * mix_;
        r = dryR * (1.0f - mix_) + wetR * mix_;
    }
};
