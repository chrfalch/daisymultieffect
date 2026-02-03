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
#include "effects/shimmer_reverb.h"
#include "effects/eq.h"
#include "effects/phaser.h"
#include "effects/compressor.h"
#include "effects/chorus.h"
#include "effects/flanger.h"
#include "effects/overdrive.h"
#include "effects/noise_gate.h"
#include "effects/delay.h"
#include "effects/stereo_sweep_delay.h"
#include "effects/stereo_mixer.h"
#include "effects/tremolo.h"
#include "effects/leslie.h"

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
// Shimmer Reverb: ProcessTank — stereo 4+4 comb + 2+2 allpass (same as reverb)
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void ShimmerReverbEffect::ProcessTank(float inL, float inR, float &outL, float &outR)
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

// ---------------------------------------------------------------------------
// Overdrive: ProcessStereo — pre-HP, soft clip, post-LP tone control
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void OverdriveEffect::ProcessStereo(float &l, float &r)
{
    hpL_ += hpCoeff_ * (l - hpL_);
    hpR_ += hpCoeff_ * (r - hpR_);
    float hpOutL = l - hpL_;
    float hpOutR = r - hpR_;

    float clipL = SoftClip(hpOutL * preGain_) * postGain_;
    float clipR = SoftClip(hpOutR * preGain_) * postGain_;

    lpL_ += lpCoeff_ * (clipL - lpL_);
    lpR_ += lpCoeff_ * (clipR - lpR_);

    l = lpL_;
    r = lpR_;
}

// ---------------------------------------------------------------------------
// NoiseGate: ProcessStereo — envelope follower with hold + range
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void NoiseGateEffect::ProcessStereo(float &l, float &r)
{
    float inputLevel = FastMath::fmax(FastMath::fabs(l), FastMath::fabs(r));

    if (inputLevel > threshLin_)
    {
        holdCounter_ = hold_ * sampleRate_;
        gateGain_ = attackCoef_ * gateGain_ + (1.0f - attackCoef_) * 1.0f;
    }
    else if (holdCounter_ > 0.0f)
    {
        holdCounter_ -= 1.0f;
    }
    else
    {
        gateGain_ = releaseCoef_ * gateGain_;
    }

    float effectiveGain = range_ + (1.0f - range_) * gateGain_;
    l *= effectiveGain;
    r *= effectiveGain;
}

// ---------------------------------------------------------------------------
// Delay: ProcessStereo — simple stereo delay with feedback
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void DelayEffect::ProcessStereo(float &l, float &r)
{
    if (!bufL_ || !bufR_)
        return;
    int d = GetPeriodSamples();
    if (d >= MAX_SAMPLES)
        d = MAX_SAMPLES - 1;
    int rp = wp - d;
    if (rp < 0)
        rp += MAX_SAMPLES;
    float dl = bufL_[rp], dr = bufR_[rp];
    float inL = l, inR = r;
    bufL_[wp] = inL + dl * feedback_;
    bufR_[wp] = inR + dr * feedback_;
    if (++wp >= MAX_SAMPLES)
        wp = 0;
    float dry = 1.0f - mix_, wet = mix_;
    l = inL * dry + dl * wet;
    r = inR * dry + dr * wet;
}

