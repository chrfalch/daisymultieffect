#pragma once

#include "core/audio/audio_processor.h"
#include "core/effects/delay.h"
#include "core/effects/stereo_sweep_delay.h"
#include "core/effects/reverb.h"

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
            float *combPtrs[4] = {
                reverbComb_[i][0].data(),
                reverbComb_[i][1].data(),
                reverbComb_[i][2].data(),
                reverbComb_[i][3].data()};
            float *apPtrs[2] = {
                reverbAp_[i][0].data(),
                reverbAp_[i][1].data()};
            processor.BindReverbBuffers(i, reverbPre_[i].data(), combPtrs, apPtrs);
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
                reverbComb_[i][c].resize(SimpleReverbEffect::Comb::MAX_DELAY, 0.0f);
            }
            for (int a = 0; a < 2; a++)
            {
                reverbAp_[i][a].resize(SimpleReverbEffect::Allpass::MAX_DELAY, 0.0f);
            }
        }
    }

    // Delay buffers
    std::vector<float> delayBufL_[2];
    std::vector<float> delayBufR_[2];

    // Sweep delay buffers
    std::vector<float> sweepBufL_[2];
    std::vector<float> sweepBufR_[2];

    // Reverb buffers
    std::vector<float> reverbPre_[2];
    std::vector<float> reverbComb_[2][4];
    std::vector<float> reverbAp_[2][2];
};
