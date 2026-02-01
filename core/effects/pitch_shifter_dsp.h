
#pragma once
#include "effects/fast_math.h"

/**
 * PitchShifter DSP primitive — self-contained pitch shifter
 * Ported from DaisySP pitchshifter.h with no DaisySP dependency.
 *
 * Two crossfaded delay lines with sine-window gains.
 * Uses FastMath::fastSin for the window function.
 * xorshift32 PRNG for flutter modulation (local state, no globals).
 *
 * Buffer size: 8192 samples (sufficient for musical pitch shifting at 48kHz).
 * Buffer must be externally allocated and bound via BindBuffer() before Init().
 * On firmware, buffers live in SDRAM. On VST, heap-allocated.
 */
struct PitchShifterDsp
{
    static constexpr int kBufSize = 8192;
    static constexpr float kBufSizeF = static_cast<float>(kBufSize);

    float *buf_ = nullptr;
    float sr_ = 48000.0f;

    // Phasor state
    float phasorFreq_ = 0.0f;
    float phasorPhase_ = 0.0f;

    // Parameters
    float transpose_ = 12.0f; // semitones
    float delSize_ = 2048.0f; // delay line length in samples
    float fun_ = 0.0f;        // flutter amount [0..1]

    // Write index
    int writeIdx_ = 0;

    // PRNG state for flutter
    uint32_t rngState_ = 0x12345678u;

    void BindBuffer(float *buf) { buf_ = buf; }

    void Init(float sampleRate)
    {
        sr_ = sampleRate;
        phasorPhase_ = 0.0f;
        writeIdx_ = 0;
        if (buf_)
        {
            for (int i = 0; i < kBufSize; i++)
                buf_[i] = 0.0f;
        }
        UpdatePhasorFreq();
    }

    void SetTransposition(float semitones)
    {
        transpose_ = semitones;
        UpdatePhasorFreq();
    }

    void SetDelSize(float size)
    {
        delSize_ = FastMath::fclamp(size, 64.0f, kBufSizeF - 1.0f);
    }

    void SetFun(float f)
    {
        fun_ = FastMath::fclamp(f, 0.0f, 1.0f);
    }

    float Process(float in)
    {
        if (!buf_)
            return in;

        // Write input to circular buffer
        buf_[writeIdx_] = in;
        writeIdx_ = (writeIdx_ + 1) & (kBufSize - 1);

        // Advance phasor
        phasorPhase_ += phasorFreq_;
        if (phasorPhase_ > 1.0f)
            phasorPhase_ -= 1.0f;
        if (phasorPhase_ < 0.0f)
            phasorPhase_ += 1.0f;

        float phase1 = phasorPhase_;
        float phase2 = phasorPhase_ + 0.5f;
        if (phase2 > 1.0f)
            phase2 -= 1.0f;

        // Hann window gains: w = 0.5*(1 - cos(2π*phase)), always in [0,1]
        // Two taps offset by 0.5 sum to exactly 1.0 (constant power)
        float w1 = 0.5f * (1.0f - FastMath::fastCos(phase1));
        float w2 = 0.5f * (1.0f - FastMath::fastCos(phase2));

        // Flutter modulation
        float flutter = 0.0f;
        if (fun_ > 0.001f)
        {
            float rnd = Xorshift32Normalized();
            flutter = fun_ * delSize_ * 0.01f * rnd;
        }

        // Read positions: phase * delSize gives delay tap position
        float d1 = phase1 * delSize_ + flutter;
        float d2 = phase2 * delSize_ + flutter;

        // Read from delay buffer with linear interpolation
        float out1 = ReadDelay(d1);
        float out2 = ReadDelay(d2);

        return out1 * w1 + out2 * w2;
    }

private:
    void UpdatePhasorFreq()
    {
        // Convert semitones to pitch ratio, then to phasor frequency.
        // Read pointer speed = 1 - phasorFreq * delSize.
        // For pitch ratio R, read speed must be R, so:
        //   phasorFreq = (1 - R) / delSize
        // Positive semitones → R > 1 → negative freq → delay shrinks → pitch up.
        // Use exp2f (not FastMath::fastPow2) for accuracy — this only runs
        // on parameter change, not per-sample, so precision matters.
        float ratio = exp2f(transpose_ / 12.0f);
        phasorFreq_ = (1.0f - ratio) / delSize_;
    }

    float ReadDelay(float delaySamples) const
    {
        float readPos = static_cast<float>(writeIdx_) - delaySamples - 1.0f;
        while (readPos < 0.0f)
            readPos += kBufSizeF;

        int idx0 = static_cast<int>(readPos) & (kBufSize - 1);
        int idx1 = (idx0 + 1) & (kBufSize - 1);
        float frac = readPos - static_cast<float>(static_cast<int>(readPos));

        return buf_[idx0] + frac * (buf_[idx1] - buf_[idx0]);
    }

    float Xorshift32Normalized()
    {
        rngState_ ^= rngState_ << 13;
        rngState_ ^= rngState_ >> 17;
        rngState_ ^= rngState_ << 5;
        // Map to [-1, 1]
        return static_cast<float>(static_cast<int32_t>(rngState_)) * (1.0f / 2147483648.0f);
    }
};
