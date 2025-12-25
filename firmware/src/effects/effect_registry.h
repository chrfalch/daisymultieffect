
#pragma once
#include "effects/base_effect.h"
struct EffectRegistry
{
    static const EffectMeta *Lookup(uint8_t typeId);
};
