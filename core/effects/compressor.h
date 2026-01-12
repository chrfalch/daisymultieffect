
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"
#include <algorithm>

struct CompressorEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Compressor::TypeId;

    // Soft knee width in dB
    static constexpr float kKneeWidthDb = 6.0f;
    static constexpr float kHalfKneeDb = kKneeWidthDb * 0.5f;

    // Raw parameter values (0..1 normalized)
    float thresholdNorm_ = 0.5f; // id0: threshold normalized
    float ratioNorm_ = 0.15789f; // id1: ratio normalized (default ~4:1)
    float attackNorm_ = 0.099f;  // id2: attack normalized
    float releaseNorm_ = 0.091f; // id3: release normalized
    float makeupNorm_ = 0.0f;    // id4: makeup normalized

    // Pre-computed coefficients (updated in SetParam/Init)
    float threshDb_ = -20.0f;  // Threshold in dB
    float threshLin_ = 0.1f;   // Threshold as linear amplitude
    float ratio_ = 4.0f;       // Compression ratio
    float attackCoef_ = 0.0f;  // Attack envelope coefficient
    float releaseCoef_ = 0.0f; // Release envelope coefficient
    float makeupLin_ = 1.0f;   // Makeup gain as linear amplitude

    // Envelope follower state (stereo-linked, single envelope)
    float env_ = 0.0f;
    float sampleRate_ = 48000.0f;

    const EffectMeta &GetMetadata() const override { return Effects::Compressor::kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        env_ = 0.0f;
        // Recompute all coefficients with new sample rate
        updateCoefficients();
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: // Threshold: 0..1 maps to -40dB to 0dB
            thresholdNorm_ = v;
            threshDb_ = -40.0f + v * 40.0f;
            threshLin_ = FastMath::fastDbToLin(threshDb_);
            break;
        case 1: // Ratio: 0..1 maps to 1:1 to 20:1
            ratioNorm_ = v;
            ratio_ = 1.0f + v * 19.0f;
            break;
        case 2: // Attack: 0..1 maps to 0.1ms to 100ms
            attackNorm_ = v;
            {
                float attackTime = 0.0001f + v * 0.0999f;
                attackCoef_ = FastMath::calcEnvelopeCoeff(attackTime, sampleRate_);
            }
            break;
        case 3: // Release: 0..1 maps to 10ms to 1000ms
            releaseNorm_ = v;
            {
                float releaseTime = 0.01f + v * 0.99f;
                releaseCoef_ = FastMath::calcEnvelopeCoeff(releaseTime, sampleRate_);
            }
            break;
        case 4: // Makeup: 0..1 maps to 0dB to +24dB
            makeupNorm_ = v;
            makeupLin_ = FastMath::fastDbToLin(v * 24.0f);
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 5)
            return 0;
        out[0] = {0, (uint8_t)(thresholdNorm_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(ratioNorm_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(attackNorm_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(releaseNorm_ * 127.0f + 0.5f)};
        out[4] = {4, (uint8_t)(makeupNorm_ * 127.0f + 0.5f)};
        return 5;
    }

    void ProcessStereo(float &l, float &r) override
    {
        // Stereo-linked envelope detection (max of L/R preserves stereo image)
        float inputLevel = FastMath::fmax(FastMath::fabs(l), FastMath::fabs(r));

        // Envelope follower (peak detection) with pre-computed coefficients
        if (inputLevel > env_)
        {
            env_ = attackCoef_ * env_ + (1.0f - attackCoef_) * inputLevel;
        }
        else
        {
            env_ = releaseCoef_ * env_ + (1.0f - releaseCoef_) * inputLevel;
        }

        // Compute gain with soft knee
        float gain = computeGainSoftKnee(env_);

        // Apply same gain to both channels (stereo-linked) with makeup
        float totalGain = gain * makeupLin_;
        l *= totalGain;
        r *= totalGain;
    }

private:
    // Recompute all coefficients (called from Init)
    void updateCoefficients()
    {
        threshDb_ = -40.0f + thresholdNorm_ * 40.0f;
        threshLin_ = FastMath::fastDbToLin(threshDb_);
        ratio_ = 1.0f + ratioNorm_ * 19.0f;

        float attackTime = 0.0001f + attackNorm_ * 0.0999f;
        attackCoef_ = FastMath::calcEnvelopeCoeff(attackTime, sampleRate_);

        float releaseTime = 0.01f + releaseNorm_ * 0.99f;
        releaseCoef_ = FastMath::calcEnvelopeCoeff(releaseTime, sampleRate_);

        makeupLin_ = FastMath::fastDbToLin(makeupNorm_ * 24.0f);
    }

    // Compute gain reduction with soft knee
    inline float computeGainSoftKnee(float env) const
    {
        // Avoid log of zero/tiny values
        if (env < 1e-10f)
            return 1.0f;

        float envDb = FastMath::fastLinToDb(env);

        // Below knee region: no compression
        if (envDb < threshDb_ - kHalfKneeDb)
            return 1.0f;

        float gainReductionDb;

        if (envDb < threshDb_ + kHalfKneeDb)
        {
            // Soft knee region: quadratic interpolation
            // Smoothly ramps from 0 gain reduction to full ratio
            float x = envDb - threshDb_ + kHalfKneeDb; // 0 to kKneeWidthDb
            float slope = (1.0f - 1.0f / ratio_);
            // Quadratic: gainReduction = slope * x^2 / (2 * kneeWidth)
            gainReductionDb = slope * x * x / (2.0f * kKneeWidthDb);
        }
        else
        {
            // Above knee region: full compression
            float overDb = envDb - threshDb_;
            gainReductionDb = overDb * (1.0f - 1.0f / ratio_);
        }

        return FastMath::fastDbToLin(-gainReductionDb);
    }
};
