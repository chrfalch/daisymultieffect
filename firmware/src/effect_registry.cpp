
#include "effect_registry.h"
#include "effects_delay.h"
#include "effects_stereo_sweep_delay.h"
#include "effects_distortion.h"
#include "effects_stereo_mixer.h"
#include "effects_reverb.h"

const EffectMeta* EffectRegistry::Lookup(uint8_t typeId)
{
    switch(typeId)
    {
        case DelayEffect::TypeId: return &DelayEffect::kMeta;
        case StereoSweepDelayEffect::TypeId: return &StereoSweepDelayEffect::kMeta;
        case DistortionEffect::TypeId: return &DistortionEffect::kMeta;
        case StereoMixerEffect::TypeId: return &StereoMixerEffect::kMeta;
        case SimpleReverbEffect::TypeId: return &SimpleReverbEffect::kMeta;
        default: return nullptr;
    }
}
