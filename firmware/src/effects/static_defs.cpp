#include "effects/delay.h"
#include "effects/reverb.h"
#include "effects/shimmer_reverb.h"
#include "effects/pitch_shifter.h"
#include "effects/stereo_sweep_delay.h"
#include "effects/looper.h"
#include "audio/audio_processor.h"

#include "dev/sdram.h"

// ---- Per-instance buffer pools (allocated in SDRAM) ----
// Pool sizes match AudioProcessor::kMax* constants.

// DelayEffect: 2 instances x 2 channels x MAX_SAMPLES
static constexpr int kMaxDelays = 2;
DSY_SDRAM_BSS float g_delayBufL[kMaxDelays][DelayEffect::MAX_SAMPLES];
DSY_SDRAM_BSS float g_delayBufR[kMaxDelays][DelayEffect::MAX_SAMPLES];

// StereoSweepDelayEffect: 2 instances x 2 channels x MAX_SAMPLES
static constexpr int kMaxSweeps = 2;
DSY_SDRAM_BSS float g_sweepBufL[kMaxSweeps][StereoSweepDelayEffect::MAX_SAMPLES];
DSY_SDRAM_BSS float g_sweepBufR[kMaxSweeps][StereoSweepDelayEffect::MAX_SAMPLES];

// SimpleReverbEffect: 2 instances with pre-delay + stereo comb + stereo allpass buffers
static constexpr int kMaxReverbs = 2;
DSY_SDRAM_BSS float g_reverbPre[kMaxReverbs][SimpleReverbEffect::MAX_PRE];
DSY_SDRAM_BSS float g_reverbCombL[kMaxReverbs][4][SimpleReverbEffect::Comb::MAX_DELAY];
DSY_SDRAM_BSS float g_reverbCombR[kMaxReverbs][4][SimpleReverbEffect::Comb::MAX_DELAY];
DSY_SDRAM_BSS float g_reverbApL[kMaxReverbs][2][SimpleReverbEffect::Allpass::MAX_DELAY];
DSY_SDRAM_BSS float g_reverbApR[kMaxReverbs][2][SimpleReverbEffect::Allpass::MAX_DELAY];

// PitchShifterEffect: 4 instances x 2 channels x kBufSize
static constexpr int kMaxPitchShifters = 4;
DSY_SDRAM_BSS float g_pitchBufL[kMaxPitchShifters][PitchShifterDsp::kBufSize];
DSY_SDRAM_BSS float g_pitchBufR[kMaxPitchShifters][PitchShifterDsp::kBufSize];

// ShimmerReverbEffect: 2 instances with pre-delay + stereo comb + stereo allpass + shimmer pitch shifter
static constexpr int kMaxShimmerReverbs = 2;
DSY_SDRAM_BSS float g_shimRevPre[kMaxShimmerReverbs][ShimmerReverbEffect::MAX_PRE];
DSY_SDRAM_BSS float g_shimRevCombL[kMaxShimmerReverbs][4][ShimmerReverbEffect::Comb::MAX_DELAY];
DSY_SDRAM_BSS float g_shimRevCombR[kMaxShimmerReverbs][4][ShimmerReverbEffect::Comb::MAX_DELAY];
DSY_SDRAM_BSS float g_shimRevApL[kMaxShimmerReverbs][2][ShimmerReverbEffect::Allpass::MAX_DELAY];
DSY_SDRAM_BSS float g_shimRevApR[kMaxShimmerReverbs][2][ShimmerReverbEffect::Allpass::MAX_DELAY];
DSY_SDRAM_BSS float g_shimRevPitchBuf[kMaxShimmerReverbs][PitchShifterDsp::kBufSize];

// LooperEffect: 1 instance x 2 channels x MAX_SAMPLES
// Duration configured in looper.h via LOOPER_DURATION_SECONDS (currently 30s = ~11.5MB)
static constexpr int kMaxLoopers = 1;
DSY_SDRAM_BSS float g_looperBufL[kMaxLoopers][LooperEffect::MAX_SAMPLES];
DSY_SDRAM_BSS float g_looperBufR[kMaxLoopers][LooperEffect::MAX_SAMPLES];

// ---- Single entry point for binding all SDRAM buffers ----
void BindProcessorBuffers(AudioProcessor &processor)
{
    for (int i = 0; i < kMaxDelays; i++)
        processor.BindDelayBuffers(i, g_delayBufL[i], g_delayBufR[i]);

    for (int i = 0; i < kMaxSweeps; i++)
        processor.BindSweepBuffers(i, g_sweepBufL[i], g_sweepBufR[i]);

    for (int i = 0; i < kMaxReverbs; i++)
    {
        float *combPtrsL[4] = {g_reverbCombL[i][0], g_reverbCombL[i][1], g_reverbCombL[i][2], g_reverbCombL[i][3]};
        float *combPtrsR[4] = {g_reverbCombR[i][0], g_reverbCombR[i][1], g_reverbCombR[i][2], g_reverbCombR[i][3]};
        float *apPtrsL[2] = {g_reverbApL[i][0], g_reverbApL[i][1]};
        float *apPtrsR[2] = {g_reverbApR[i][0], g_reverbApR[i][1]};
        processor.BindReverbBuffers(i, g_reverbPre[i], combPtrsL, combPtrsR, apPtrsL, apPtrsR);
    }

    for (int i = 0; i < kMaxPitchShifters; i++)
        processor.BindPitchShifterBuffers(i, g_pitchBufL[i], g_pitchBufR[i]);

    for (int i = 0; i < kMaxShimmerReverbs; i++)
    {
        float *combPtrsL[4] = {g_shimRevCombL[i][0], g_shimRevCombL[i][1], g_shimRevCombL[i][2], g_shimRevCombL[i][3]};
        float *combPtrsR[4] = {g_shimRevCombR[i][0], g_shimRevCombR[i][1], g_shimRevCombR[i][2], g_shimRevCombR[i][3]};
        float *apPtrsL[2] = {g_shimRevApL[i][0], g_shimRevApL[i][1]};
        float *apPtrsR[2] = {g_shimRevApR[i][0], g_shimRevApR[i][1]};
        processor.BindShimmerReverbBuffers(i, g_shimRevPre[i], combPtrsL, combPtrsR, apPtrsL, apPtrsR, g_shimRevPitchBuf[i]);
    }

    for (int i = 0; i < kMaxLoopers; i++)
        processor.BindLooperBuffers(i, g_looperBufL[i], g_looperBufR[i]);
}
