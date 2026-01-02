#pragma once

#include <cstddef>
#include <cstdint>

#include "daisy_seed.h"
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

// Owns the DSP graph/runtime state (effects, routing, patch application).
// Keeps main.cpp small and focused on hardware + scheduling.
class AudioEngine
{
public:
    explicit AudioEngine(TempoSource &tempo);

    void Init(float sample_rate);

    void ApplyPatch(const PatchWireDesc &pw);

    void ProcessBlock(daisy::AudioHandle::InputBuffer in,
                      daisy::AudioHandle::OutputBuffer out,
                      size_t size);

    PedalBoardRuntime<12> &Board() { return board_; }

private:
    BaseEffect *Instantiate(uint8_t typeId, int slotIndex);
    void ProcessFrame(float inL, float inR, float &outL, float &outR);

    TempoSource &tempo_;
    PedalBoardRuntime<12> board_;

    // Effect pools - each slot gets its own instance to have independent parameters.
    // Pool sizes are limited by memory (delay buffers use SDRAM).
    static constexpr int kMaxDelays = 2;
    static constexpr int kMaxSweeps = 2;
    static constexpr int kMaxDistortions = 4;
    static constexpr int kMaxMixers = 2;
    static constexpr int kMaxReverbs = 2;
    static constexpr int kMaxCompressors = 4;
    static constexpr int kMaxChoruses = 4;

    DelayEffect fx_delays_[kMaxDelays];
    StereoSweepDelayEffect fx_sweeps_[kMaxSweeps];
    OverdriveEffect fx_dists_[kMaxDistortions];
    StereoMixerEffect fx_mixers_[kMaxMixers];
    SimpleReverbEffect fx_reverbs_[kMaxReverbs];
    CompressorEffect fx_compressors_[kMaxCompressors];
    ChorusEffect fx_choruses_[kMaxChoruses];

    // Track which pool instances are in use (reset on ApplyPatch).
    int delay_next_ = 0;
    int sweep_next_ = 0;
    int dist_next_ = 0;
    int mixer_next_ = 0;
    int reverb_next_ = 0;
    int compressor_next_ = 0;
    int chorus_next_ = 0;
};
