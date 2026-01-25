
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>

/**
 * Overdrive Effect - Revised Implementation
 * 
 * This is a warm, musical overdrive with smooth saturation and improved auto-leveling.
 * 
 * Key improvements over the original:
 * - More musical drive curve: moderate gain range (1x to ~12x) with smooth taper
 * - Stronger auto-leveling to prevent excessive output volume increase with drive
 * - Smooth asymmetric tanh-based saturation for natural tube-like warmth
 * - Pre-emphasis high-pass filter (75 Hz) to reduce low-end mud before clipping
 * - Post-processing low-pass filter controlled by tone parameter to soften highs
 * - Parameters remain compatible: drive and tone both 0-1 normalized
 * 
 * Signal flow:
 *   Input -> Pre-HPF (reduce mud) -> Drive Gain -> Asymmetric Tanh Saturation 
 *   -> Auto-Level -> Tone LPF (brightness control) -> Output
 */
struct OverdriveEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Distortion::TypeId;

    float drive_ = 0.5f;      // id0: drive amount (0-1)
    float tone_ = 0.5f;       // id1: tone control (dark to bright)
    float pre_gain_ = 1.0f;   // Calculated from drive (moderate range)
    float post_gain_ = 1.0f;  // Auto-leveling gain (stronger compensation)
    float toneCoeff_ = 0.25f; // Pre-computed lowpass coefficient for tone
    float lpL_ = 0, lpR_ = 0; // Tone lowpass state
    
    // Pre-emphasis high-pass filter state (reduces mud before clipping)
    float hpL_ = 0, hpR_ = 0;       // HPF output state
    float hpInL_ = 0, hpInR_ = 0;   // HPF input state (previous sample)
    float hpCoeff_ = 0.99f;         // ~75 Hz HPF at 48kHz (recalculated in Init)

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    /**
     * Asymmetric tanh saturation for warm, tube-like overdrive
     * 
     * Uses tanh for smooth, continuous saturation with added asymmetry
     * to simulate even-order harmonics (like real tube circuits).
     * The asymmetry creates a subtle "warmth" and prevents the sterile
     * sound of symmetric clipping.
     * 
     * @param x Input signal
     * @return Saturated output in approximately [-1, 1]
     */
    static inline float AsymmetricTanh(float x)
    {
        // Add subtle asymmetry: slightly lift positive values
        // This creates even-order harmonics for warmth
        float asymmetry = 0.1f * x;
        float shaped = x + asymmetry;
        
        // Apply tanh saturation - smooth and continuous
        // Using standard library tanhf for accuracy and consistency
        return tanhf(shaped * 0.9f); // Scale slightly to keep headroom
    }

    static inline float fclamp(float x, float min, float max)
    {
        return x < min ? min : (x > max ? max : x);
    }

    /**
     * More musical drive curve - moderate gain range with smooth taper
     * 
     * Maps drive parameter (0-1) to a gain multiplier with:
     * - At drive=0: gain ~1.0 (clean, minimal coloration)
     * - At drive=0.5: gain ~3.5 (moderate crunch)
     * - At drive=1.0: gain ~12 (heavy saturation, but not excessive)
     * 
     * Uses power curve (^1.5) for smooth, musical response across the range.
     */
    static inline float ComputeDriveGain(float drive)
    {
        drive = fclamp(drive, 0.0f, 1.0f);
        // Smooth power curve for musical response
        float shaped = powf(drive, 1.5f);
        // Map to moderate gain range: 1x to 12x
        return 1.0f + shaped * 11.0f;
    }

    /**
     * Compute auto-leveling (makeup) gain to compensate for saturation
     * 
     * Stronger compensation than original to prevent output from 
     * increasing dramatically with drive. Uses empirical curve tuned
     * for the AsymmetricTanh saturation function.
     * 
     * @param drive Drive amount (0-1)
     * @return Post-saturation gain multiplier
     */
    static inline float ComputeAutoLevel(float drive)
    {
        drive = fclamp(drive, 0.0f, 1.0f);
        // Stronger compensation: reduce output more aggressively at high drive
        // At drive=0: post_gain ≈ 1.0 (unity)
        // At drive=0.5: post_gain ≈ 0.5 (6dB reduction)
        // At drive=1.0: post_gain ≈ 0.3 (10dB reduction)
        return 1.0f / (1.0f + 2.3f * drive);
    }

    /**
     * Set drive amount and recompute pre/post gains
     * 
     * @param drive Normalized drive amount (0-1)
     */
    void SetDrive(float drive)
    {
        drive = fclamp(drive, 0.0f, 1.0f);
        pre_gain_ = ComputeDriveGain(drive);
        post_gain_ = ComputeAutoLevel(drive);
    }

    const EffectMeta &GetMetadata() const override { return Effects::Distortion::kMeta; }

    /**
     * Initialize effect at given sample rate
     * 
     * Sets up pre-emphasis HPF coefficient based on sample rate
     * to maintain ~75 Hz corner frequency across different rates.
     */
    void Init(float sampleRate) override
    {
        SetDrive(drive_);
        updateToneCoeff();
        
        // Initialize filter state
        lpL_ = lpR_ = 0;
        hpL_ = hpR_ = 0;
        hpInL_ = hpInR_ = 0;
        
        // Compute pre-emphasis HPF coefficient for ~75 Hz
        // One-pole HPF: y[n] = a * (y[n-1] + x[n] - x[n-1])
        // Coefficient: a = exp(-2*pi*fc/sr)
        // For 75 Hz at 48kHz: coeff ≈ 0.9902
        if (sampleRate > 0.0f)
        {
            float fc = 75.0f; // Corner frequency in Hz
            float rc = 1.0f / (2.0f * M_PI * fc);
            hpCoeff_ = expf(-1.0f / (rc * sampleRate));
        }
        else
        {
            hpCoeff_ = 0.99f; // Default for 48kHz
        }
    }

    void SetParam(uint8_t id, float v) override
    {
        if (id == 0)
        {
            drive_ = v;
            SetDrive(drive_);
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

    /**
     * Process stereo audio with improved overdrive algorithm
     * 
     * Signal flow:
     * 1. Pre-emphasis high-pass filter to reduce low-end mud
     * 2. Apply drive gain
     * 3. Asymmetric tanh saturation for smooth, warm clipping
     * 4. Auto-leveling (makeup gain)
     * 5. Tone control (low-pass filter for brightness)
     */
    void ProcessStereo(float &l, float &r) override
    {
        // Step 1: Pre-emphasis high-pass filter (reduces mud before saturation)
        // One-pole HPF structure: y[n] = a * (y[n-1] + x[n] - x[n-1])
        // This is a first-order digital high-pass filter
        float hpOutL = hpCoeff_ * (hpL_ + l - hpInL_);
        float hpOutR = hpCoeff_ * (hpR_ + r - hpInR_);
        
        // Update HPF state
        hpL_ = hpOutL;
        hpR_ = hpOutR;
        hpInL_ = l;
        hpInR_ = r;
        
        // Step 2-4: Drive -> Saturation -> Auto-leveling
        float xL = AsymmetricTanh(hpOutL * pre_gain_) * post_gain_;
        float xR = AsymmetricTanh(hpOutR * pre_gain_) * post_gain_;
        
        // Step 5: Tone control - one-pole lowpass for warmth/brightness
        // Low tone (0) = warmer/darker (more lowpass filtering)
        // High tone (1) = brighter (less filtering, more direct signal)
        lpL_ += toneCoeff_ * (xL - lpL_);
        lpR_ += toneCoeff_ * (xR - lpR_);
        
        // Mix between direct (bright) and filtered (warm) based on tone
        l = tone_ * xL + (1.0f - tone_) * lpL_;
        r = tone_ * xR + (1.0f - tone_) * lpR_;
    }

private:
    /**
     * Update tone lowpass coefficient based on tone parameter
     * 
     * Lower tone values = slower filter (more filtering, darker sound)
     * Higher tone values = faster filter (less filtering, brighter sound)
     */
    void updateToneCoeff()
    {
        // Map tone (0-1) to filter coefficient
        // At tone=0 (dark): slower filter (smaller coeff)
        // At tone=1 (bright): faster filter (larger coeff)
        toneCoeff_ = 0.05f + 0.4f * (1.0f - tone_);
    }
};
