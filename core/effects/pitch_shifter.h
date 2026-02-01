
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"
#include "effects/pitch_shifter_dsp.h"

struct PitchShifterEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::PitchShifter::TypeId;

    PitchShifterDsp shiftL_, shiftR_;

    float mix_ = 0.5f;   // id0: wet/dry mix [0..1]
    float pitch_ = 0.5f;  // id1: pitch normalized [0..1] -> -24..+24 st
    float fun_ = 0.0f;    // id2: flutter [0..1]
    float tone_ = 1.0f;   // id3: tone (low-pass) [0..1]

    // Simple one-pole low-pass for tone control
    float lpStateL_ = 0.0f;
    float lpStateR_ = 0.0f;
    float lpCoeff_ = 1.0f;
    float sampleRate_ = 48000.0f;

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }
    const EffectMeta &GetMetadata() const override { return Effects::PitchShifter::kMeta; }

    // Bind external buffers for L/R pitch shifter delay lines (kBufSize floats each)
    void BindBuffers(float *bufL, float *bufR)
    {
        shiftL_.BindBuffer(bufL);
        shiftR_.BindBuffer(bufR);
    }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        shiftL_.Init(sr);
        shiftR_.Init(sr);
        shiftL_.SetDelSize(4096.0f);
        shiftR_.SetDelSize(4096.0f);
        lpStateL_ = 0.0f;
        lpStateR_ = 0.0f;
        UpdatePitch();
        UpdateTone();
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0:
            mix_ = v;
            break;
        case 1:
            pitch_ = v;
            UpdatePitch();
            break;
        case 2:
            fun_ = v;
            shiftL_.SetFun(v);
            shiftR_.SetFun(v);
            break;
        case 3:
            tone_ = v;
            UpdateTone();
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 4)
            return 0;
        out[0] = {0, (uint8_t)(mix_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(pitch_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(fun_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(tone_ * 127.0f + 0.5f)};
        return 4;
    }

    void ProcessStereo(float &l, float &r) override
    {
        float wetL = shiftL_.Process(l);
        float wetR = shiftR_.Process(r);

        // Apply tone filter (one-pole low-pass)
        lpStateL_ += lpCoeff_ * (wetL - lpStateL_);
        lpStateR_ += lpCoeff_ * (wetR - lpStateR_);
        wetL = lpStateL_;
        wetR = lpStateR_;

        // Wet/dry mix
        l = l * (1.0f - mix_) + wetL * mix_;
        r = r * (1.0f - mix_) + wetR * mix_;
    }

private:
    void UpdatePitch()
    {
        // Map 0..1 to -24..+24 semitones, quantized to nearest semitone
        float semi = -24.0f + pitch_ * 48.0f;
        semi = (int)(semi + (semi >= 0 ? 0.5f : -0.5f));
        shiftL_.SetTransposition(semi);
        shiftR_.SetTransposition(semi);
    }

    void UpdateTone()
    {
        // Map tone 0..1 to low-pass coefficient
        // 0 = dark (low cutoff ~200 Hz), 1 = bright (wide open)
        float freq = 200.0f + tone_ * 18000.0f;
        float w = freq / sampleRate_;
        if (w > 0.49f)
            w = 0.49f;
        lpCoeff_ = FastMath::kTwoPi * w / (1.0f + FastMath::kTwoPi * w);
    }
};
