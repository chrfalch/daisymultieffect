
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"
#include "effects/pitch_shifter_dsp.h"

struct ShimmerReverbEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::ShimmerReverb::TypeId;

    // Reuse same Comb/Allpass/PreDelay structures as SimpleReverbEffect
    struct Comb
    {
        static constexpr int MAX_DELAY = 48000;
        float *buf = nullptr;
        int size = 1, idx = 0;
        float fb = 0.7f, damp = 0.2f, lp = 0;
        void Bind(float *b) { buf = b; }
        void Init(int s, float f, float d)
        {
            if (!buf)
                return;
            if (s < 1)
                s = 1;
            if (s > MAX_DELAY)
                s = MAX_DELAY;
            size = s;
            idx = 0;
            fb = f;
            damp = d;
            lp = 0;
            for (int i = 0; i < size; i++)
                buf[i] = 0;
        }
        float Process(float in)
        {
            if (!buf)
                return in;
            float y = buf[idx];
            lp += damp * (y - lp);
            buf[idx] = in + lp * fb;
            if (++idx >= size)
                idx = 0;
            return y;
        }
    };
    struct Allpass
    {
        static constexpr int MAX_DELAY = 2400;
        float *buf = nullptr;
        int size = 1, idx = 0;
        float g = 0.7f;
        void Bind(float *b) { buf = b; }
        void Init(int s, float gain)
        {
            if (!buf)
                return;
            if (s < 1)
                s = 1;
            if (s > MAX_DELAY)
                s = MAX_DELAY;
            size = s;
            idx = 0;
            g = gain;
            for (int i = 0; i < size; i++)
                buf[i] = 0;
        }
        float Process(float in)
        {
            if (!buf)
                return in;
            float y = buf[idx];
            float xn = in + (-g) * y;
            buf[idx] = xn;
            if (++idx >= size)
                idx = 0;
            return y + g * xn;
        }
    };

    float sr_ = 48000;
    float mix_ = 0.3f, decay_ = 0.7f, damp_ = 0.3f, preMs_ = 20.0f, size_ = 0.7f;
    float shimmer_ = 0.3f;   // Shimmer feedback amount [0..1]
    float shimPitch_ = 0.5f; // Shimmer pitch normalized [0..1] -> +5..+24 st

    static constexpr int MAX_PRE = 9600;
    float *preBuf_ = nullptr;
    int preSize = 1, preIdx = 0;

    Comb combsL[4], combsR[4];
    Allpass apsL[2], apsR[2];

    // Shimmer: one mono pitch shifter in the feedback path
    PitchShifterDsp shimmerShift_;
    float shimmerFeedback_ = 0.0f; // Accumulated shimmer feedback sample
    float shimmerLp_ = 0.0f;       // One-pole LP state for feedback damping
    float shimmerLpCoeff_ = 0.3f;  // LP coefficient (updated in Init)
    float dcX_ = 0.0f, dcY_ = 0.0f; // DC blocker state

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }
    const EffectMeta &GetMetadata() const override { return Effects::ShimmerReverb::kMeta; }

    // Bind all buffers before Init (same pattern as SimpleReverbEffect)
    // shimmerBuf: PitchShifterDsp::kBufSize floats for the shimmer pitch shifter
    void BindBuffers(float *preBuf, float *combBufsL[4], float *combBufsR[4],
                     float *apBufsL[2], float *apBufsR[2], float *shimmerBuf)
    {
        preBuf_ = preBuf;
        for (int i = 0; i < 4; i++)
        {
            combsL[i].Bind(combBufsL[i]);
            combsR[i].Bind(combBufsR[i]);
        }
        for (int i = 0; i < 2; i++)
        {
            apsL[i].Bind(apBufsL[i]);
            apsR[i].Bind(apBufsR[i]);
        }
        shimmerShift_.BindBuffer(shimmerBuf);
    }

    void Init(float sr) override
    {
        sr_ = sr;
        shimmerShift_.Init(sr);
        shimmerShift_.SetDelSize(4096.0f);
        shimmerFeedback_ = 0.0f;
        shimmerLp_ = 0.0f;
        dcX_ = 0.0f;
        dcY_ = 0.0f;
        // LP at ~6kHz to tame ultrasonic buildup while preserving shimmer harmonics
        float w = 6000.0f / sr;
        shimmerLpCoeff_ = FastMath::kTwoPi * w / (1.0f + FastMath::kTwoPi * w);
        UpdatePre();
        UpdateTank();
        UpdateShimmerPitch();
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0:
            mix_ = v;
            break;
        case 1:
            decay_ = 0.2f + v * 0.79f;
            UpdateTank();
            break;
        case 2:
            damp_ = v * 0.8f;
            UpdateTank();
            break;
        case 3:
            preMs_ = v * 200.0f;
            UpdatePre();
            break;
        case 4:
            size_ = v;
            UpdateTank();
            break;
        case 5:
            shimmer_ = v;
            break;
        case 6:
            shimPitch_ = v;
            UpdateShimmerPitch();
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 7)
            return 0;
        out[0] = {0, (uint8_t)(mix_ * 127 + 0.5f)};
        out[1] = {1, (uint8_t)(((decay_ - 0.2f) / 0.79f) * 127 + 0.5f)};
        out[2] = {2, (uint8_t)((damp_ / 0.8f) * 127 + 0.5f)};
        out[3] = {3, (uint8_t)((preMs_ / 200.0f) * 127 + 0.5f)};
        out[4] = {4, (uint8_t)(size_ * 127 + 0.5f)};
        out[5] = {5, (uint8_t)(shimmer_ * 127 + 0.5f)};
        out[6] = {6, (uint8_t)(shimPitch_ * 127 + 0.5f)};
        return 7;
    }

    void ProcessStereo(float &l, float &r) override
    {
        float mono = 0.5f * (l + r);

        // Mix in shimmer feedback before predelay
        float tankIn = mono + shimmerFeedback_;

        float pre = ProcessPre(tankIn);
        float wetL, wetR;
        ProcessTank(pre, pre, wetL, wetR);

        // Shimmer: pitch-shift the reverb wet signal
        float wetMono = 0.5f * (wetL + wetR);
        float shifted = shimmerShift_.Process(wetMono);

        // LP filter to tame ultrasonic buildup from repeated pitch-up
        shimmerLp_ += shimmerLpCoeff_ * (shifted - shimmerLp_);
        float shimSig = shimmerLp_;

        // Blend pitch-shifted signal directly into wet output
        // so shimmer harmonics are immediately audible
        wetL += shimSig * shimmer_;
        wetR += shimSig * shimmer_;

        // Feedback: scale by (1-decay) to compensate for the reverb tank's
        // own recirculation, preventing compound gain blowup
        float fbGain = shimmer_ * (1.0f - decay_ * 0.8f);
        float fb = shimSig * fbGain;

        // Soft saturation to prevent runaway
        float absFb = fb > 0 ? fb : -fb;
        if (absFb > 0.5f)
            fb = fb / (1.0f + absFb);

        // DC blocker: y[n] = x[n] - x[n-1] + 0.995 * y[n-1]
        float dcIn = fb;
        dcY_ = dcIn - dcX_ + 0.995f * dcY_;
        dcX_ = dcIn;
        fb = dcY_;

        shimmerFeedback_ = fb;

        float dry = 1.0f - mix_, wet = mix_;
        l = l * dry + wetL * wet;
        r = r * dry + wetR * wet;
    }

