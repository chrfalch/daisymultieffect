
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/filters/biquad.h"
#include <cmath>

// Classic wah-wah pedal effect using swept bandpass filter
// Simulates the vocal "wah" sound made famous by pedals like Cry Baby
struct WahEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Wah::TypeId;

    // Raw parameter values (0..1 normalized)
    float position_ = 0.5f; // id0: wah position (0=low freq, 1=high freq)

    // Filter state
    Biquad filterL_;
    Biquad filterR_;
    
    float sampleRate_ = 48000.0f;
    float currentFreq_ = 500.0f; // Current center frequency

    const EffectMeta &GetMetadata() const override { return Effects::Wah::kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        filterL_.Reset();
        filterR_.Reset();
        updateFilter();
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: // Position: 0..1 maps to low to high frequency
            position_ = v;
            updateFilter();
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 1)
            return 0;
        out[0] = {0, (uint8_t)(position_ * 127.0f + 0.5f)};
        return 1;
    }

    void ProcessStereo(float &l, float &r) override
#if !defined(DAISY_SEED_BUILD)
    {
        // Process through bandpass filter
        l = filterL_.Process(l);
        r = filterR_.Process(r);
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif

private:
    // Update filter coefficients based on position parameter
    void updateFilter()
    {
        // Classic wah frequency range: 400Hz to 2kHz
        // This range emphasizes the vocal formant frequencies
        const float minFreq = 400.0f;
        const float maxFreq = 2000.0f;
        
        // Use exponential mapping for more natural feel
        // position_ = 0 -> 400Hz, position_ = 1 -> 2000Hz
        currentFreq_ = minFreq * std::pow(maxFreq / minFreq, position_);
        
        // Set bandpass filter with high Q for wah characteristic
        // Q of 12 gives the classic wah resonance
        SetBandpass(currentFreq_, 12.0f, sampleRate_);
    }

    // Configure as bandpass filter with adjustable Q
    // Based on RBJ Audio EQ Cookbook formulas
    // Note: Uses local pi constant for consistency with Biquad's SetLowpass/SetHighpass methods
    void SetBandpass(float fc, float Q, float fs)
    {
        const float pi = 3.14159265358979323846f;
        float w0 = 2.0f * pi * fc / fs;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * Q);

        // Bandpass filter (constant skirt gain, peak gain = Q)
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosw0;
        float a2 = 1.0f - alpha;
        float b0 = alpha;
        float b1 = 0.0f;
        float b2 = -alpha;

        // Normalize by a0 and set coefficients
        float a0inv = 1.0f / a0;
        filterL_.b0_ = b0 * a0inv;
        filterL_.b1_ = b1 * a0inv;
        filterL_.b2_ = b2 * a0inv;
        filterL_.a1_ = a1 * a0inv;
        filterL_.a2_ = a2 * a0inv;

        // Copy coefficients to right channel filter
        filterR_.b0_ = filterL_.b0_;
        filterR_.b1_ = filterL_.b1_;
        filterR_.b2_ = filterL_.b2_;
        filterR_.a1_ = filterL_.a1_;
        filterR_.a2_ = filterL_.a2_;
    }
};
