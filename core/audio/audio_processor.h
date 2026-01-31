
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
#include "effects/cabinet_ir.h"

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

    // Input/Output gain staging for instrument-to-line level conversion
    // Input gain: boost low-level instrument signals before processing
    // Output gain: scale output to match line level expectations
    void SetInputGain(float gain) { inputGain_ = gain; }
    void SetOutputGain(float gain) { outputGain_ = gain; }
    float GetInputGain() const { return inputGain_; }
    float GetOutputGain() const { return outputGain_; }

    // Global bypass: skip all processing, pass audio straight through
    // with only gain staging applied. Reduces CPU to near-zero.
    void SetGlobalBypass(bool bypass) { globalBypass_ = bypass; }
    bool GetGlobalBypass() const { return globalBypass_; }

    // Get peak levels (post-gain for input, post-processing for output)
    // Call ResetPeakLevels() after reading to start fresh measurement
    float GetInputPeakLevel() const { return inputPeakLevel_; }
    float GetOutputPeakLevel() const { return outputPeakLevel_; }
    void ResetPeakLevels()
    {
        inputPeakLevel_ = 0.0f;
        outputPeakLevel_ = 0.0f;
    }

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

    void BindReverbBuffers(int index, float *preBuf,
                           float *combBufsL[4], float *combBufsR[4],
                           float *apBufsL[2], float *apBufsR[2])
    {
        if (index >= 0 && index < kMaxReverbs)
            fx_reverbs_[index].BindBuffers(preBuf, combBufsL, combBufsR, apBufsL, apBufsR);
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

    // Access to Cabinet IR effects for IR loading
    CabinetIREffect *GetCabinetIREffect(int index)
    {
        if (index >= 0 && index < kMaxCabinetIRs)
            return &fx_cabinetirs_[index];
        return nullptr;
    }

    static constexpr int GetMaxCabinetIRs() { return kMaxCabinetIRs; }

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
    static constexpr int kMaxCabinetIRs = 2; // Cabinet IRs use convolution

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
    CabinetIREffect fx_cabinetirs_[kMaxCabinetIRs];

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
    int cabinetir_next_ = 0;

    // Input/output gain staging
    // Default: +18dB input boost to bring instrument level signals
    // into a range where effects (especially overdrive) respond properly.
    // Guitar pickups output ~0.1 peak, this brings it to ~0.8 peak.
    float inputGain_ = 8.0f;  // ~+18dB boost for instrument level input
    float outputGain_ = 1.0f; // Unity output gain
    bool globalBypass_ = false;

    // Peak level tracking (updated per frame)
    float inputPeakLevel_ = 0.0f;  // Post-gain input peak
    float outputPeakLevel_ = 0.0f; // Post-processing output peak
};
