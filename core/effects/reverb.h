
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>

struct SimpleReverbEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Reverb::TypeId;

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

    static constexpr int MAX_PRE = 9600;
    float *preBuf_ = nullptr;
    int preSize = 1, preIdx = 0;

    Comb combsL[4], combsR[4];
    Allpass apsL[2], apsR[2];

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    const EffectMeta &GetMetadata() const override { return Effects::Reverb::kMeta; }

    // Bind all buffers before Init.
    // preBuf: MAX_PRE floats
    // combBufsL/R: 4 x Comb::MAX_DELAY floats each (L and R channels)
    // apBufsL/R: 2 x Allpass::MAX_DELAY floats each (L and R channels)
    void BindBuffers(float *preBuf, float *combBufsL[4], float *combBufsR[4],
                     float *apBufsL[2], float *apBufsR[2])
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
    }

    void Init(float sr) override
    {
        sr_ = sr;
        UpdatePre();
        UpdateTank();
    }

    void SetParam(uint8_t id, float v) override
    {
        if (id == 0)
            mix_ = v;
        else if (id == 1)
        {
            decay_ = 0.2f + v * 0.75f;
            UpdateTank();
        }
        else if (id == 2)
        {
            damp_ = v * 0.8f;
            UpdateTank();
        }
        else if (id == 3)
        {
            preMs_ = v * 200.0f;
            UpdatePre();
        }
        else if (id == 4)
        {
            size_ = v;
            UpdateTank();
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 5)
            return 0;
        out[0] = {0, (uint8_t)(mix_ * 127 + 0.5f)};
        out[1] = {1, (uint8_t)(((decay_ - 0.2f) / 0.75f) * 127 + 0.5f)};
        out[2] = {2, (uint8_t)((damp_ / 0.8f) * 127 + 0.5f)};
        out[3] = {3, (uint8_t)((preMs_ / 200.0f) * 127 + 0.5f)};
        out[4] = {4, (uint8_t)(size_ * 127 + 0.5f)};
        return 5;
    }

    void ProcessStereo(float &l, float &r) override
    {
        float mono = 0.5f * (l + r);
        float pre = ProcessPre(mono);
        float wetL, wetR;
        ProcessTank(pre, pre, wetL, wetR);

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
        static constexpr int kStereoSpread = 23; // classic Freeverb stereo offset
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
        if (sL > 1) sL = 1;
        if (sL < -1) sL = -1;
        if (sR > 1) sR = 1;
        if (sR < -1) sR = -1;
        outL = sL;
        outR = sR;
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif
};
