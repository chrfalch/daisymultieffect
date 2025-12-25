
#pragma once
#include "effects/base_effect.h"
#include <cmath>

struct SimpleReverbEffect : BaseEffect
{
    static constexpr uint8_t TypeId = 14;

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

    static constexpr int MAX_PRE = 4800;
    float *preBuf_ = nullptr;
    int preSize = 1, preIdx = 0;

    Comb combs[4];
    Allpass aps[2];

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    static const NumberParamRange kMixRange, kDecayRange, kDampRange, kPreRange, kSizeRange;
    static const ParamInfo kParams[5];
    static const EffectMeta kMeta;
    const EffectMeta &GetMetadata() const override { return kMeta; }

    // Bind all buffers before Init.
    // preBuf: MAX_PRE floats
    // combBufs: 4 x Comb::MAX_DELAY floats each
    // apBufs: 2 x Allpass::MAX_DELAY floats each
    void BindBuffers(float *preBuf, float *combBufs[4], float *apBufs[2])
    {
        preBuf_ = preBuf;
        for (int i = 0; i < 4; i++)
            combs[i].Bind(combBufs[i]);
        for (int i = 0; i < 2; i++)
            aps[i].Bind(apBufs[i]);
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
            preMs_ = v * 80.0f;
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
        out[3] = {3, (uint8_t)((preMs_ / 80.0f) * 127 + 0.5f)};
        out[4] = {4, (uint8_t)(size_ * 127 + 0.5f)};
        return 5;
    }

    void ProcessStereo(float &l, float &r) override
    {
        float inMono = 0.5f * (l + r);
        float x = ProcessPre(inMono);
        float y = ProcessTank(x);

        float width = 0.3f;
        float yL = y * (1.0f + width * 0.3f);
        float yR = y * (1.0f - width * 0.3f);

        float dry = 1.0f - mix_, wet = mix_;
        l = l * dry + yL * wet;
        r = r * dry + yR * wet;
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
        const float base[4] = {0.0297f, 0.0371f, 0.0411f, 0.0437f};
        for (int i = 0; i < 4; i++)
        {
            float t = base[i] * (0.7f + size_ * 0.6f);
            int ds = (int)(t * sr_ + 0.5f);
            combs[i].Init(ds, decay_, damp_);
        }
        int ap1 = (int)(0.005f * (0.8f + size_ * 0.4f) * sr_ + 0.5f);
        int ap2 = (int)(0.0017f * (0.8f + size_ * 0.4f) * sr_ + 0.5f);
        aps[0].Init(ap1, 0.7f);
        aps[1].Init(ap2, 0.7f);
    }
    float ProcessTank(float x)
    {
        float s = 0;
        for (int i = 0; i < 4; i++)
            s += combs[i].Process(x);
        s *= 0.25f;
        s = aps[0].Process(s);
        s = aps[1].Process(s);
        if (s > 1)
            s = 1;
        if (s < -1)
            s = -1;
        return s;
    }
};
