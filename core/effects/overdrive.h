
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"

/**
 * Warm Overdrive Effect
 *
 * Classic tube-style overdrive with smooth saturation characteristics.
 * Features:
 *   - Fixed 80Hz pre-highpass to tighten bass before clipping
 *   - Gentle gain curve (max ~4x) for musical saturation
 *   - Mutable Instruments SoftLimit clipper (smooth Padé tanh approximation)
 *   - Sample-rate-aware post-lowpass for warmth/brightness control
 *   - Strong post-gain compensation for consistent output level
 */
struct OverdriveEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Distortion::TypeId;
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kPreHpFreq = 80.0f;   // Fixed pre-HP cutoff (Hz)
    static constexpr float kLpMinFreq = 3000.0f; // Tone LP minimum (Hz)
    static constexpr float kLpMaxFreq = 12000.0f; // Tone LP maximum (Hz)

    // Parameters (0-1 range)
    float drive_ = 0.5f; // id0: drive amount
    float tone_ = 0.5f;  // id1: tone (dark to bright)

    // Computed gains
    float preGain_ = 1.0f;
    float postGain_ = 1.0f;

    // Filter coefficients (precomputed)
    float hpCoeff_ = 0.0f; // Pre-HP coefficient
    float lpCoeff_ = 0.0f; // Post-LP coefficient

    // Filter states
    float hpL_ = 0.0f, hpR_ = 0.0f; // Pre-HP state
    float lpL_ = 0.0f, lpR_ = 0.0f; // Post-LP state

    // Sample rate
    float sampleRate_ = 48000.0f;

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }
    const EffectMeta &GetMetadata() const override { return Effects::Distortion::kMeta; }

    // ─────────────────────────────────────────────────────────────────────────
    // Fast math utilities (inlined for performance)
    // ─────────────────────────────────────────────────────────────────────────

    static inline float fclamp(float x, float lo, float hi)
    {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    // Mutable Instruments SoftLimit - smooth tanh-like saturation
    // Padé approximant: approaches ±1 asymptotically, never clips hard
    static inline float SoftLimit(float x)
    {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // Soft clipper with extended headroom (no hard ceiling)
    // Clamps input to ±6 to avoid numerical issues, then applies SoftLimit
    static inline float SoftClip(float x)
    {
        x = fclamp(x, -6.0f, 6.0f);
        return SoftLimit(x);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Gain staging
    // ─────────────────────────────────────────────────────────────────────────

    void updateGains()
    {
        // Gentle quadratic taper on drive parameter
        float d = drive_ * drive_; // 0 to 1, quadratic curve

        // Pre-gain: 1.0 to 30.0 (full overdrive range)
        // At drive=0: preGain=1 (clean), at drive=1: preGain=30 (heavy saturation)
        preGain_ = 1.0f + d * 29.0f;

        // Post-gain compensation: scales inversely with preGain
        postGain_ = 2.0f / (0.5f + 0.5f * preGain_);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Filter coefficient calculations
    // ─────────────────────────────────────────────────────────────────────────

    void updateHpCoeff()
    {
        // One-pole highpass coefficient for fixed 80Hz
        // coeff = 2π * fc / sr (clamped to prevent instability)
        float c = 2.0f * kPi * kPreHpFreq / sampleRate_;
        hpCoeff_ = fclamp(c, 0.0001f, 0.5f);
    }

    void updateLpCoeff()
    {
        // One-pole lowpass coefficient, frequency controlled by tone
        // tone=0: 3kHz (warm), tone=1: 12kHz (bright)
        float freq = kLpMinFreq + tone_ * (kLpMaxFreq - kLpMinFreq);
        float c = 2.0f * kPi * freq / sampleRate_;
        lpCoeff_ = fclamp(c, 0.0001f, 0.99f);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Effect interface
    // ─────────────────────────────────────────────────────────────────────────

    void Init(float sampleRate) override
    {
        sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
        updateGains();
        updateHpCoeff();
        updateLpCoeff();
        hpL_ = hpR_ = 0.0f;
        lpL_ = lpR_ = 0.0f;
    }

    void SetParam(uint8_t id, float v) override
    {
        v = fclamp(v, 0.0f, 1.0f);
        if (id == 0)
        {
            drive_ = v;
            updateGains();
        }
        else if (id == 1)
        {
            tone_ = v;
            updateLpCoeff();
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 2)
            return 0;
        out[0] = {0, static_cast<uint8_t>(drive_ * 127.0f + 0.5f)};
        out[1] = {1, static_cast<uint8_t>(tone_ * 127.0f + 0.5f)};
        return 2;
    }

    void ProcessStereo(float &l, float &r) override
#if !defined(DAISY_SEED_BUILD)
    {
        hpL_ += hpCoeff_ * (l - hpL_);
        hpR_ += hpCoeff_ * (r - hpR_);
        float hpOutL = l - hpL_;
        float hpOutR = r - hpR_;

        float clipL = SoftClip(hpOutL * preGain_) * postGain_;
        float clipR = SoftClip(hpOutR * preGain_) * postGain_;

        lpL_ += lpCoeff_ * (clipL - lpL_);
        lpR_ += lpCoeff_ * (clipR - lpR_);

        l = lpL_;
        r = lpR_;
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif
};
