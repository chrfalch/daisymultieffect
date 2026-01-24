
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>

/**
 * Overdrive Effect - Revised Implementation
 * 
 * This overdrive uses a musical gain curve, strong auto-leveling, and asymmetric
 * tanh-based saturation for warmth. Pre-emphasis high-pass filtering reduces
 * low-end mud before clipping, and improved post-tone filtering shapes the output.
 * 
 * Key improvements:
 * - Moderate pre-gain curve (1.0x to ~10x) instead of steep exponential
 * - Stronger auto-leveling to prevent excessive output volume with drive
 * - Asymmetric tanh saturation for smoother, warmer clipping character
 * - Pre-emphasis HPF (80Hz) to reduce bass mud before saturation
 * - Improved tone control with better frequency response
 */
struct OverdriveEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Distortion::TypeId;

    float drive_ = 0.5f;      // id0: drive amount (0-1)
    float tone_ = 0.5f;       // id1: tone control (dark to bright)
    float pre_gain_ = 1.0f;   // Calculated from drive
    float post_gain_ = 1.0f;  // Auto-leveling gain
    float toneCoeff_ = 0.25f; // Pre-computed lowpass coefficient
    float lpL_ = 0, lpR_ = 0; // Lowpass filter state for tone
    
    // Pre-emphasis high-pass filter state (80Hz @ 48kHz)
    // Coefficient derived from: coeff = (RC)/(RC + 1/fs) where RC = 1/(2*pi*fc)
    // For fc=80Hz, fs=48kHz: RC ≈ 0.001989, coeff ≈ 0.9895
    float hpInL_ = 0, hpInR_ = 0;
    float hpOutL_ = 0, hpOutR_ = 0;
    static constexpr float kHpCoeff = 0.9895f; // ~80Hz cutoff
    
    // Drive and leveling constants
    static constexpr float kMaxDriveGain = 9.0f;      // Max boost: 1 + 9 = 10x at full drive
    static constexpr float kLevelingStrength = 2.5f;  // Auto-leveling compensation factor
    
    // Asymmetric saturation constants
    static constexpr float kAsymmetryBias = 0.05f;    // 2nd harmonic bias for warmth
    static constexpr float kSaturationScale = 0.98f;  // Pre-compression to avoid hard limits
    
    // Tone control constants
    static constexpr float kToneMinCoeff = 0.08f;     // Maximum filtering (darkest)
    static constexpr float kToneRange = 0.35f;        // Tone control range

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    // Fast tanh approximation for smooth saturation
    // More CPU-friendly than std::tanh while maintaining smooth character
    static inline float FastTanh(float x)
    {
        if (x < -3.0f)
            return -1.0f;
        else if (x > 3.0f)
            return 1.0f;
        
        // Polynomial approximation of tanh in [-3, 3]
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // Asymmetric soft clipping for warmth (simulates tube-like behavior)
    // Positive half clips softer than negative for even-harmonic content
    static inline float AsymmetricSaturation(float x)
    {
        // Apply slight bias and scale to create asymmetry
        float biased = x + kAsymmetryBias * x * x;  // Adds subtle 2nd harmonic
        return FastTanh(biased * kSaturationScale); // Slightly pre-compress to avoid hard limits
    }

    static inline float fclamp(float x, float min, float max)
    {
        return x < min ? min : (x > max ? max : x);
    }

    // Parameter taper: gentle curve to make drive more musical across range
    // Maps UI drive (0-1) to a smoother, less aggressive response
    static inline float ApplyDriveTaper(float drive)
    {
        drive = fclamp(drive, 0.0f, 1.0f);
        // Gentle power curve - not as steep as before
        return powf(drive, 1.5f);
    }

    // Compute gains for overdrive processing
    // New approach: moderate gain range with strong auto-leveling
    void SetDrive(float drive)
    {
        drive = fclamp(drive, 0.0f, 1.0f);
        
        // Pre-gain: moderate range from 1.0 (unity) to ~10.0 (heavy drive)
        // Much more musical than the previous d^5 * 24 exponential curve
        // At drive=0: pre_gain = 1.0 (clean passthrough)
        // At drive=1: pre_gain = 1 + kMaxDriveGain = 10.0 (strong but not excessive)
        pre_gain_ = 1.0f + drive * drive * kMaxDriveGain;  // Quadratic: smoother ramp
        
        // Strong auto-leveling: compensate for increased RMS from saturation
        // At low drive: near unity (clean signal passes through)
        // At high drive: aggressively reduce output to prevent volume jump
        // Factor accounts for both clipping compression and harmonic content
        float levelingFactor = 1.0f + drive * drive * kLevelingStrength;
        post_gain_ = 1.0f / levelingFactor;  // Ranges from 1.0 to ~0.28
    }

    const EffectMeta &GetMetadata() const override { return Effects::Distortion::kMeta; }

    void Init(float) override
    {
        SetDrive(ApplyDriveTaper(drive_));
        updateToneCoeff();
        lpL_ = lpR_ = 0;
        hpInL_ = hpInR_ = 0;
        hpOutL_ = hpOutR_ = 0;
    }

    void SetParam(uint8_t id, float v) override
    {
        if (id == 0)
        {
            drive_ = v;
            SetDrive(ApplyDriveTaper(drive_));
        }
        else if (id == 1)
        {
            tone_ = v;
            updateToneCoeff();
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 2)
            return 0;
        out[0] = {0, (uint8_t)(drive_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(tone_ * 127.0f + 0.5f)};
        return 2;
    }

    void ProcessStereo(float &l, float &r) override
    {
        // Pre-emphasis high-pass filter: reduce low-end mud before saturation
        // Simple first-order HPF at ~80Hz to tighten bass without losing body
        // Formula: y[n] = coeff * (y[n-1] + x[n] - x[n-1])
        float hpL = kHpCoeff * (hpOutL_ + l - hpInL_);
        float hpR = kHpCoeff * (hpOutR_ + r - hpInR_);
        hpInL_ = l;
        hpInR_ = r;
        hpOutL_ = hpL;
        hpOutR_ = hpR;
        
        // Apply drive gain and asymmetric saturation
        // Asymmetry adds warmth through even-order harmonics
        float xL = AsymmetricSaturation(hpL * pre_gain_) * post_gain_;
        float xR = AsymmetricSaturation(hpR * pre_gain_) * post_gain_;

        // Tone control: one-pole lowpass for warmth shaping
        // Low tone = warmer (more lowpass), high tone = brighter (more direct)
        lpL_ += toneCoeff_ * (xL - lpL_);
        lpR_ += toneCoeff_ * (xR - lpR_);

        // Mix between filtered (warm) and direct (bright) based on tone
        l = tone_ * xL + (1.0f - tone_) * lpL_;
        r = tone_ * xR + (1.0f - tone_) * lpR_;
    }

private:
    void updateToneCoeff()
    {
        // Improved tone mapping: better frequency response control
        // Lower values = more filtering (darker/warmer)
        // Higher values = less filtering (brighter)
        toneCoeff_ = kToneMinCoeff + kToneRange * (1.0f - tone_);
    }
};
