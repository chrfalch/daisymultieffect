// Static definitions for effect metadata
// These are shared between all platforms (firmware, VST, etc.)

#include "effects/delay.h"
#include "effects/overdrive.h"
#include "effects/reverb.h"
#include "effects/stereo_mixer.h"
#include "effects/stereo_sweep_delay.h"
#include "effects/compressor.h"
#include "effects/chorus.h"

// ---- DelayEffect ----
const NumberParamRange DelayEffect::kFbRange = {0.0f, 0.95f, 0.01f};
const NumberParamRange DelayEffect::kMixRange = {0.0f, 1.0f, 0.01f};

const ParamInfo DelayEffect::kParams[5] = {
    {0, "Free Time", "Delay time ms if not synced", ParamValueKind::Number, nullptr, nullptr},
    {1, "Division", "Beat division index", ParamValueKind::Number, nullptr, nullptr},
    {2, "Synced", "0/1 tempo synced", ParamValueKind::Number, nullptr, nullptr},
    {3, "Feedback", "Delay feedback", ParamValueKind::Number, &DelayEffect::kFbRange, nullptr},
    {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &DelayEffect::kMixRange, nullptr},
};

const EffectMeta DelayEffect::kMeta = {"Delay", "Tempo-synced delay.", DelayEffect::kParams, 5};

// ---- StereoSweepDelayEffect ----
const NumberParamRange StereoSweepDelayEffect::kFbRange = {0.0f, 0.95f, 0.01f};
const NumberParamRange StereoSweepDelayEffect::kMixRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange StereoSweepDelayEffect::kPanDepthRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange StereoSweepDelayEffect::kPanRateRange = {0.05f, 5.0f, 0.01f};

const ParamInfo StereoSweepDelayEffect::kParams[7] = {
    {0, "Free Time", "Delay time if not synced", ParamValueKind::Number, nullptr, nullptr},
    {1, "Division", "Beat division index", ParamValueKind::Number, nullptr, nullptr},
    {2, "Synced", "0/1 tempo synced", ParamValueKind::Number, nullptr, nullptr},
    {3, "Feedback", "Feedback", ParamValueKind::Number, &StereoSweepDelayEffect::kFbRange, nullptr},
    {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &StereoSweepDelayEffect::kMixRange, nullptr},
    {5, "Pan Depth", "Pan sweep depth", ParamValueKind::Number, &StereoSweepDelayEffect::kPanDepthRange, nullptr},
    {6, "Pan Rate", "Pan rate (Hz)", ParamValueKind::Number, &StereoSweepDelayEffect::kPanRateRange, nullptr},
};

const EffectMeta StereoSweepDelayEffect::kMeta = {"Stereo Sweep", "Panning delay.", StereoSweepDelayEffect::kParams, 7};

// ---- DistortionEffect ----
const NumberParamRange OverdriveEffect::kDriveRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange OverdriveEffect::kToneRange = {0.0f, 1.0f, 0.01f};

const ParamInfo OverdriveEffect::kParams[2] = {
    {0, "Drive", "Overdrive amount", ParamValueKind::Number, &OverdriveEffect::kDriveRange, nullptr},
    {1, "Tone", "Dark to bright", ParamValueKind::Number, &OverdriveEffect::kToneRange, nullptr},
};

const EffectMeta OverdriveEffect::kMeta = {"Overdrive", "Musical overdrive with auto-leveling.", OverdriveEffect::kParams, 2};

// ---- StereoMixerEffect ----
const NumberParamRange StereoMixerEffect::kMixRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange StereoMixerEffect::kCrossRange = {0.0f, 1.0f, 0.01f};

const ParamInfo StereoMixerEffect::kParams[3] = {
    {0, "Mix A", "Input A level", ParamValueKind::Number, &StereoMixerEffect::kMixRange, nullptr},
    {1, "Mix B", "Input B level", ParamValueKind::Number, &StereoMixerEffect::kMixRange, nullptr},
    {2, "Cross", "Crossfade", ParamValueKind::Number, &StereoMixerEffect::kCrossRange, nullptr},
};

const EffectMeta StereoMixerEffect::kMeta = {"Stereo Mixer", "Mix two inputs.", StereoMixerEffect::kParams, 3};

