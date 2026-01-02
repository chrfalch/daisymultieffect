# Adding a New Effect

This guide explains how to add a new audio effect to the DaisyMultiFX project.

## Overview

Effects are defined once in `core/effects/` and shared between the firmware (Daisy Seed) and the VST plugin. This ensures identical DSP behavior on both platforms.

**Important**: All effect metadata (names, parameters, ranges) is defined in `core/effects/effect_metadata.h`. Effect classes reference this metadata - they don't define their own.

## Steps

### 1. Add Effect Metadata

Edit `core/effects/effect_metadata.h` and add a new namespace:

```cpp
//=========================================================================
// My Effect
//=========================================================================
namespace MyEffect
{
    constexpr uint8_t TypeId = 19;  // Choose next available ID
    inline const NumberParamRange kParam1Range = {0.0f, 1.0f, 0.01f};
    inline const NumberParamRange kParam2Range = {-12.0f, 12.0f, 0.5f};
    inline const ParamInfo kParams[] = {
        {0, "Param1", "Description", ParamValueKind::Number, &kParam1Range, nullptr},
        {1, "Param2", "Description", ParamValueKind::Number, &kParam2Range, nullptr},
    };
    inline const ::EffectMeta kMeta = {"My Effect", "Short description.", kParams, 2};
}
```

Then add to the `kAllEffects` array:
```cpp
inline const EffectEntry kAllEffects[] = {
    // ... existing effects ...
    {MyEffect::TypeId, &MyEffect::kMeta},
};
```

### 2. Create the Effect Header

Create a new file in `core/effects/` (e.g., `my_effect.h`):

```cpp
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>

struct MyEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::MyEffect::TypeId;

    // Parameters (normalized 0..1 internally)
    float param1_ = 0.5f;
    float param2_ = 0.5f;

    // Internal state
    float sampleRate_ = 48000.0f;

    // Metadata from effect_metadata.h
    const EffectMeta &GetMetadata() const override { return Effects::MyEffect::kMeta; }
    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        // Reset state here
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: param1_ = v; break;
        case 1: param2_ = v; break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 2) return 0;
        out[0] = {0, (uint8_t)(param1_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(param2_ * 127.0f + 0.5f)};
        return 2;
    }

    void ProcessStereo(float &l, float &r) override
    {
        // Your DSP code here
    }
};
```

### 3. Update Audio Processor

**In `core/audio/audio_processor.h`:**
- Add include: `#include "effects/my_effect.h"`
- Add pool constant: `static constexpr int kMaxMyEffects = 4;`
- Add pool array: `MyEffect fx_myeffects_[kMaxMyEffects];`
- Add counter: `int myeffect_next_ = 0;`

**In `core/audio/audio_processor.cpp`:**
- Add to constructor initializer list: `fx_myeffects_{},` and `myeffect_next_(0)`
- Add case to `Instantiate()`:
```cpp
case MyEffect::TypeId:
    if (myeffect_next_ < kMaxMyEffects)
        return &fx_myeffects_[myeffect_next_++];
    return nullptr;
```
- Add counter reset in `ApplyPatch()`: `myeffect_next_ = 0;`

### 4. Build and Test

```bash
# Build VST
cd vst/build && cmake --build . --target DaisyMultiFX_Standalone

# Build firmware
cd firmware && make
```

## Type ID Conventions

| Range | Purpose |
|-------|---------|
| 0 | Off/Bypass |
| 1-9 | Time-based (Delay, etc.) |
| 10-11 | Distortion/Overdrive |
| 12-13 | Modulation delays, Mixer |
| 14-16 | Reverb, Compressor, Chorus |
| 17 | Noise Gate |
| 18 | Graphic EQ |
| 19 | Flanger |
| 20+ | New effects |

## Tips

- Use `std::fabs()` instead of `abs()` for floats
- Include `<algorithm>` for `std::max/min`
- Parameters are 0-1 normalized; convert in `SetParam()`
- Keep DSP simple - this runs at 48kHz per sample
- Test in VST first (easier debugging than firmware)
- For static const arrays, use static getter functions to avoid C++14/17 linkage issues
