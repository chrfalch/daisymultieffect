
#include "effects/effect_registry.h"
#include "effects/effect_metadata.h"

const EffectMeta *EffectRegistry::Lookup(uint8_t typeId)
{
    return Effects::findByTypeId(typeId);
}