private:
    void UpdatePre()
    {
        if (!preBuf_)
            return;
        int s = (int)(preMs_ * 0.001f * sr_ + 0.5f);
        if (s < 1)
            s = 1;
        if (s > MAX_PRE)
            s = MAX_PRE;
        preSize = s;
        preIdx = 0;
        for (int i = 0; i < preSize; i++)
            preBuf_[i] = 0;
    }

    float ProcessPre(float x)
    {
        if (!preBuf_)
            return x;
        float y = preBuf_[preIdx];
        preBuf_[preIdx] = x;
        if (++preIdx >= preSize)
            preIdx = 0;
        return y;
    }

    void UpdateTank()
    {
        static constexpr int kStereoSpread = 23;
        const float base[4] = {0.0297f, 0.0371f, 0.0411f, 0.0437f};
        for (int i = 0; i < 4; i++)
        {
            float t = base[i] * (0.5f + size_ * 1.5f);
            int ds = (int)(t * sr_ + 0.5f);
            combsL[i].Init(ds, decay_, damp_);
            combsR[i].Init(ds + kStereoSpread, decay_, damp_);
        }
        int ap1 = (int)(0.005f * (0.5f + size_ * 1.5f) * sr_ + 0.5f);
        int ap2 = (int)(0.0017f * (0.5f + size_ * 1.5f) * sr_ + 0.5f);
        apsL[0].Init(ap1, 0.7f);
        apsL[1].Init(ap2, 0.7f);
        apsR[0].Init(ap1 + kStereoSpread, 0.7f);
        apsR[1].Init(ap2 + kStereoSpread, 0.7f);
    }

    void ProcessTank(float inL, float inR, float &outL, float &outR)
#if !defined(DAISY_SEED_BUILD)
    {
        float sL = 0, sR = 0;
        for (int i = 0; i < 4; i++)
        {
            sL += combsL[i].Process(inL);
            sR += combsR[i].Process(inR);
        }
        sL *= 0.25f;
        sR *= 0.25f;
        sL = apsL[0].Process(sL);
        sL = apsL[1].Process(sL);
        sR = apsR[0].Process(sR);
        sR = apsR[1].Process(sR);
        if (sL > 1)
            sL = 1;
        if (sL < -1)
            sL = -1;
        if (sR > 1)
            sR = 1;
        if (sR < -1)
            sR = -1;
        outL = sL;
        outR = sR;
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif

    void UpdateShimmerPitch()
    {
        // Map 0..1 to +5..+24 semitones, quantized to nearest semitone
        float semi = 5.0f + shimPitch_ * 19.0f;
        semi = (int)(semi + 0.5f);
        shimmerShift_.SetTransposition(semi);
    }
};
