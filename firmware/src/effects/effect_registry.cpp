
#include "effects/effect_registry.h"
#include "effects/delay.h"
#include "effects/stereo_sweep_delay.h"
#include "effects/distortion.h"
#include "effects/stereo_mixer.h"
#include "effects/reverb.h"
#include "effects/compressor.h"
#include "effects/chorus.h"

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