// ---------------------------------------------------------------------------
// StereoSweepDelay: ProcessStereo — delay with panning LFO
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void StereoSweepDelayEffect::ProcessStereo(float &l, float &r)
{
    if (!bufL_ || !bufR_)
        return;
    float inL = l, inR = r;

    int d = GetPeriodSamples();
    if (d >= MAX_SAMPLES)
        d = MAX_SAMPLES - 1;
    int rp = wp - d;
    if (rp < 0)
        rp += MAX_SAMPLES;

    float dl = bufL_[rp], dr = bufR_[rp];

    // pan LFO using pre-computed increment and fast sine
    phase_ += panInc_;
    if (phase_ >= 1.0f)
        phase_ -= 1.0f;
    float pan = 0.5f * (1.0f + FastMath::fastSin(phase_)); // 0..1
    float baseL = 1.0f - pan, baseR = pan;
    float panL = (1.0f - panDepth_) * 0.5f + panDepth_ * baseL;
    float panR = (1.0f - panDepth_) * 0.5f + panDepth_ * baseR;

    float inMono = 0.5f * (inL + inR);
    bufL_[wp] = inMono + dl * feedback_;
    bufR_[wp] = inMono + dr * feedback_;
    if (++wp >= MAX_SAMPLES)
        wp = 0;

    float wetL = dl * panL, wetR = dr * panR;
    float dry = 1.0f - mix_, wet = mix_;
    l = inL * dry + wetL * wet;
    r = inR * dry + wetR * wet;
}

// ---------------------------------------------------------------------------
// Tremolo: ProcessStereo — LFO-modulated amplitude
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void TremoloEffect::ProcessStereo(float &l, float &r)
{
    // Advance LFO phase
    lfoPhase_ += lfoInc_;
    if (lfoPhase_ >= 1.0f)
        lfoPhase_ -= 1.0f;

    // Compute LFO value (0..1)
    float lfo = computeLfo(lfoPhase_);

    // Compute gain: 1.0 at lfo=0, (1-depth) at lfo=1
    float gainL = 1.0f - depth_ * lfo;

    if (stereo_ > 0.5f)
    {
        // Stereo mode: opposite-phase LFO for right channel
        float phaseR = lfoPhase_ + 0.5f;
        if (phaseR >= 1.0f)
            phaseR -= 1.0f;
        float lfoR = computeLfo(phaseR);
        float gainR = 1.0f - depth_ * lfoR;
        l *= gainL;
        r *= gainR;
    }
    else
    {
        // Mono mode: same modulation on both channels
        l *= gainL;
        r *= gainL;
    }
}

// ---------------------------------------------------------------------------
// StereoMixer: ProcessStereo — A/B mix with crossfade and limiter
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void StereoMixerEffect::ProcessStereo(float &l, float &r)
{
    float a = l * mixA_;
    float b = r * mixB_;
    float outL = (1.0f - cross_) * a + cross_ * b;
    float outR = (1.0f - cross_) * b + cross_ * a;

    float maxAbs = FastMath::fmax(FastMath::fabs(outL), FastMath::fabs(outR));
    if (maxAbs > 1.0f)
    {
        float g = 1.0f / maxAbs;
        outL *= g;
        outR *= g;
    }

    l = outL;
    r = outR;
}

// ---------------------------------------------------------------------------
// Leslie: ProcessRotor — Doppler + amplitude modulation helper
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void LeslieEffect::ProcessRotor(float inL, float inR, float angle, float radius,
                                float *delayBufL, float *delayBufR,
                                float &outL, float &outR)
{
    // Virtual microphones at ±90° (left/right)
    const float micAngleL = FastMath::kHalfPi;      // 90° left
    const float micAngleR = -FastMath::kHalfPi;     // -90° right
    
    // Calculate Doppler delay in samples
    float dopplerScale = (radius / SPEED_OF_SOUND) * sampleRate_;
    
    // For left channel: angle relative to left microphone
    float angleRelL = angle - micAngleL;
    float dopplerDelayL = dopplerScale * FastMath::fastCos(angleRelL);
    
    // For right channel: angle relative to right microphone
    float angleRelR = angle - micAngleR;
    float dopplerDelayR = dopplerScale * FastMath::fastCos(angleRelR);
    
    // Base delay to keep all reads positive
    float baseDelay = DELAY_BUF_SIZE / 2.0f;
    float readDelayL = baseDelay + dopplerDelayL;
    float readDelayR = baseDelay + dopplerDelayR;
    
    // Read from delay buffers with interpolation
    float dopplerL = ReadDelayInterpolated(delayBufL, readDelayL);
    float dopplerR = ReadDelayInterpolated(delayBufR, readDelayR);
    
    // Amplitude modulation (volume changes based on direction)
    float ampModL = 0.6f + 0.4f * FastMath::fastCos(angleRelL);
    float ampModR = 0.6f + 0.4f * FastMath::fastCos(angleRelR);
    
    // Stereo panning (position in stereo field)
    float pan = FastMath::fastSin(angle) * separation_;
    float panL = 0.5f - pan * 0.5f;
    float panR = 0.5f + pan * 0.5f;
    
    // Combine modulations
    outL = dopplerL * ampModL * panL;
    outR = dopplerR * ampModR * panR;
    
    // Write input to delay buffers
    delayBufL[delayWriteIdx_] = inL;
    delayBufR[delayWriteIdx_] = inR;
}

