
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"

struct TremoloEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Tremolo::TypeId;

    // Raw parameter values (0..1 normalized)
    float rate_ = 0.034f;   // id0: rate normalized (default ~1 Hz)
    float depth_ = 0.5f;    // id1: depth normalized
    float shape_ = 0.0f;    // id2: shape normalized (0=sine, 0.5=tri, 1=square)
    float stereo_ = 0.0f;   // id3: stereo mode normalized (0=mono, 1=stereo)

    // Pre-computed state
    float lfoPhase_ = 0.0f;
    float lfoInc_ = 0.0f;
    float sampleRate_ = 48000.0f;

    const EffectMeta &GetMetadata() const override { return Effects::Tremolo::kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        lfoPhase_ = 0.0f;
        updateLfoInc();
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: // Rate: 0..1 maps to 0.5-15.0 Hz
            rate_ = v;
            updateLfoInc();
            break;
        case 1: // Depth: 0..1 maps to 0-100%
            depth_ = v;
            break;
        case 2: // Shape: enum (0=sine, ~0.5=triangle, ~1.0=square)
            shape_ = v;
            break;
        case 3: // Stereo: enum (0=mono, ~1.0=stereo)
            stereo_ = v;
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 4)
            return 0;
        out[0] = {0, (uint8_t)(rate_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(depth_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(shape_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(stereo_ * 127.0f + 0.5f)};
        return 4;
    }

    void ProcessStereo(float &l, float &r) override
#if !defined(DAISY_SEED_BUILD)
    {
        // Advance LFO phase
        lfoPhase_ += lfoInc_;
        if (lfoPhase_ >= 1.0f)
            lfoPhase_ -= 1.0f;

        // Compute LFO value (0..1)
        float lfo = computeLfo(lfoPhase_);

        // Compute gain: 1.0 at lfo=0 (no attenuation), (1-depth) at lfo=1 (max attenuation)
        float gainL = 1.0f - depth_ * lfo;

        if (stereo_ > 0.5f)
        {
            // Stereo mode: opposite-phase LFO for right channel
            float phaseR = lfoPhase_ + 0.5f;
            if (phaseR >= 1.0f)
                phaseR -= 1.0f;
            float lfoR = computeLfo(phaseR);
            float gainR = 1.0f - depth_ * lfoR;
            l *= gainL;
            r *= gainR;
        }
        else
        {
            // Mono mode: same modulation on both channels
            l *= gainL;
            r *= gainL;
        }
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif

private:
    void updateLfoInc()
    {
        float rateHz = 0.5f + rate_ * 14.5f; // 0.5 to 15.0 Hz
        lfoInc_ = rateHz / sampleRate_;
    }

    inline float computeLfo(float phase) const
    {
        // Determine shape from normalized value
        // shape_ ~0.0 = sine, ~0.5 = triangle, ~1.0 = square
        if (shape_ < 0.33f)
        {
            // Sine: 0..1 range (half-wave rectified sine shape for tremolo)
            return 0.5f * (1.0f - FastMath::fastCos(phase));
        }
        else if (shape_ < 0.67f)
        {
            // Triangle: 0..1 range
            return (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
        }
        else
        {
            // Square: 0 or 1
            return (phase < 0.5f) ? 0.0f : 1.0f;
        }
    }
};
