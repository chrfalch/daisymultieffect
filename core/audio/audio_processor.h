
#pragma once

#include <cstddef>
#include <cstdint>

#include "patch/patch_protocol.h"
#include "audio/pedalboard.h"
#include "audio/tempo.h"

#include "effects/delay.h"
#include "effects/overdrive.h"
#include "effects/reverb.h"
#include "effects/stereo_mixer.h"
#include "effects/stereo_sweep_delay.h"
#include "effects/compressor.h"
#include "effects/chorus.h"
#include "effects/noise_gate.h"
#include "effects/eq.h"
#include "effects/flanger.h"
#include "effects/phaser.h"
#include "effects/neural_amp.h"

// Platform-agnostic audio processor.
// Manages effect instances and processes audio frames.
// Platforms must:
// 1. Create an instance
// 2. Bind buffers to delay/reverb effects via BindBuffers()
// 3. Call Init() with sample rate
// 4. Call ProcessFrame() or ProcessBlock() from audio callback
class AudioProcessor
{
public:
    explicit AudioProcessor(TempoSource &tempo);

    void Init(float sampleRate);

    // Apply a patch configuration
    void ApplyPatch(const PatchWireDesc &pw);

    // Process a single stereo frame
    void ProcessFrame(float inL, float inR, float &outL, float &outR);

    // Process a block of audio (convenience wrapper)
    template <typename InBuf, typename OutBuf>
    void ProcessBlock(InBuf in, OutBuf out, size_t size)
    {
        for (size_t i = 0; i < size; i++)
        {
            float l, r;
            ProcessFrame(in[0][i], in[1][i], l, r);
            out[0][i] = l;
            out[1][i] = r;
        }
    }

    // Access to the runtime board for slot manipulation
    PedalBoardRuntime<12> &Board() { return board_; }

    // ---- Buffer binding for effects that need external memory ----
    // Call these before Init() if you're using delays/reverbs

    void BindDelayBuffers(int index, float *bufL, float *bufR)
    {
        if (index >= 0 && index < kMaxDelays)
            fx_delays_[index].BindBuffers(bufL, bufR);
    }

    void BindSweepBuffers(int index, float *bufL, float *bufR)
    {
        if (index >= 0 && index < kMaxSweeps)
            fx_sweeps_[index].BindBuffers(bufL, bufR);
    }

    void BindReverbBuffers(int index, float *preBuf, float *combBufs[4], float *apBufs[2])
    {
        if (index >= 0 && index < kMaxReverbs)
            fx_reverbs_[index].BindBuffers(preBuf, combBufs, apBufs);
    }

    // Get pool sizes for buffer allocation
    static constexpr int GetMaxDelays() { return kMaxDelays; }
    static constexpr int GetMaxSweeps() { return kMaxSweeps; }
    static constexpr int GetMaxReverbs() { return kMaxReverbs; }

    // Access to Neural Amp effects for model loading
    NeuralAmpEffect *GetNeuralAmpEffect(int index)
    {
        if (index >= 0 && index < kMaxNeuralAmps)
            return &fx_neuralamps_[index];
        return nullptr;
    }

    static constexpr int GetMaxNeuralAmps() { return kMaxNeuralAmps; }

private:
    BaseEffect *Instantiate(uint8_t typeId, int slotIndex);

    TempoSource &tempo_;
    PedalBoardRuntime<12> board_;

    // Effect pools
    static constexpr int kMaxDelays = 2;
    static constexpr int kMaxSweeps = 2;
    static constexpr int kMaxDistortions = 4;
    static constexpr int kMaxMixers = 2;
    static constexpr int kMaxReverbs = 2;
    static constexpr int kMaxCompressors = 4;
    static constexpr int kMaxChoruses = 4;
    static constexpr int kMaxNoiseGates = 4;
    static constexpr int kMaxEQs = 4;
    static constexpr int kMaxFlangers = 4;
    static constexpr int kMaxPhasers = 4;
    static constexpr int kMaxNeuralAmps = 2; // Neural amps are CPU-intensive

    DelayEffect fx_delays_[kMaxDelays];
    StereoSweepDelayEffect fx_sweeps_[kMaxSweeps];
    OverdriveEffect fx_dists_[kMaxDistortions];
    StereoMixerEffect fx_mixers_[kMaxMixers];
    SimpleReverbEffect fx_reverbs_[kMaxReverbs];
    CompressorEffect fx_compressors_[kMaxCompressors];
    ChorusEffect fx_choruses_[kMaxChoruses];
    NoiseGateEffect fx_noisegates_[kMaxNoiseGates];
    GraphicEQEffect fx_eqs_[kMaxEQs];
    FlangerEffect fx_flangers_[kMaxFlangers];
    PhaserEffect fx_phasers_[kMaxPhasers];
    NeuralAmpEffect fx_neuralamps_[kMaxNeuralAmps];

    // Pool counters
    int delay_next_ = 0;
    int sweep_next_ = 0;
    int dist_next_ = 0;
    int mixer_next_ = 0;
    int reverb_next_ = 0;
    int compressor_next_ = 0;
    int chorus_next_ = 0;
    int noisegate_next_ = 0;
    int eq_next_ = 0;
    int flanger_next_ = 0;
    int phaser_next_ = 0;
    int neuralamp_next_ = 0;
};
