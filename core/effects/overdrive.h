
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

    // Parameter taper: keep 0..1 range and max drive, but make the response
    // less aggressive in the 0..~0.6 region.
    static inline float ApplyDriveTaper(float drive)
    {
        drive = fclamp(drive, 0.0f, 1.0f);
        constexpr float kLinearMix = 0.35f; // 0=quadratic, 1=linear
        return drive * (kLinearMix + (1.0f - kLinearMix) * drive);
    }

    // Exact DaisySP SetDrive implementation
    void SetDrive(float drive)
    {
        drive = fclamp(drive, 0.0f, 1.0f);
        float d = 2.0f * drive;

        // Compute pre-gain with polynomial curve for musical response
        float d2 = d * d;
        float pre_a = d * 0.5f;
        float pre_b = d2 * d2 * d * 24.0f;
        pre_gain_ = pre_a + (pre_b - pre_a) * d2;

        // Auto-leveling: compute post-gain to maintain consistent volume
        float drive_squashed = d * (2.0f - d);
        post_gain_ = 1.0f / SoftClip(0.33f + drive_squashed * (pre_gain_ - 0.33f));
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
