#include "effects/delay.h"
#include "effects/reverb.h"
#include "effects/stereo_sweep_delay.h"
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
}
