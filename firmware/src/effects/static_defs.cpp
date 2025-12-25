#include "effects/delay.h"
#include "effects/distortion.h"
#include "effects/reverb.h"
#include "effects/stereo_mixer.h"
#include "effects/stereo_sweep_delay.h"
#include "effects/compressor.h"
#include "effects/chorus.h"

#include "dev/sdram.h"

// ---- Per-instance buffer pools (allocated in SDRAM) ----
// Pool sizes match AudioEngine::kMax* constants.

// DelayEffect: 2 instances x 2 channels x MAX_SAMPLES
static constexpr int kMaxDelays = 2;
DSY_SDRAM_BSS float g_delayBufL[kMaxDelays][DelayEffect::MAX_SAMPLES];
DSY_SDRAM_BSS float g_delayBufR[kMaxDelays][DelayEffect::MAX_SAMPLES];

// StereoSweepDelayEffect: 2 instances x 2 channels x MAX_SAMPLES
static constexpr int kMaxSweeps = 2;
DSY_SDRAM_BSS float g_sweepBufL[kMaxSweeps][StereoSweepDelayEffect::MAX_SAMPLES];
DSY_SDRAM_BSS float g_sweepBufR[kMaxSweeps][StereoSweepDelayEffect::MAX_SAMPLES];

// SimpleReverbEffect: 2 instances with pre-delay + comb + allpass buffers
static constexpr int kMaxReverbs = 2;
DSY_SDRAM_BSS float g_reverbPre[kMaxReverbs][SimpleReverbEffect::MAX_PRE];
DSY_SDRAM_BSS float g_reverbComb[kMaxReverbs][4][SimpleReverbEffect::Comb::MAX_DELAY];
DSY_SDRAM_BSS float g_reverbAp[kMaxReverbs][2][SimpleReverbEffect::Allpass::MAX_DELAY];

// ---- Binding helpers (called from AudioEngine constructor) ----
void BindDelayBuffers(DelayEffect *delays, int count)
{
    for (int i = 0; i < count && i < kMaxDelays; i++)
        delays[i].BindBuffers(g_delayBufL[i], g_delayBufR[i]);
}

void BindSweepBuffers(StereoSweepDelayEffect *sweeps, int count)
{
    for (int i = 0; i < count && i < kMaxSweeps; i++)
        sweeps[i].BindBuffers(g_sweepBufL[i], g_sweepBufR[i]);
}

void BindReverbBuffers(SimpleReverbEffect *reverbs, int count)
{
    for (int i = 0; i < count && i < kMaxReverbs; i++)
    {
        float *combPtrs[4] = {g_reverbComb[i][0], g_reverbComb[i][1], g_reverbComb[i][2], g_reverbComb[i][3]};
        float *apPtrs[2] = {g_reverbAp[i][0], g_reverbAp[i][1]};
        reverbs[i].BindBuffers(g_reverbPre[i], combPtrs, apPtrs);
    }
}

// ---- DelayEffect ----
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

// ---- CompressorEffect ----
const NumberParamRange CompressorEffect::kThreshRange = {-40.0f, 0.0f, 0.5f};
const NumberParamRange CompressorEffect::kRatioRange = {1.0f, 20.0f, 0.1f};
const NumberParamRange CompressorEffect::kAttackRange = {0.1f, 100.0f, 0.1f};
const NumberParamRange CompressorEffect::kReleaseRange = {10.0f, 1000.0f, 1.0f};
const NumberParamRange CompressorEffect::kMakeupRange = {0.0f, 24.0f, 0.1f};

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
const NumberParamRange ChorusEffect::kFeedbackRange = {0.0f, 0.9f, 0.01f};
const NumberParamRange ChorusEffect::kDelayRange = {5.0f, 25.0f, 0.1f};
const NumberParamRange ChorusEffect::kMixRange = {0.0f, 1.0f, 0.01f};

const ParamInfo ChorusEffect::kParams[5] = {
    {0, "Rate", "LFO rate (Hz)", ParamValueKind::Number, &ChorusEffect::kRateRange, nullptr},
    {1, "Depth", "Modulation depth", ParamValueKind::Number, &ChorusEffect::kDepthRange, nullptr},
    {2, "Feedback", "Feedback amount", ParamValueKind::Number, &ChorusEffect::kFeedbackRange, nullptr},
    {3, "Delay", "Base delay (ms)", ParamValueKind::Number, &ChorusEffect::kDelayRange, nullptr},
    {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &ChorusEffect::kMixRange, nullptr},
};

const EffectMeta ChorusEffect::kMeta = {"Chorus", "Classic chorus modulation.", ChorusEffect::kParams, 5};
