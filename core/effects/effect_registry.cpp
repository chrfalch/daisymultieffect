
#include "core/effects/effect_registry.h"
#include "core/effects/delay.h"
#include "core/effects/stereo_sweep_delay.h"
#include "core/effects/distortion.h"
#include "core/effects/stereo_mixer.h"
#include "core/effects/reverb.h"
#include "core/effects/compressor.h"
#include "core/effects/chorus.h"

const EffectMeta *EffectRegistry::Lookup(uint8_t typeId)
{
    switch (typeId)
    {
    case DelayEffect::TypeId:
        return &DelayEffect::kMeta;
    case StereoSweepDelayEffect::TypeId:
        return &StereoSweepDelayEffect::kMeta;
    case DistortionEffect::TypeId:
        return &DistortionEffect::kMeta;
    case StereoMixerEffect::TypeId:
        return &StereoMixerEffect::kMeta;
    case SimpleReverbEffect::TypeId:
        return &SimpleReverbEffect::kMeta;
    case CompressorEffect::TypeId:
        return &CompressorEffect::kMeta;
    case ChorusEffect::TypeId:
        return &ChorusEffect::kMeta;
    default:
        return nullptr;
    }
}
