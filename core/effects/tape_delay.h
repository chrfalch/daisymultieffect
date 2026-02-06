
#pragma once
#include "effects/time_synced_effect.h"
#include "effects/effect_metadata.h"
#include "effects/tape_flutter.h"

/**
 * Tape Delay Effect
 * 
 * Delay with analog tape wow and flutter modulation for vintage character.
 * Uses TapeFlutter utility to modulate delay time with smooth Perlin noise.
 */
struct TapeDelayEffect : TimeSyncedEffect
{
    static constexpr uint8_t TypeId = Effects::TapeDelay::TypeId;
    static constexpr int MAX_SAMPLES = 48000 * 4;

    float *bufL_ = nullptr;
    float *bufR_ = nullptr;
    int wp = 0;

    float feedback_ = 0.4f;     // id3: Feedback amount
    float mix_ = 0.5f;          // id4: Wet/dry mix
    float tapeCharacter_ = 0.5f; // id5: Tape character (controls wow/flutter depth)
    
    TapeFlutter tapeFlutter_;   // Tape modulation engine

    const EffectMeta &GetMetadata() const override { return Effects::TapeDelay::kMeta; }

    TapeDelayEffect(TempoSource &t) : TimeSyncedEffect(t) {}

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
        tapeFlutter_.Init(sr);
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
        else if (id == 5)
            tapeCharacter_ = v;
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 6)
            return 0;
        uint8_t n = TimeSyncedEffect::GetParamsSnapshot(out, max);
        out[n++] = {3, (uint8_t)(feedback_ / 0.95f * 127 + 0.5f)};
        out[n++] = {4, (uint8_t)(mix_ * 127 + 0.5f)};
        out[n++] = {5, (uint8_t)(tapeCharacter_ * 127 + 0.5f)};
        return n;
    }

    void ProcessStereo(float &l, float &r) override
#if !defined(DAISY_SEED_BUILD)
    {
        if (!bufL_ || !bufR_)
            return;

        // Get base delay time from tempo sync
        int baseDelaySamples = GetPeriodSamples();
        if (baseDelaySamples >= MAX_SAMPLES)
            baseDelaySamples = MAX_SAMPLES - 1;

        // Get tape flutter modulation
        // Map character to wow/flutter depths (0 = no modulation, 1 = full character)
        float wowDepth = tapeCharacter_ * 1.5f;      // 0-1.5
        float flutterDepth = tapeCharacter_ * 0.75f; // 0-0.75
        
        float speedMod = tapeFlutter_.GetTapeSpeed(
            0.5f,          // wow_rate: 0.5 Hz (slow)
            3.0f,          // flutter_rate: 3 Hz (fast)
            wowDepth,      // wow_depth scaled by character
            flutterDepth   // flutter_depth scaled by character
        );

        // Apply modulation to delay time
        // Scale modulation amount based on delay time and character
        // Longer delays can handle more modulation
        float modulationAmount = (baseDelaySamples * 0.02f) * tapeCharacter_;
        float modulatedDelay = baseDelaySamples + speedMod * modulationAmount;

        // Clamp to valid range
        if (modulatedDelay < 1.0f)
            modulatedDelay = 1.0f;
        if (modulatedDelay >= MAX_SAMPLES - 1)
            modulatedDelay = MAX_SAMPLES - 2;

        // Read from delay buffer with linear interpolation for fractional delay
        int rp0 = wp - (int)modulatedDelay;
        if (rp0 < 0)
            rp0 += MAX_SAMPLES;
        int rp1 = (rp0 + 1) % MAX_SAMPLES;
        
        float frac = modulatedDelay - (int)modulatedDelay;
        float dl = bufL_[rp0] * (1.0f - frac) + bufL_[rp1] * frac;
        float dr = bufR_[rp0] * (1.0f - frac) + bufR_[rp1] * frac;

        // Process input and feedback
        float inL = l, inR = r;
        bufL_[wp] = inL + dl * feedback_;
        bufR_[wp] = inR + dr * feedback_;
        
        if (++wp >= MAX_SAMPLES)
            wp = 0;

        // Output mix
        float dry = 1.0f - mix_, wet = mix_;
        l = inL * dry + dl * wet;
        r = inR * dry + dr * wet;
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif
};
