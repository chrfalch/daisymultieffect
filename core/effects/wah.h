
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/filters/biquad.h"
#include "effects/fast_math.h"
#include <cmath>

// Classic wah-wah pedal effect using swept bandpass filter
// Simulates the vocal "wah" sound made famous by pedals like Cry Baby
// Features: adjustable Q-factor and envelope follower (auto-wah)
struct WahEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Wah::TypeId;

    // Raw parameter values (0..1 normalized)
    float position_ = 0.5f;    // id0: wah position (0=low freq, 1=high freq)
    float qFactor_ = 0.5f;     // id1: Q factor normalized (0=mild, 1=extreme)
    float mode_ = 0.0f;        // id2: mode (0=manual, 1=auto/envelope)
    float attack_ = 0.02f;     // id3: envelope attack time normalized
    float release_ = 0.2f;     // id4: envelope release time normalized
    float sensitivity_ = 0.5f; // id5: envelope sensitivity normalized

    // Filter state
    Biquad filterL_;
    Biquad filterR_;
    
    // Envelope follower state
    float envelope_ = 0.0f;
    float attackCoef_ = 0.0f;
    float releaseCoef_ = 0.0f;
    
    float sampleRate_ = 48000.0f;
    float currentFreq_ = 500.0f; // Current center frequency
    float currentQ_ = 12.0f;     // Current Q factor

    const EffectMeta &GetMetadata() const override { return Effects::Wah::kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        envelope_ = 0.0f;
        filterL_.Reset();
        filterR_.Reset();
        updateEnvelopeCoefficients();
        updateFilter();
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: // Position: 0..1 maps to low to high frequency
            position_ = v;
            if (mode_ < 0.5f) // Manual mode
                updateFilter();
            break;
        case 1: // Q Factor: 0..1 maps to mild to extreme resonance
            qFactor_ = v;
            updateFilter();
            break;
        case 2: // Mode: 0=manual, 1=auto/envelope
            mode_ = v;
            break;
        case 3: // Attack: envelope follower attack time
            attack_ = v;
            updateEnvelopeCoefficients();
            break;
        case 4: // Release: envelope follower release time
            release_ = v;
            updateEnvelopeCoefficients();
            break;
        case 5: // Sensitivity: envelope to position mapping
            sensitivity_ = v;
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 6)
            return 0;
        out[0] = {0, (uint8_t)(position_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(qFactor_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(mode_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(attack_ * 127.0f + 0.5f)};
        out[4] = {4, (uint8_t)(release_ * 127.0f + 0.5f)};
        out[5] = {5, (uint8_t)(sensitivity_ * 127.0f + 0.5f)};
        return 6;
    }

    void ProcessStereo(float &l, float &r) override
#if !defined(DAISY_SEED_BUILD)
    {
        // Auto-wah mode: derive position from input envelope
        if (mode_ > 0.5f)
        {
            // Envelope follower (peak detection)
            float inputLevel = FastMath::fmax(FastMath::fabs(l), FastMath::fabs(r));
            
            if (inputLevel > envelope_)
            {
                envelope_ = attackCoef_ * envelope_ + (1.0f - attackCoef_) * inputLevel;
            }
            else
            {
                envelope_ = releaseCoef_ * envelope_ + (1.0f - releaseCoef_) * inputLevel;
            }
            
            // Map envelope to position with sensitivity control
            // sensitivity_ controls how much envelope affects position
            float envelopePosition = envelope_ * sensitivity_ * 10.0f; // Scale up for typical guitar levels
            envelopePosition = FastMath::fclamp(envelopePosition, 0.0f, 1.0f);
            
            // Update filter if position changed significantly (avoid unnecessary updates)
            float positionDelta = FastMath::fabs(envelopePosition - position_);
            if (positionDelta > 0.001f)
            {
                position_ = envelopePosition;
                updateFilter();
            }
        }
        
        // Process through bandpass filter
        l = filterL_.Process(l);
        r = filterR_.Process(r);
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif

private:
    // Update envelope follower coefficients
    void updateEnvelopeCoefficients()
    {
        // Attack: 1ms to 100ms
        float attackTime = 0.001f + attack_ * 0.099f;
        attackCoef_ = FastMath::calcEnvelopeCoeff(attackTime, sampleRate_);
        
        // Release: 10ms to 500ms
        float releaseTime = 0.01f + release_ * 0.49f;
        releaseCoef_ = FastMath::calcEnvelopeCoeff(releaseTime, sampleRate_);
    }

    // Update filter coefficients based on position and Q parameters
    void updateFilter()
    {
        // Classic wah frequency range: 400Hz to 2kHz
        // This range emphasizes the vocal formant frequencies
        const float minFreq = 400.0f;
        const float maxFreq = 2000.0f;
        
        // Use exponential mapping for more natural feel
        // position_ = 0 -> 400Hz, position_ = 1 -> 2000Hz
        currentFreq_ = minFreq * std::pow(maxFreq / minFreq, position_);
        
        // Q factor: 4 (mild) to 20 (extreme resonance)
        // Default middle value is ~12 (classic wah sound)
        currentQ_ = 4.0f + qFactor_ * 16.0f;
        
        // Set bandpass filter with variable Q
        SetBandpass(currentFreq_, currentQ_, sampleRate_);
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
