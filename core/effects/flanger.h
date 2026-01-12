
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"

// Classic flanger effect inspired by DaisySP
// Uses very short modulated delay with triangle LFO
struct FlangerEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Flanger::TypeId;
    static constexpr int MAX_DELAY_SAMPLES = 480; // ~10ms at 48kHz

    // Internal delay lines
    float delayBufL_[MAX_DELAY_SAMPLES] = {0};
    float delayBufR_[MAX_DELAY_SAMPLES] = {0};
    int writeIdx_ = 0;

    // LFO state - DaisySP style triangle with direction flip
    float lfoPhaseL_ = 0.0f;
    float lfoPhaseR_ = 0.0f;
    float lfoFreq_ = 0.0f; // Signed frequency for direction

    // Parameters (normalized 0-1)
    float rate_ = 0.3f;     // id0: LFO rate
    float depth_ = 0.7f;    // id1: modulation depth
    float feedback_ = 0.5f; // id2: feedback (0-1 maps to -0.95 to +0.95)
    float delay_ = 0.5f;    // id3: base delay
    float mix_ = 0.5f;      // id4: wet/dry mix

    // Computed values
    float delaySamples_ = 0.0f; // Base delay in samples
    float lfoAmp_ = 0.0f;       // LFO amplitude in samples
    float sampleRate_ = 48000.0f;

    const EffectMeta &GetMetadata() const override { return Effects::Flanger::kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        writeIdx_ = 0;
        lfoPhaseL_ = 0.0f;
        lfoPhaseR_ = 0.5f; // Start R channel at opposite phase for stereo spread

        // Clear delay buffers
        for (int i = 0; i < MAX_DELAY_SAMPLES; i++)
        {
            delayBufL_[i] = 0.0f;
            delayBufR_[i] = 0.0f;
        }

        // Initialize computed values
        UpdateDelay();
        UpdateLfoFreq();
        UpdateLfoDepth();
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0:
            rate_ = v;
            UpdateLfoFreq();
            break;
        case 1:
            depth_ = v;
            UpdateLfoDepth();
            break;
        case 2:
            feedback_ = v;
            break;
        case 3:
            delay_ = v;
            UpdateDelay();
            UpdateLfoDepth(); // Depth depends on delay
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

        // Update LFO - DaisySP style triangle (bounce between -1 and 1)
        float lfoSigL = UpdateLfo(lfoPhaseL_);
        float lfoSigR = UpdateLfo(lfoPhaseR_);

        // Calculate modulated delay (always >= 1 sample)
        float delayL = 1.0f + lfoSigL + delaySamples_;
        float delayR = 1.0f + lfoSigR + delaySamples_;

        // Read from delay lines with linear interpolation
        float wetL = ReadDelay(delayBufL_, delayL);
        float wetR = ReadDelay(delayBufR_, delayR);

        // Feedback: map 0-1 to -0.95 to +0.95 for through-zero flanging
        float fb = (feedback_ * 2.0f - 1.0f) * 0.95f;

        // Write to delay lines with feedback (same sample, no extra delay)
        delayBufL_[writeIdx_] = l + wetL * fb;
        delayBufR_[writeIdx_] = r + wetR * fb;

        // Advance write pointer
        writeIdx_ = (writeIdx_ + 1) % MAX_DELAY_SAMPLES;

        // Output mix
        l = dryL * (1.0f - mix_) + wetL * mix_;
        r = dryR * (1.0f - mix_) + wetR * mix_;
    }

private:
    void UpdateDelay()
    {
        // Map 0-1 to 0.1-7ms (DaisySP range)
        float delayMs = 0.1f + delay_ * 6.9f;
        delaySamples_ = delayMs * 0.001f * sampleRate_;
    }

    void UpdateLfoFreq()
    {
        // Map 0-1 to 0.05-5Hz, then to phase increment
        float freq = 0.05f + rate_ * 4.95f;
        float inc = 4.0f * freq / sampleRate_;
        // Preserve direction if already moving
        lfoFreq_ = (lfoFreq_ < 0.0f) ? -inc : inc;
    }

    void UpdateLfoDepth()
    {
        // Depth as fraction of base delay (like DaisySP)
        float d = depth_ * 0.93f; // Max 93% to avoid zero-crossing issues
        lfoAmp_ = d * delaySamples_;
    }

    // DaisySP-style triangle LFO: bounces between -1 and +1
    float UpdateLfo(float &phase)
    {
        phase += lfoFreq_;

        // Bounce off boundaries and flip direction
        if (phase > 1.0f)
        {
            phase = 1.0f - (phase - 1.0f);
            lfoFreq_ = -FastMath::fabs(lfoFreq_);
        }
        else if (phase < -1.0f)
        {
            phase = -1.0f - (phase + 1.0f);
            lfoFreq_ = FastMath::fabs(lfoFreq_);
        }

        return phase * lfoAmp_;
    }

    float ReadDelay(float *buf, float delaySamples)
    {
        // Clamp to valid range
        if (delaySamples < 1.0f)
            delaySamples = 1.0f;
        if (delaySamples > MAX_DELAY_SAMPLES - 2)
            delaySamples = MAX_DELAY_SAMPLES - 2;

        float readPos = (float)writeIdx_ - delaySamples;
        if (readPos < 0.0f)
            readPos += MAX_DELAY_SAMPLES;

        int idx0 = (int)readPos;
        int idx1 = (idx0 + 1) % MAX_DELAY_SAMPLES;
        float frac = readPos - (float)idx0;

        return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
    }
};
