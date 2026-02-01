#pragma once

#include "core/audio/audio_processor.h"
#include "core/effects/delay.h"
#include "core/effects/stereo_sweep_delay.h"
#include "core/effects/reverb.h"
#include "core/effects/pitch_shifter.h"
#include "core/effects/shimmer_reverb.h"

#include <memory>
#include <vector>

// Manages heap-allocated buffers for delay lines and reverb.
// On hardware (Daisy Seed), these live in SDRAM.
// For the VST plugin, we allocate them on the heap.
class BufferManager
{
public:
    BufferManager()
    {
        // Pre-allocate all buffers
        AllocateDelayBuffers();
        AllocateSweepBuffers();
        AllocateReverbBuffers();
        AllocatePitchShifterBuffers();
        AllocateShimmerReverbBuffers();
    }

    void BindTo(AudioProcessor &processor)
    {
        // Bind delay buffers
        for (int i = 0; i < AudioProcessor::GetMaxDelays(); i++)
        {
            processor.BindDelayBuffers(i, delayBufL_[i].data(), delayBufR_[i].data());
        }

        // Bind sweep delay buffers
        for (int i = 0; i < AudioProcessor::GetMaxSweeps(); i++)
        {
            processor.BindSweepBuffers(i, sweepBufL_[i].data(), sweepBufR_[i].data());
        }

        // Bind reverb buffers
        for (int i = 0; i < AudioProcessor::GetMaxReverbs(); i++)
        {
            float *combPtrsL[4] = {
                reverbCombL_[i][0].data(),
                reverbCombL_[i][1].data(),
                reverbCombL_[i][2].data(),
                reverbCombL_[i][3].data()};
            float *combPtrsR[4] = {
                reverbCombR_[i][0].data(),
                reverbCombR_[i][1].data(),
                reverbCombR_[i][2].data(),
                reverbCombR_[i][3].data()};
            float *apPtrsL[2] = {
                reverbApL_[i][0].data(),
                reverbApL_[i][1].data()};
            float *apPtrsR[2] = {
                reverbApR_[i][0].data(),
                reverbApR_[i][1].data()};
            processor.BindReverbBuffers(i, reverbPre_[i].data(), combPtrsL, combPtrsR, apPtrsL, apPtrsR);
        }

        // Bind pitch shifter buffers
        for (int i = 0; i < AudioProcessor::GetMaxPitchShifters(); i++)
        {
            processor.BindPitchShifterBuffers(i, pitchBufL_[i].data(), pitchBufR_[i].data());
        }

        // Bind shimmer reverb buffers
        for (int i = 0; i < AudioProcessor::GetMaxShimmerReverbs(); i++)
        {
            float *combPtrsL[4] = {
                shimRevCombL_[i][0].data(),
                shimRevCombL_[i][1].data(),
                shimRevCombL_[i][2].data(),
                shimRevCombL_[i][3].data()};
            float *combPtrsR[4] = {
                shimRevCombR_[i][0].data(),
                shimRevCombR_[i][1].data(),
                shimRevCombR_[i][2].data(),
                shimRevCombR_[i][3].data()};
            float *apPtrsL[2] = {
                shimRevApL_[i][0].data(),
                shimRevApL_[i][1].data()};
            float *apPtrsR[2] = {
                shimRevApR_[i][0].data(),
                shimRevApR_[i][1].data()};
            processor.BindShimmerReverbBuffers(i, shimRevPre_[i].data(), combPtrsL, combPtrsR, apPtrsL, apPtrsR, shimRevPitchBuf_[i].data());
        }
    }

private:
    void AllocateDelayBuffers()
    {
        for (int i = 0; i < AudioProcessor::GetMaxDelays(); i++)
        {
            delayBufL_[i].resize(DelayEffect::MAX_SAMPLES, 0.0f);
            delayBufR_[i].resize(DelayEffect::MAX_SAMPLES, 0.0f);
        }
    }

    void AllocateSweepBuffers()
    {
        for (int i = 0; i < AudioProcessor::GetMaxSweeps(); i++)
        {
            sweepBufL_[i].resize(StereoSweepDelayEffect::MAX_SAMPLES, 0.0f);
            sweepBufR_[i].resize(StereoSweepDelayEffect::MAX_SAMPLES, 0.0f);
        }
    }

    void AllocateReverbBuffers()
    {
        for (int i = 0; i < AudioProcessor::GetMaxReverbs(); i++)
        {
            reverbPre_[i].resize(SimpleReverbEffect::MAX_PRE, 0.0f);
            for (int c = 0; c < 4; c++)
            {
                reverbCombL_[i][c].resize(SimpleReverbEffect::Comb::MAX_DELAY, 0.0f);
                reverbCombR_[i][c].resize(SimpleReverbEffect::Comb::MAX_DELAY, 0.0f);
            }
            for (int a = 0; a < 2; a++)
            {
                reverbApL_[i][a].resize(SimpleReverbEffect::Allpass::MAX_DELAY, 0.0f);
                reverbApR_[i][a].resize(SimpleReverbEffect::Allpass::MAX_DELAY, 0.0f);
            }
        }
    }

    void AllocatePitchShifterBuffers()
    {
        for (int i = 0; i < AudioProcessor::GetMaxPitchShifters(); i++)
        {
            pitchBufL_[i].resize(PitchShifterDsp::kBufSize, 0.0f);
            pitchBufR_[i].resize(PitchShifterDsp::kBufSize, 0.0f);
        }
    }

    void AllocateShimmerReverbBuffers()
    {
        for (int i = 0; i < AudioProcessor::GetMaxShimmerReverbs(); i++)
        {
            shimRevPre_[i].resize(ShimmerReverbEffect::MAX_PRE, 0.0f);
            for (int c = 0; c < 4; c++)
            {
                shimRevCombL_[i][c].resize(ShimmerReverbEffect::Comb::MAX_DELAY, 0.0f);
                shimRevCombR_[i][c].resize(ShimmerReverbEffect::Comb::MAX_DELAY, 0.0f);
            }
            for (int a = 0; a < 2; a++)
            {
                shimRevApL_[i][a].resize(ShimmerReverbEffect::Allpass::MAX_DELAY, 0.0f);
                shimRevApR_[i][a].resize(ShimmerReverbEffect::Allpass::MAX_DELAY, 0.0f);
            }
            shimRevPitchBuf_[i].resize(PitchShifterDsp::kBufSize, 0.0f);
        }
    }

    // Delay buffers
    std::vector<float> delayBufL_[2];
    std::vector<float> delayBufR_[2];

    // Sweep delay buffers
    std::vector<float> sweepBufL_[2];
    std::vector<float> sweepBufR_[2];

    // Reverb buffers (stereo L/R)
    std::vector<float> reverbPre_[2];
    std::vector<float> reverbCombL_[2][4];
    std::vector<float> reverbCombR_[2][4];
    std::vector<float> reverbApL_[2][2];
    std::vector<float> reverbApR_[2][2];

    // Pitch shifter buffers (stereo L/R)
    std::vector<float> pitchBufL_[4];
    std::vector<float> pitchBufR_[4];

    // Shimmer reverb buffers (stereo L/R + shimmer pitch shifter)
    std::vector<float> shimRevPre_[2];
    std::vector<float> shimRevCombL_[2][4];
    std::vector<float> shimRevCombR_[2][4];
    std::vector<float> shimRevApL_[2][2];
    std::vector<float> shimRevApR_[2][2];
    std::vector<float> shimRevPitchBuf_[2];
};
