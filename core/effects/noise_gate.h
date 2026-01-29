#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"
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
    static constexpr uint8_t TypeId = Effects::NoiseGate::TypeId;

    // Normalized parameter values
    float threshold_ = 0.3f; // id0: threshold (0..1 maps to -80dB to -20dB)
    float attack_ = 0.001f;  // id1: attack time in seconds
    float hold_ = 0.1f;      // id2: hold time in seconds
    float release_ = 0.1f;   // id3: release time in seconds
    float range_ = 0.0f;     // id4: range/floor (0 = full cut, 1 = no cut)

    // Pre-computed coefficients
    float threshLin_ = 0.001f; // Threshold as linear amplitude
    float attackCoef_ = 0.0f;  // Attack envelope coefficient
    float releaseCoef_ = 0.0f; // Release envelope coefficient

    // Gate state
    float gateGain_ = 0.0f;    // Current gate gain (0 = closed, 1 = open)
    float holdCounter_ = 0.0f; // Samples remaining in hold phase
    float sampleRate_ = 48000.0f;

    const EffectMeta &GetMetadata() const override { return Effects::NoiseGate::kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        gateGain_ = 0.0f;
        holdCounter_ = 0.0f;
        updateCoefficients();
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: // Threshold: 0..1 maps to -80dB to -20dB
            threshold_ = v;
            {
                float threshDb = -80.0f + threshold_ * 60.0f;
                threshLin_ = FastMath::fastDbToLin(threshDb);
            }
            break;
        case 1: // Attack: 0..1 maps to 0.1ms to 50ms
            attack_ = 0.0001f + v * 0.0499f;
            attackCoef_ = FastMath::calcEnvelopeCoeff(attack_, sampleRate_);
            break;
        case 2: // Hold: 0..1 maps to 10ms to 500ms
            hold_ = 0.01f + v * 0.49f;
            break;
        case 3: // Release: 0..1 maps to 10ms to 500ms
            release_ = 0.01f + v * 0.49f;
            releaseCoef_ = FastMath::calcEnvelopeCoeff(release_, sampleRate_);
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
#if !defined(DAISY_SEED_BUILD)
    {
        float inputLevel = FastMath::fmax(FastMath::fabs(l), FastMath::fabs(r));

        if (inputLevel > threshLin_)
        {
            holdCounter_ = hold_ * sampleRate_;
            gateGain_ = attackCoef_ * gateGain_ + (1.0f - attackCoef_) * 1.0f;
        }
        else if (holdCounter_ > 0.0f)
        {
            holdCounter_ -= 1.0f;
        }
        else
        {
            gateGain_ = releaseCoef_ * gateGain_;
        }

        float effectiveGain = range_ + (1.0f - range_) * gateGain_;
        l *= effectiveGain;
        r *= effectiveGain;
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif

private:
    // Recompute all coefficients (called from Init)
    void updateCoefficients()
    {
        float threshDb = -80.0f + threshold_ * 60.0f;
        threshLin_ = FastMath::fastDbToLin(threshDb);
        attackCoef_ = FastMath::calcEnvelopeCoeff(attack_, sampleRate_);
        releaseCoef_ = FastMath::calcEnvelopeCoeff(release_, sampleRate_);
    }
};
