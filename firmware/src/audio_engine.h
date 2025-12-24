#pragma once

#include <cstddef>
#include <cstdint>

#include "daisy_seed.h"
#include "patch_protocol.h"
#include "pedalboard.h"
#include "tempo.h"

#include "effects_delay.h"
#include "effects_distortion.h"
#include "effects_reverb.h"
#include "effects_stereo_mixer.h"
#include "effects_stereo_sweep_delay.h"

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

    DelayEffect fx_delay_;
    StereoSweepDelayEffect fx_sweep_;
    DistortionEffect fx_distA_;
    DistortionEffect fx_distB_;
    StereoMixerEffect fx_stMix_;
    SimpleReverbEffect fx_reverb_;
};