// ---- SimpleReverbEffect ----
const NumberParamRange SimpleReverbEffect::kMixRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange SimpleReverbEffect::kDecayRange = {0.2f, 0.95f, 0.01f};
const NumberParamRange SimpleReverbEffect::kDampRange = {0.0f, 0.8f, 0.01f};
const NumberParamRange SimpleReverbEffect::kPreRange = {0.0f, 80.0f, 1.0f};
const NumberParamRange SimpleReverbEffect::kSizeRange = {0.0f, 1.0f, 0.01f};

const ParamInfo SimpleReverbEffect::kParams[5] = {
    {0, "Mix", "Wet/dry mix", ParamValueKind::Number, &SimpleReverbEffect::kMixRange, nullptr},
    {1, "Decay", "Reverb decay", ParamValueKind::Number, &SimpleReverbEffect::kDecayRange, nullptr},
    {2, "Damp", "High-freq damping", ParamValueKind::Number, &SimpleReverbEffect::kDampRange, nullptr},
    {3, "Pre-delay", "Pre-delay (ms)", ParamValueKind::Number, &SimpleReverbEffect::kPreRange, nullptr},
    {4, "Size", "Room size", ParamValueKind::Number, &SimpleReverbEffect::kSizeRange, nullptr},
};

const EffectMeta SimpleReverbEffect::kMeta = {"Reverb", "Simple reverb.", SimpleReverbEffect::kParams, 5};

// ---- CompressorEffect ----
const NumberParamRange CompressorEffect::kThreshRange = {-40.0f, 0.0f, 1.0f};
const NumberParamRange CompressorEffect::kRatioRange = {1.0f, 20.0f, 0.5f};
const NumberParamRange CompressorEffect::kAttackRange = {0.1f, 100.0f, 0.1f};
const NumberParamRange CompressorEffect::kReleaseRange = {10.0f, 1000.0f, 1.0f};
const NumberParamRange CompressorEffect::kMakeupRange = {0.0f, 24.0f, 0.5f};

const ParamInfo CompressorEffect::kParams[5] = {
    {0, "Threshold", "Threshold (dB)", ParamValueKind::Number, &CompressorEffect::kThreshRange, nullptr},
    {1, "Ratio", "Compression ratio", ParamValueKind::Number, &CompressorEffect::kRatioRange, nullptr},
    {2, "Attack", "Attack time (ms)", ParamValueKind::Number, &CompressorEffect::kAttackRange, nullptr},
    {3, "Release", "Release time (ms)", ParamValueKind::Number, &CompressorEffect::kReleaseRange, nullptr},
    {4, "Makeup", "Makeup gain (dB)", ParamValueKind::Number, &CompressorEffect::kMakeupRange, nullptr},
};

const EffectMeta CompressorEffect::kMeta = {"Compressor", "Dynamics compressor.", CompressorEffect::kParams, 5};

// ---- ChorusEffect ----
const NumberParamRange ChorusEffect::kRateRange = {0.1f, 2.0f, 0.01f};
const NumberParamRange ChorusEffect::kDepthRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange ChorusEffect::kFeedbackRange = {0.0f, 1.0f, 0.01f};
const NumberParamRange ChorusEffect::kDelayRange = {5.0f, 25.0f, 0.1f};
const NumberParamRange ChorusEffect::kMixRange = {0.0f, 1.0f, 0.01f};

const ParamInfo ChorusEffect::kParams[5] = {
    {0, "Rate", "LFO rate (Hz)", ParamValueKind::Number, &ChorusEffect::kRateRange, nullptr},
    {1, "Depth", "Modulation depth", ParamValueKind::Number, &ChorusEffect::kDepthRange, nullptr},
    {2, "Feedback", "Feedback amount", ParamValueKind::Number, &ChorusEffect::kFeedbackRange, nullptr},
    {3, "Delay", "Base delay (ms)", ParamValueKind::Number, &ChorusEffect::kDelayRange, nullptr},
    {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &ChorusEffect::kMixRange, nullptr},
};

const EffectMeta ChorusEffect::kMeta = {"Chorus", "Stereo chorus effect.", ChorusEffect::kParams, 5};
