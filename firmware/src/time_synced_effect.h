
#pragma once
#include "base_effect.h"
#include "tempo.h"
#include <cmath>

struct TimeSyncedEffect : BaseEffect
{
    explicit TimeSyncedEffect(TempoSource &t) : tempo_(t) {}

    void Init(float sr) override
    {
        sampleRate_ = sr;
        freeTimeMs_ = 250.0f;
        division_ = 0;
        synced_ = true;
    }

    void SetParam(uint8_t id, float v) override
    {
        if (id == 0)
        {
            float minMs = 10.0f, maxMs = 2000.0f;
            freeTimeMs_ = std::pow(maxMs / minMs, v) * minMs;
        }
        else if (id == 1)
        {
            division_ = (uint8_t)(v * 7.0f + 0.5f);
            if (division_ > 7)
                division_ = 7;
        }
        else if (id == 2)
        {
            synced_ = v >= 0.5f;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 3)
            return 0;
        float minMs = 10.0f, maxMs = 2000.0f;
        float v = std::log(freeTimeMs_ / minMs) / std::log(maxMs / minMs);
        if (v < 0)
            v = 0;
        if (v > 1)
            v = 1;
        out[0] = {0, (uint8_t)(v * 127 + 0.5f)};
        out[1] = {1, (uint8_t)(division_ * 127 / 7)};
        out[2] = {2, (uint8_t)(synced_ ? 127 : 0)};
        return 3;
    }

protected:
    int GetPeriodSamples() const
    {
        if (synced_ && tempo_.valid && tempo_.bpm > 1.0f)
        {
            float q = 60.0f / tempo_.bpm;
            static const float mult[8] = {1.0f, 0.5f, 0.25f, 0.125f, 0.0625f, 0.375f, 0.1666667f, 0.3333333f};
            float sec = q * mult[division_];
            int s = (int)(sec * sampleRate_ + 0.5f);
            return s > 1 ? s : 1;
        }
        int s = (int)(freeTimeMs_ * 0.001f * sampleRate_ + 0.5f);
        return s > 1 ? s : 1;
    }

    TempoSource &tempo_;
    float sampleRate_ = 48000.0f;
    float freeTimeMs_ = 250.0f;
    uint8_t division_ = 0;
    bool synced_ = true;
};