// ---------------------------------------------------------------------------
// Leslie: ProcessStereo — Full rotary speaker simulation
// ---------------------------------------------------------------------------
ITCMRAM_CODE __attribute__((noinline))
void LeslieEffect::ProcessStereo(float &l, float &r)
{
    float dryL = l, dryR = r;
    
    // 1. Optional input drive (soft saturation)
    if (drive_ > 0.01f)
    {
        float driveAmount = drive_ * 3.0f;
        l = SoftClip(l * (1.0f + driveAmount)) / (1.0f + driveAmount * 0.5f);
        r = SoftClip(r * (1.0f + driveAmount)) / (1.0f + driveAmount * 0.5f);
    }
    
    // 2. Crossover filtering
    // Horn path: 4th-order highpass
    float hornL = hornHP1L_.Process(l);
    hornL = hornHP2L_.Process(hornL);
    float hornR = hornHP1R_.Process(r);
    hornR = hornHP2R_.Process(hornR);
    
    // Bass path: 4th-order lowpass
    float bassL = bassLP1L_.Process(l);
    bassL = bassLP2L_.Process(bassL);
    float bassR = bassLP1R_.Process(r);
    bassR = bassLP2R_.Process(bassR);
    
    // 3. Speed smoothing (exponential approach to target)
    hornSpeed_ += (hornTargetSpeed_ - hornSpeed_) * accelCoeff_;
    bassSpeed_ += (bassTargetSpeed_ - bassSpeed_) * accelCoeff_;
    
    // 4. Update rotor angles
    float hornInc = FastMath::kTwoPi * hornSpeed_ / sampleRate_;
    float bassInc = FastMath::kTwoPi * bassSpeed_ / sampleRate_;
    hornAngle_ += hornInc;
    bassAngle_ += bassInc;
    
    // Wrap angles to [0, 2π)
    if (hornAngle_ >= FastMath::kTwoPi)
        hornAngle_ -= FastMath::kTwoPi;
    if (bassAngle_ >= FastMath::kTwoPi)
        bassAngle_ -= FastMath::kTwoPi;
    
    // 5. Process horn rotor
    float hornOutL, hornOutR;
    ProcessRotor(hornL, hornR, hornAngle_, HORN_RADIUS,
                hornDelayL_, hornDelayR_, hornOutL, hornOutR);
    
    // 6. Process bass rotor
    float bassOutL, bassOutR;
    ProcessRotor(bassL, bassR, bassAngle_, BASS_RADIUS,
                bassDelayL_, bassDelayR_, bassOutL, bassOutR);
    
    // 7. Mix rotors with level controls
    float wetL = hornOutL * hornLevel_ + bassOutL * bassLevel_;
    float wetR = hornOutR * hornLevel_ + bassOutR * bassLevel_;
    
    // 8. Wet/dry mix
    l = dryL * (1.0f - mix_) + wetL * mix_;
    r = dryR * (1.0f - mix_) + wetR * mix_;
    
    // Advance delay write pointer
    delayWriteIdx_ = (delayWriteIdx_ + 1) % DELAY_BUF_SIZE;
}
