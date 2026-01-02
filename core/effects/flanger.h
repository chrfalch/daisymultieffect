
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>

// Classic flanger effect with through-zero capability
// Uses very short modulated delay (0.1-10ms) with high feedback
struct FlangerEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Flanger::TypeId;
    static constexpr int MAX_DELAY_SAMPLES = 960; // ~20ms at 48kHz (plenty of headroom)

    // Internal delay line
    float delayBufL_[MAX_DELAY_SAMPLES] = {0};
    float delayBufR_[MAX_DELAY_SAMPLES] = {0};
    int writeIdx_ = 0;

    // LFO state (triangle wave for classic flanger sound)
    float lfoPhase_ = 0.0f;
    float lfoPhaseR_ = 0.5f; // 180° offset for stereo spread

    // Parameters (normalized 0-1)
    float rate_ = 0.3f;     // id0: LFO rate
    float depth_ = 0.7f;    // id1: modulation depth
    float feedback_ = 0.5f; // id2: feedback (-1 to +1 range, stored as 0-1)
    float delay_ = 0.3f;    // id3: base delay
    float mix_ = 0.5f;      // id4: wet/dry mix

    float sampleRate_ = 48000.0f;

    // Previous wet samples for feedback
    float prevWetL_ = 0.0f;
    float prevWetR_ = 0.0f;

    const EffectMeta &GetMetadata() const override { return Effects::Flanger::kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        writeIdx_ = 0;
        lfoPhase_ = 0.0f;
        lfoPhaseR_ = 0.5f;
        prevWetL_ = 0.0f;
        prevWetR_ = 0.0f;

        // Clear delay buffers
        for (int i = 0; i < MAX_DELAY_SAMPLES; i++)
        {
            delayBufL_[i] = 0.0f;
            delayBufR_[i] = 0.0f;
        }
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0:
            rate_ = v;
            break;
        case 1:
            depth_ = v;
            break;
        case 2:
            feedback_ = v;
            break; // 0-1 maps to -0.95 to +0.95
        case 3:
            delay_ = v;
            break;
        case 4:
            mix_ = v;
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 5)
            return 0;
        out[0] = {0, (uint8_t)(rate_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(depth_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(feedback_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(delay_ * 127.0f + 0.5f)};
        out[4] = {4, (uint8_t)(mix_ * 127.0f + 0.5f)};
        return 5;
    }

    void ProcessStereo(float &l, float &r) override
    {
        float dryL = l, dryR = r;

        // LFO: 0.05 to 5 Hz
        float lfoFreq = 0.05f + rate_ * 4.95f;
        float lfoInc = lfoFreq / sampleRate_;

        // Update LFO phases
        lfoPhase_ += lfoInc;
        if (lfoPhase_ >= 1.0f)
            lfoPhase_ -= 1.0f;

        lfoPhaseR_ += lfoInc;
        if (lfoPhaseR_ >= 1.0f)
            lfoPhaseR_ -= 1.0f;

        // Triangle wave LFO (classic flanger sweep)
        auto triangleLFO = [](float phase) -> float
        {
            if (phase < 0.5f)
                return phase * 4.0f - 1.0f; // -1 to +1
            else
                return 3.0f - phase * 4.0f; // +1 to -1
        };

        float lfoL = triangleLFO(lfoPhase_);
        float lfoR = triangleLFO(lfoPhaseR_);

        // Base delay: 0.1-10ms (very short for flanger comb filtering)
        float baseDelayMs = 0.1f + delay_ * 9.9f;
        float baseDelaySamples = baseDelayMs * 0.001f * sampleRate_;

        // Modulation depth (sweep nearly to zero for classic flanger)
        // At full depth, sweeps from ~0.1ms to base delay
        float modDepthSamples = depth_ * baseDelaySamples * 0.9f;

        float delayL = baseDelaySamples + lfoL * modDepthSamples;
        float delayR = baseDelaySamples + lfoR * modDepthSamples;

        // Clamp delays (ensure at least 1 sample)
        float minDelay = 1.0f;
        float maxDelay = (float)(MAX_DELAY_SAMPLES - 2);
        if (delayL < minDelay)
            delayL = minDelay;
        if (delayL > maxDelay)
            delayL = maxDelay;
        if (delayR < minDelay)
            delayR = minDelay;
        if (delayR > maxDelay)
            delayR = maxDelay;

        // Read from delay lines with linear interpolation
        auto readDelay = [this](float *buf, float delaySamples) -> float
        {
            float readPos = (float)writeIdx_ - delaySamples;
            if (readPos < 0)
                readPos += MAX_DELAY_SAMPLES;

            int idx0 = (int)readPos;
            int idx1 = (idx0 + 1) % MAX_DELAY_SAMPLES;
            float frac = readPos - (float)idx0;

            return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
        };

        float wetL = readDelay(delayBufL_, delayL);
        float wetR = readDelay(delayBufR_, delayR);

        // Feedback: map 0-1 to -0.95 to +0.95 for through-zero flanging
        float fb = (feedback_ * 2.0f - 1.0f) * 0.95f;

        // Write to delay lines with feedback
        // Use previous wet samples for feedback to avoid click artifacts
        delayBufL_[writeIdx_] = l + prevWetL_ * fb;
        delayBufR_[writeIdx_] = r + prevWetR_ * fb;

        // Store for next iteration
        prevWetL_ = wetL;
        prevWetR_ = wetR;

        // Advance write pointer
        writeIdx_ = (writeIdx_ + 1) % MAX_DELAY_SAMPLES;

        // Output mix
        l = dryL * (1.0f - mix_) + wetL * mix_;
        r = dryR * (1.0f - mix_) + wetR * mix_;
    }
};
