
#pragma once
#include "core/effects/base_effect.h"
#include <cmath>

struct CompressorEffect : BaseEffect
{
    static constexpr uint8_t TypeId = 15;

    float threshold_ = 0.5f; // id0: threshold in dB mapped to 0..1 (-40dB to 0dB)
    float ratio_ = 4.0f;     // id1: compression ratio (1:1 to 20:1)
    float attack_ = 0.01f;   // id2: attack time in seconds
    float release_ = 0.1f;   // id3: release time in seconds
    float makeup_ = 1.0f;    // id4: makeup gain (0dB to +24dB)

    // Envelope follower state
    float envL_ = 0.0f;
    float envR_ = 0.0f;
    float sampleRate_ = 48000.0f;

    static const NumberParamRange kThreshRange, kRatioRange, kAttackRange, kReleaseRange, kMakeupRange;
    static const ParamInfo kParams[5];
    static const EffectMeta kMeta;
    const EffectMeta &GetMetadata() const override { return kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        envL_ = envR_ = 0.0f;
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: // Threshold: 0..1 maps to -40dB to 0dB
            threshold_ = v;
            break;
        case 1: // Ratio: 0..1 maps to 1:1 to 20:1
            ratio_ = 1.0f + v * 19.0f;
            break;
        case 2: // Attack: 0..1 maps to 0.1ms to 100ms
            attack_ = 0.0001f + v * 0.0999f;
            break;
        case 3: // Release: 0..1 maps to 10ms to 1000ms
            release_ = 0.01f + v * 0.99f;
            break;
        case 4: // Makeup: 0..1 maps to 0dB to +24dB
            makeup_ = std::pow(10.0f, v * 24.0f / 20.0f);
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 5)
            return 0;
        out[0] = {0, (uint8_t)(threshold_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(((ratio_ - 1.0f) / 19.0f) * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(((attack_ - 0.0001f) / 0.0999f) * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(((release_ - 0.01f) / 0.99f) * 127.0f + 0.5f)};
        float makeupDb = 20.0f * std::log10(makeup_);
        out[4] = {4, (uint8_t)((makeupDb / 24.0f) * 127.0f + 0.5f)};
        return 5;
    }

    void ProcessStereo(float &l, float &r) override
    {
        // Convert threshold from 0..1 to linear amplitude
        // 0 = -40dB, 1 = 0dB
        float threshDb = -40.0f + threshold_ * 40.0f;
        float threshLin = std::pow(10.0f, threshDb / 20.0f);

        // Envelope follower coefficients
        float attackCoef = std::exp(-1.0f / (attack_ * sampleRate_));
        float releaseCoef = std::exp(-1.0f / (release_ * sampleRate_));

        // Get input levels
        float inL = std::fabs(l);
        float inR = std::fabs(r);

        // Envelope follower (peak detection)
        envL_ = (inL > envL_) ? attackCoef * envL_ + (1.0f - attackCoef) * inL
                              : releaseCoef * envL_ + (1.0f - releaseCoef) * inL;
        envR_ = (inR > envR_) ? attackCoef * envR_ + (1.0f - attackCoef) * inR
                              : releaseCoef * envR_ + (1.0f - releaseCoef) * inR;

        // Compute gain reduction for each channel
        auto computeGain = [&](float env) -> float
        {
            if (env <= threshLin || env < 1e-10f)
                return 1.0f;

            // Convert to dB
            float envDb = 20.0f * std::log10(env);
            float overDb = envDb - threshDb;

            // Apply ratio
            float compressedDb = threshDb + overDb / ratio_;
            float targetDb = compressedDb - envDb;

            return std::pow(10.0f, targetDb / 20.0f);
        };

        float gainL = computeGain(envL_);
        float gainR = computeGain(envR_);

        // Apply gain reduction and makeup gain
        l = l * gainL * makeup_;
        r = r * gainR * makeup_;
    }
};
