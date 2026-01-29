
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"

// Pure C++ chorus implementation (no DaisySP dependency)
// Uses a modulated delay line with LFO
struct ChorusEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Chorus::TypeId;
    static constexpr int MAX_DELAY_SAMPLES = 4800; // ~100ms at 48kHz

    // Internal delay line
    float delayBufL_[MAX_DELAY_SAMPLES] = {0};
    float delayBufR_[MAX_DELAY_SAMPLES] = {0};
    int writeIdx_ = 0;

    // LFO state
    float lfoPhase_ = 0.0f;
    float lfoPhaseR_ = 0.25f; // Offset for stereo spread
    float lfoInc_ = 0.0f;     // Pre-computed LFO phase increment

    // Parameters
    float rate_ = 0.3f;     // id0: LFO rate (0.1-2 Hz)
    float depth_ = 0.4f;    // id1: modulation depth
    float feedback_ = 0.0f; // id2: feedback amount
    float delay_ = 0.4f;    // id3: base delay (0-1 maps to 5-25ms)
    float mix_ = 0.5f;      // id4: wet/dry mix

    float sampleRate_ = 48000.0f;

    const EffectMeta &GetMetadata() const override { return Effects::Chorus::kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        writeIdx_ = 0;
        lfoPhase_ = 0.0f;
        lfoPhaseR_ = 0.25f;

        // Pre-compute LFO increment
        updateLfoInc();

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
            updateLfoInc(); // Recompute LFO increment when rate changes
            break;
        case 1:
            depth_ = v;
            break;
        case 2:
            feedback_ = v;
            break;
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
#if !defined(DAISY_SEED_BUILD)
    {
        float dryL = l, dryR = r;

        // Update LFO phases using pre-computed increment
        lfoPhase_ += lfoInc_;
        if (lfoPhase_ >= 1.0f)
            lfoPhase_ -= 1.0f;

        lfoPhaseR_ += lfoInc_;
        if (lfoPhaseR_ >= 1.0f)
            lfoPhaseR_ -= 1.0f;

        // Calculate modulated delays
        // Base delay: 5-25ms
        float baseDelayMs = 5.0f + delay_ * 20.0f;
        float baseDelaySamples = baseDelayMs * 0.001f * sampleRate_;

        // LFO modulation using fast sine lookup
        float lfoL = FastMath::fastSin(lfoPhase_);
        float lfoR = FastMath::fastSin(lfoPhaseR_);

        // Modulation depth in samples (up to ±3ms)
        float modDepthSamples = depth_ * 0.003f * sampleRate_;

        float delayL = baseDelaySamples + lfoL * modDepthSamples;
        float delayR = baseDelaySamples + lfoR * modDepthSamples;

        // Clamp delays
        if (delayL < 1.0f)
            delayL = 1.0f;
        if (delayL > MAX_DELAY_SAMPLES - 2)
            delayL = MAX_DELAY_SAMPLES - 2;
        if (delayR < 1.0f)
            delayR = 1.0f;
        if (delayR > MAX_DELAY_SAMPLES - 2)
            delayR = MAX_DELAY_SAMPLES - 2;

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

        // Write to delay lines with feedback
        delayBufL_[writeIdx_] = l + wetL * feedback_ * 0.7f;
        delayBufR_[writeIdx_] = r + wetR * feedback_ * 0.7f;

        // Advance write pointer
        writeIdx_ = (writeIdx_ + 1) % MAX_DELAY_SAMPLES;

        // Mix dry/wet
        l = dryL * (1.0f - mix_) + wetL * mix_;
        r = dryR * (1.0f - mix_) + wetR * mix_;
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif

private:
    // Pre-compute LFO phase increment when rate or sample rate changes
    void updateLfoInc()
    {
        // LFO: 0.1 to 2 Hz
        float lfoFreq = 0.1f + rate_ * 1.9f;
        lfoInc_ = lfoFreq / sampleRate_;
    }
};
