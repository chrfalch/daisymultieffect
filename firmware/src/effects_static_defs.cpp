#include "effects_delay.h"
#include "effects_distortion.h"
#include "effects_reverb.h"
#include "effects_stereo_mixer.h"
#include "effects_stereo_sweep_delay.h"

#include "dev/sdram.h"

// ---- DelayEffect ----
DSY_SDRAM_BSS float DelayEffect::bufL[DelayEffect::MAX_SAMPLES];
DSY_SDRAM_BSS float DelayEffect::bufR[DelayEffect::MAX_SAMPLES];

const NumberParamRange DelayEffect::kFbRange = {0.0f, 0.95f, 0.01f};
const NumberParamRange DelayEffect::kMixRange = {0.0f, 1.0f, 0.01f};

const ParamInfo DelayEffect::kParams[5] = {
    {0, "Free Time", "Delay time ms if not synced", ParamValueKind::Number, nullptr, nullptr},
    {1, "Division", "Beat division index", ParamValueKind::Number, nullptr, nullptr},
    {2, "Synced", "0/1 tempo synced", ParamValueKind::Number, nullptr, nullptr},
    {3, "Feedback", "Delay feedback", ParamValueKind::Number, &kFbRange, nullptr},
    {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &kMixRange, nullptr},
};

const EffectMeta DelayEffect::kMeta = {"Delay", "Tempo-synced delay.", kParams, 5};

// ---- StereoSweepDelayEffect ----
DSY_SDRAM_BSS float StereoSweepDelayEffect::bufL[StereoSweepDelayEffect::MAX_SAMPLES];
DSY_SDRAM_BSS float StereoSweepDelayEffect::bufR[StereoSweepDelayEffect::MAX_SAMPLES];

const NumberParamRange StereoSweepDelayEffect::kFbRange = {0.0f, 0.95f, 0.01f};
const NumberParamRange StereoSweepDelayEffect::kMixRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange StereoSweepDelayEffect::kPanDepthRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange StereoSweepDelayEffect::kPanRateRange = {0.05f, 5.0f, 0.01f};

const ParamInfo StereoSweepDelayEffect::kParams[7] = {
    {0, "Free Time", "Delay time if not synced", ParamValueKind::Number, nullptr, nullptr},
    {1, "Division", "Beat division index", ParamValueKind::Number, nullptr, nullptr},
    {2, "Synced", "0/1 tempo synced", ParamValueKind::Number, nullptr, nullptr},
    {3, "Feedback", "Feedback", ParamValueKind::Number, &kFbRange, nullptr},
    {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &kMixRange, nullptr},
    {5, "Pan Depth", "Pan sweep depth", ParamValueKind::Number, &kPanDepthRange, nullptr},
    {6, "Pan Rate", "Pan rate (Hz)", ParamValueKind::Number, &kPanRateRange, nullptr},
};

const EffectMeta StereoSweepDelayEffect::kMeta = {"Sweep Delay", "Stereo delay with pan sweep.", kParams, 7};

// ---- DistortionEffect ----
const NumberParamRange DistortionEffect::kDriveRange = {1.0f, 20.0f, 0.1f};
const NumberParamRange DistortionEffect::kToneRange = {0.0f, 1.0f, 0.01f};

const ParamInfo DistortionEffect::kParams[2] = {
    {0, "Drive", "Input drive amount", ParamValueKind::Number, &kDriveRange, nullptr},
    {1, "Tone", "Darker/brighter blend", ParamValueKind::Number, &kToneRange, nullptr},
};

const EffectMeta DistortionEffect::kMeta = {"Distortion", "Simple tanh distortion + tone.", kParams, 2};

// ---- StereoMixerEffect ----
const NumberParamRange StereoMixerEffect::kMixRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange StereoMixerEffect::kCrossRange = {0.0f, 1.0f, 0.01f};

const ParamInfo StereoMixerEffect::kParams[3] = {
    {0, "Mix A", "Level for branch A", ParamValueKind::Number, &kMixRange, nullptr},
    {1, "Mix B", "Level for branch B", ParamValueKind::Number, &kMixRange, nullptr},
    {2, "Cross", "Cross-couple A/B", ParamValueKind::Number, &kCrossRange, nullptr},
};

const EffectMeta StereoMixerEffect::kMeta = {"Stereo Mixer", "Mix two branches into stereo.", kParams, 3};

// ---- SimpleReverbEffect ----
DSY_SDRAM_BSS float SimpleReverbEffect::preBuf[SimpleReverbEffect::MAX_PRE];
DSY_SDRAM_BSS float SimpleReverbEffect::combBuf[4][SimpleReverbEffect::Comb::MAX_DELAY];
DSY_SDRAM_BSS float SimpleReverbEffect::apBuf[2][SimpleReverbEffect::Allpass::MAX_DELAY];

const NumberParamRange SimpleReverbEffect::kMixRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange SimpleReverbEffect::kDecayRange = {0.2f, 0.95f, 0.01f};
const NumberParamRange SimpleReverbEffect::kDampRange = {0.0f, 0.8f, 0.01f};
const NumberParamRange SimpleReverbEffect::kPreRange = {0.0f, 80.0f, 1.0f};
const NumberParamRange SimpleReverbEffect::kSizeRange = {0.0f, 1.0f, 0.01f};

const ParamInfo SimpleReverbEffect::kParams[5] = {
    {0, "Mix", "Wet/dry mix", ParamValueKind::Number, &kMixRange, nullptr},
    {1, "Decay", "Reverb decay", ParamValueKind::Number, &kDecayRange, nullptr},
    {2, "Damping", "High damping", ParamValueKind::Number, &kDampRange, nullptr},
    {3, "PreDelay", "Pre-delay (ms)", ParamValueKind::Number, &kPreRange, nullptr},
    {4, "Size", "Room size", ParamValueKind::Number, &kSizeRange, nullptr},
};

const EffectMeta SimpleReverbEffect::kMeta = {"Reverb", "Simple Schroeder reverb.", kParams, 5};
