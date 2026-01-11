
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>

/**
 * Overdrive Effect
 * DaisySP/Mutable Instruments Plaits overdrive core.
 * Note: We apply a gentle taper to the UI drive parameter before feeding the
 * original DaisySP SetDrive curve. This keeps maximum drive available, but
 * makes mid-drive less aggressive for typical line-level / full-scale inputs.
 */
struct OverdriveEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Distortion::TypeId;

    float drive_ = 0.5f;     // id0: drive amount (0-1)
    float tone_ = 0.5f;      // id1: tone control (dark to bright)
    float pre_gain_ = 1.0f;  // Calculated from drive
    float post_gain_ = 1.0f; // Auto-leveling gain
    float lpL_ = 0, lpR_ = 0;

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    // Soft limiting function from Mutable Instruments stmlib
    static inline float SoftLimit(float x)
    {
        return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
    }

    // Soft clipping - more musical than tanh
    static inline float SoftClip(float x)
    {
        if (x < -3.0f)
            return -1.0f;
        else if (x > 3.0f)
            return 1.0f;
        else
            return SoftLimit(x);
    }

    static inline float fclamp(float x, float min, float max)
    {
        return x < min ? min : (x > max ? max : x);
    }

    // Parameter taper: concave curve to slow the mid-drive ramp while keeping full scale.
    static inline float ApplyDriveTaper(float drive)
    {
        drive = fclamp(drive, 0.0f, 1.0f);
        return powf(drive, 1.8f);
    }

    // Exact DaisySP SetDrive implementation
    void SetDrive(float drive)
    {
        drive = fclamp(drive, 0.0f, 1.0f);
        float d = 1.6f * drive; // slightly reduce pre-gain build-up vs stock
        d = fclamp(d, 0.0f, 2.0f);

        // Compute pre-gain with polynomial curve for musical response
        float d2 = d * d;
        float pre_a = d * 0.5f;
        float pre_b = d2 * d2 * d * 24.0f;
        pre_gain_ = pre_a + (pre_b - pre_a) * d2;

        // Auto-leveling: compute post-gain to maintain consistent volume
        float drive_squashed = d * (1.8f - 0.8f * d); // gentler than (2 - d)
        float target = SoftClip(0.25f + drive_squashed * (pre_gain_ - 0.25f));
        post_gain_ = 0.85f / target; // add small downward trim at higher drive
    }

    const EffectMeta &GetMetadata() const override { return Effects::Distortion::kMeta; }

    void Init(float) override
    {
        SetDrive(ApplyDriveTaper(drive_));
        lpL_ = lpR_ = 0;
    }

    void SetParam(uint8_t id, float v) override
    {
        if (id == 0)
        {
            drive_ = v;
            SetDrive(ApplyDriveTaper(drive_));
        }
        else if (id == 1)
            tone_ = v;
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
        // Exact DaisySP process: pre_gain -> SoftClip -> post_gain
        float xL = SoftClip(l * pre_gain_) * post_gain_;
        float xR = SoftClip(r * pre_gain_) * post_gain_;

        // Tone: one-pole lowpass for warmth control
        // Low tone = warmer (more lowpass), high tone = brighter (more direct)
        float coeff = 0.05f + 0.4f * (1.0f - tone_);
        lpL_ += coeff * (xL - lpL_);
        lpR_ += coeff * (xR - lpR_);

        l = tone_ * xL + (1.0f - tone_) * lpL_;
        r = tone_ * xR + (1.0f - tone_) * lpR_;
    }
};
