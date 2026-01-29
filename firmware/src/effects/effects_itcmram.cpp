/**
 * ITCMRAM-placed effect implementations for Daisy Seed firmware.
 *
 * These are the CPU-heavy per-sample DSP functions placed in 64KB
 * zero-wait-state ITCMRAM to avoid QSPI flash cache miss penalties.
 *
 * Each function uses noinline to prevent LTO from inlining it back
 * into a QSPI-flash caller. Inline helper methods (Biquad::Process,
 * Comb::Process, FastMath::*, etc.) are inlined INTO these functions,
 * so they also execute from ITCMRAM.
 */

#if defined(DAISY_SEED_BUILD)
#define ITCMRAM_CODE __attribute__((section(".itcmram_text")))
#else
#define ITCMRAM_CODE
#endif

#include "effects/reverb.h"
#include "effects/eq.h"
#include "effects/phaser.h"
#include "effects/compressor.h"
#include "effects/chorus.h"
#include "effects/flanger.h"

// ---------------------------------------------------------------------------
// Reverb: ProcessTank — stereo 4+4 comb filters + 2+2 allpass filters
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void SimpleReverbEffect::ProcessTank(float inL, float inR, float &outL, float &outR)
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

// ---------------------------------------------------------------------------
// Graphic EQ: ProcessStereo — 7 cascaded biquad filters per channel
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void GraphicEQEffect::ProcessStereo(float &l, float &r)
{
    if (filtersDirty_)
    {
        UpdateAllFilters();
        filtersDirty_ = false;
    }

    float outL = l;
    float outR = r;

    for (int i = 0; i < NUM_BANDS; i++)
    {
        outL = filtersL_[i].Process(outL);
        outR = filtersR_[i].Process(outR);
    }

    l = outL;
    r = outR;
}

// ---------------------------------------------------------------------------
// Phaser: ProcessStereo — LFO + fastTan + allpass cascade per channel
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void PhaserEffect::ProcessStereo(float &l, float &r)
{
    float dryL = l, dryR = r;

    lfoPhase_ += lfoInc_;
    if (lfoPhase_ >= 1.0f)
        lfoPhase_ -= 1.0f;

    float lfo = FastMath::fastSin(lfoPhase_);

    float centerFreq = 400.0f + freq_ * 1200.0f;
    float sweepRange = centerFreq * 0.8f * depth_;
    float currentFreq = centerFreq + lfo * sweepRange;
    currentFreq = FastMath::fclamp(currentFreq, 100.0f, 4000.0f);

    float w = FastMath::kPi * currentFreq / sampleRate_;
    float wPhase = w * (1.0f / FastMath::kTwoPi);
    float tanw = FastMath::fastTan(wPhase);
    float coeff = (tanw - 1.0f) / (tanw + 1.0f);

    float fb = feedback_ * 0.7f;

    float wetL = l + fbL_ * fb;
    for (int i = 0; i < numStages_; i++)
        wetL = ProcessAllPass(stagesL_[i], wetL, coeff);
    fbL_ = wetL;

    float lfoR = FastMath::fastSin(lfoPhase_ + 0.25f);
    float freqR = FastMath::fclamp(centerFreq + lfoR * sweepRange, 100.0f, 4000.0f);
    float wR = FastMath::kPi * freqR / sampleRate_;
    float wRPhase = wR * (1.0f / FastMath::kTwoPi);
    float tanwR = FastMath::fastTan(wRPhase);
    float coeffR = (tanwR - 1.0f) / (tanwR + 1.0f);

    float wetR = r + fbR_ * fb;
    for (int i = 0; i < numStages_; i++)
        wetR = ProcessAllPass(stagesR_[i], wetR, coeffR);
    fbR_ = wetR;

    l = dryL + wetL * mix_;
    r = dryR + wetR * mix_;
    l *= 0.7f;
    r *= 0.7f;
}

// ---------------------------------------------------------------------------
// Compressor: ProcessStereo — envelope follower + soft-knee gain
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void CompressorEffect::ProcessStereo(float &l, float &r)
{
    float inputLevel = FastMath::fmax(FastMath::fabs(l), FastMath::fabs(r));

    if (inputLevel > env_)
        env_ = attackCoef_ * env_ + (1.0f - attackCoef_) * inputLevel;
    else
        env_ = releaseCoef_ * env_ + (1.0f - releaseCoef_) * inputLevel;

    float gain = computeGainSoftKnee(env_);
    float totalGain = gain * makeupLin_;
    l *= totalGain;
    r *= totalGain;
}

// ---------------------------------------------------------------------------
// Chorus: ProcessStereo — LFO sine + interpolated delay read
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void ChorusEffect::ProcessStereo(float &l, float &r)
{
    float dryL = l, dryR = r;

    lfoPhase_ += lfoInc_;
    if (lfoPhase_ >= 1.0f)
        lfoPhase_ -= 1.0f;

    lfoPhaseR_ += lfoInc_;
    if (lfoPhaseR_ >= 1.0f)
        lfoPhaseR_ -= 1.0f;

    float baseDelayMs = 5.0f + delay_ * 20.0f;
    float baseDelaySamples = baseDelayMs * 0.001f * sampleRate_;

    float lfoL = FastMath::fastSin(lfoPhase_);
    float lfoR = FastMath::fastSin(lfoPhaseR_);
    float modDepthSamples = depth_ * 0.003f * sampleRate_;

    float delayL = baseDelaySamples + lfoL * modDepthSamples;
    float delayR = baseDelaySamples + lfoR * modDepthSamples;

    if (delayL < 1.0f)
        delayL = 1.0f;
    if (delayL > MAX_DELAY_SAMPLES - 2)
        delayL = MAX_DELAY_SAMPLES - 2;
    if (delayR < 1.0f)
        delayR = 1.0f;
    if (delayR > MAX_DELAY_SAMPLES - 2)
        delayR = MAX_DELAY_SAMPLES - 2;

    // Inline delay read with linear interpolation
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

    delayBufL_[writeIdx_] = l + wetL * feedback_ * 0.7f;
    delayBufR_[writeIdx_] = r + wetR * feedback_ * 0.7f;

    writeIdx_ = (writeIdx_ + 1) % MAX_DELAY_SAMPLES;

    l = dryL * (1.0f - mix_) + wetL * mix_;
    r = dryR * (1.0f - mix_) + wetR * mix_;
}

// ---------------------------------------------------------------------------
// Flanger: ProcessStereo — triangle LFO + interpolated delay read
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void FlangerEffect::ProcessStereo(float &l, float &r)
{
    float dryL = l, dryR = r;

    float lfoSigL = UpdateLfo(lfoPhaseL_);
    float lfoSigR = UpdateLfo(lfoPhaseR_);

    float delayL = 1.0f + lfoSigL + delaySamples_;
    float delayR = 1.0f + lfoSigR + delaySamples_;

    float wetL = ReadDelay(delayBufL_, delayL);
    float wetR = ReadDelay(delayBufR_, delayR);

    float fb = (feedback_ * 2.0f - 1.0f) * 0.95f;

    delayBufL_[writeIdx_] = l + wetL * fb;
    delayBufR_[writeIdx_] = r + wetR * fb;

    writeIdx_ = (writeIdx_ + 1) % MAX_DELAY_SAMPLES;

    l = dryL * (1.0f - mix_) + wetL * mix_;
    r = dryR * (1.0f - mix_) + wetR * mix_;
}

