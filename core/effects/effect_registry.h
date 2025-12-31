
#pragma once
#include "core/effects/base_effect.h"

struct EffectRegistry
{
    static const EffectMeta *Lookup(uint8_t typeId);
};
