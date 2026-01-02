#pragma once
#include "effects/base_effect.h"
#include <cmath>
#include <algorithm>

/**
 * NoiseGateEffect - Simple noise gate for cutting hum/buzz when not playing.
 *
 * Parameters:
 * - Threshold (0): Level below which audio is cut (-80dB to -20dB)
 * - Attack (1): How fast the gate opens (0.1ms to 50ms)
 * - Hold (2): How long to keep gate open after signal drops (10ms to 500ms)
 * - Release (3): How fast the gate closes (10ms to 500ms)
 * - Range (4): How much to attenuate when closed (0dB to -80dB, 0=full cut)
 */
struct NoiseGateEffect : BaseEffect
{
    static constexpr uint8_t TypeId = 17;

    float threshold_ = 0.3f; // id0: threshold (0..1 maps to -80dB to -20dB)
    float attack_ = 0.001f;  // id1: attack time in seconds
    float hold_ = 0.1f;      // id2: hold time in seconds
    float release_ = 0.1f;   // id3: release time in seconds
    float range_ = 0.0f;     // id4: range/floor (0 = full cut, 1 = no cut)

    // Gate state
    float gateGain_ = 0.0f;    // Current gate gain (0 = closed, 1 = open)
    float holdCounter_ = 0.0f; // Samples remaining in hold phase
    float sampleRate_ = 48000.0f;

    static const NumberParamRange kThreshRange, kAttackRange, kHoldRange, kReleaseRange, kRangeRange;
    static const ParamInfo kParams[5];
    static const EffectMeta kMeta;
    const EffectMeta &GetMetadata() const override { return kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        gateGain_ = 0.0f;
        holdCounter_ = 0.0f;
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: // Threshold: 0..1 maps to -80dB to -20dB
            threshold_ = v;
            break;
        case 1: // Attack: 0..1 maps to 0.1ms to 50ms
            attack_ = 0.0001f + v * 0.0499f;
            break;
        case 2: // Hold: 0..1 maps to 10ms to 500ms
            hold_ = 0.01f + v * 0.49f;
            break;
        case 3: // Release: 0..1 maps to 10ms to 500ms
            release_ = 0.01f + v * 0.49f;
            break;
        case 4: // Range: 0..1 (0 = full cut -inf dB, 1 = 0dB no attenuation)
            range_ = v;
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 5)
            return 0;
        out[0] = {0, (uint8_t)(threshold_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(((attack_ - 0.0001f) / 0.0499f) * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(((hold_ - 0.01f) / 0.49f) * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(((release_ - 0.01f) / 0.49f) * 127.0f + 0.5f)};
        out[4] = {4, (uint8_t)(range_ * 127.0f + 0.5f)};
        return 5;
    }

    void ProcessStereo(float &l, float &r) override
    {
        // Convert threshold from 0..1 to linear amplitude
        // 0 = -80dB, 1 = -20dB
        float threshDb = -80.0f + threshold_ * 60.0f;
        float threshLin = std::pow(10.0f, threshDb / 20.0f);

        // Get input level (max of L and R)
        float inputLevel = std::max(std::fabs(l), std::fabs(r));

        // Gate logic
        if (inputLevel > threshLin)
        {
            // Signal above threshold - open gate
            holdCounter_ = hold_ * sampleRate_;

            // Attack - smoothly open
            float attackCoef = std::exp(-1.0f / (attack_ * sampleRate_));
            gateGain_ = attackCoef * gateGain_ + (1.0f - attackCoef) * 1.0f;
        }
        else if (holdCounter_ > 0.0f)
        {
            // In hold phase - keep gate open
            holdCounter_ -= 1.0f;
            // Keep gateGain_ at current level (near 1.0)
        }
        else
        {
            // Release - smoothly close
            float releaseCoef = std::exp(-1.0f / (release_ * sampleRate_));
            gateGain_ = releaseCoef * gateGain_;
        }

        // Apply gate with range floor
        // range_ = 0 means full cut (multiply by gateGain_)
        // range_ = 1 means no cut (multiply by 1.0)
        float effectiveGain = range_ + (1.0f - range_) * gateGain_;

        l *= effectiveGain;
        r *= effectiveGain;
    }
};

// Static metadata definitions
inline const NumberParamRange NoiseGateEffect::kThreshRange = {-80.0f, -20.0f, 0.1f};
inline const NumberParamRange NoiseGateEffect::kAttackRange = {0.1f, 50.0f, 0.1f};
inline const NumberParamRange NoiseGateEffect::kHoldRange = {10.0f, 500.0f, 1.0f};
inline const NumberParamRange NoiseGateEffect::kReleaseRange = {10.0f, 500.0f, 1.0f};
inline const NumberParamRange NoiseGateEffect::kRangeRange = {0.0f, 1.0f, 0.01f};

inline const ParamInfo NoiseGateEffect::kParams[5] = {
    {0, "Threshold", "Gate open level", ParamValueKind::Number, &NoiseGateEffect::kThreshRange, nullptr},
    {1, "Attack", "Gate open speed", ParamValueKind::Number, &NoiseGateEffect::kAttackRange, nullptr},
    {2, "Hold", "Hold time after signal", ParamValueKind::Number, &NoiseGateEffect::kHoldRange, nullptr},
    {3, "Release", "Gate close speed", ParamValueKind::Number, &NoiseGateEffect::kReleaseRange, nullptr},
    {4, "Range", "Floor level when closed", ParamValueKind::Number, &NoiseGateEffect::kRangeRange, nullptr},
};

inline const EffectMeta NoiseGateEffect::kMeta = {
    "Noise Gate",
    "Cut signal below threshold to eliminate hum/buzz.",
    NoiseGateEffect::kParams,
    5};
