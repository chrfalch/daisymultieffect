
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"

// Classic phaser effect using cascaded first-order all-pass filters
// Like MXR Phase 90/100 - creates sweeping notches in frequency response
struct PhaserEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Phaser::TypeId;
    static constexpr int MAX_STAGES = 6;

    // First-order all-pass filter state
    // y[n] = a*x[n] + x[n-1] - a*y[n-1]
    struct AllPassState
    {
        float x1 = 0.0f; // x[n-1]
        float y1 = 0.0f; // y[n-1]
    };

    AllPassState stagesL_[MAX_STAGES];
    AllPassState stagesR_[MAX_STAGES];

    // LFO state
    float lfoPhase_ = 0.0f;
    float lfoInc_ = 0.0f; // Pre-computed LFO phase increment

    // Feedback state
    float fbL_ = 0.0f;
    float fbR_ = 0.0f;

    // Parameters (normalized 0-1)
    float rate_ = 0.3f;     // id0: LFO rate
    float depth_ = 0.8f;    // id1: sweep depth
    float feedback_ = 0.5f; // id2: feedback/resonance
    float freq_ = 0.5f;     // id3: center frequency
    float mix_ = 0.5f;      // id4: wet/dry mix

    int numStages_ = 4;
    float sampleRate_ = 48000.0f;

    const EffectMeta &GetMetadata() const override { return Effects::Phaser::kMeta; }

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        lfoPhase_ = 0.0f;
        fbL_ = 0.0f;
        fbR_ = 0.0f;

        // Pre-compute LFO increment
        updateLfoInc();

        // Clear all-pass states
        for (int i = 0; i < MAX_STAGES; i++)
        {
            stagesL_[i] = {0.0f, 0.0f};
            stagesR_[i] = {0.0f, 0.0f};
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
            freq_ = v;
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
        out[3] = {3, (uint8_t)(freq_ * 127.0f + 0.5f)};
        out[4] = {4, (uint8_t)(mix_ * 127.0f + 0.5f)};
        return 5;
    }

    void ProcessStereo(float &l, float &r) override
#if !defined(DAISY_SEED_BUILD)
    {
        float dryL = l, dryR = r;

        // Update LFO using pre-computed increment
        lfoPhase_ += lfoInc_;
        if (lfoPhase_ >= 1.0f)
            lfoPhase_ -= 1.0f;

        // Fast sine lookup for LFO
        float lfo = FastMath::fastSin(lfoPhase_);

        // Calculate all-pass coefficient from LFO
        // Sweep frequency range: 200Hz to 2000Hz (typical phaser range)
        float centerFreq = 400.0f + freq_ * 1200.0f; // 400-1600 Hz center
        float sweepRange = centerFreq * 0.8f * depth_;
        float currentFreq = centerFreq + lfo * sweepRange;

        // Clamp frequency
        currentFreq = FastMath::fclamp(currentFreq, 100.0f, 4000.0f);

        // Calculate all-pass coefficient using fast tan
        // a = (tan(π*f/sr) - 1) / (tan(π*f/sr) + 1)
        float w = FastMath::kPi * currentFreq / sampleRate_;
        float wPhase = w * (1.0f / FastMath::kTwoPi); // Convert to normalized phase
        float tanw = FastMath::fastTan(wPhase);
        float coeff = (tanw - 1.0f) / (tanw + 1.0f);

        // Feedback amount (0 to 0.7 for stability)
        float fb = feedback_ * 0.7f;

        // Process left channel through all-pass cascade
        float wetL = l + fbL_ * fb;
        for (int i = 0; i < numStages_; i++)
        {
            wetL = ProcessAllPass(stagesL_[i], wetL, coeff);
        }
        fbL_ = wetL;

        // Process right channel (slightly offset phase for stereo width)
        float lfoR = FastMath::fastSin(lfoPhase_ + 0.25f);
        float freqR = FastMath::fclamp(centerFreq + lfoR * sweepRange, 100.0f, 4000.0f);
        float wR = FastMath::kPi * freqR / sampleRate_;
        float wRPhase = wR * (1.0f / FastMath::kTwoPi);
        float tanwR = FastMath::fastTan(wRPhase);
        float coeffR = (tanwR - 1.0f) / (tanwR + 1.0f);

        float wetR = r + fbR_ * fb;
        for (int i = 0; i < numStages_; i++)
        {
            wetR = ProcessAllPass(stagesR_[i], wetR, coeffR);
        }
        fbR_ = wetR;

        // Mix: when mixing dry + wet, notches appear at frequencies where
        // the phase shift causes cancellation
        l = dryL + wetL * mix_;
        r = dryR + wetR * mix_;

        // Normalize output (dry + wet can exceed 1.0)
        l *= 0.7f;
        r *= 0.7f;
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif

private:
    // Pre-compute LFO phase increment when rate or sample rate changes
    void updateLfoInc()
    {
        // LFO: 0.1 to 5 Hz
        float lfoRate = 0.1f + rate_ * 4.9f;
        lfoInc_ = lfoRate / sampleRate_;
    }

    // First-order all-pass filter
    // H(z) = (a + z^-1) / (1 + a*z^-1)
    // y[n] = a*x[n] + x[n-1] - a*y[n-1]
    float ProcessAllPass(AllPassState &state, float in, float coeff)
    {
        float out = coeff * in + state.x1 - coeff * state.y1;
        state.x1 = in;
        state.y1 = out;
        return out;
    }
};
